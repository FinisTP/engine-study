"""Trains a learned turn-based enemy with PPO and writes an umbra.policy model file.

Developer-only (Python 3, numpy, PyTorch, CPU is enough). The fights run in the engine's C++
rules through UmbraPolicyEnv. The method, the reward and the preference term are explained in
tools/policy/README.md.

    python train.py --library <UmbraPolicyEnv> --content ../../projects/reference/assets/rpg/reference.rpg.json \
                    --out bandit_leader.policy.json --preference 0.15 --seed 1
"""
import argparse
import json
import platform
import time

import numpy as np
import torch
import torch.nn as nn

from umbra_env import FightEnv, load_library

LOW_HEALTH = 0.35  # "the player is low": below this share of maximum health
OPPONENT_HEALTH = 6  # index of opponent.health in the observation (game/policy PolicyContract.hpp)


def mlp(inputs, outputs, hidden):
    return nn.Sequential(nn.Linear(inputs, hidden), nn.Tanh(), nn.Linear(hidden, hidden), nn.Tanh(), nn.Linear(hidden, outputs))


def masked_logits(logits, mask):
    return logits.masked_fill(mask == 0, -1e9)


def export(policy, env, args, stats, path):
    """The policy network as the engine reads it: row-major weights, tanh hidden layers, linear output."""
    linears = [m for m in policy if isinstance(m, nn.Linear)]
    layers = []
    for k, layer in enumerate(linears):
        layers.append({
            "in": layer.in_features, "out": layer.out_features,
            "activation": "linear" if k == len(linears) - 1 else "tanh",
            "weights": [round(float(w), 7) for w in layer.weight.detach().numpy().reshape(-1)],
            "bias": [round(float(b), 7) for b in layer.bias.detach().numpy()],
        })
    actions = [{"item": n[5:]} if n.startswith("item:") else ({} if n == "wait" else {"skill": n}) for n in env.names]
    model = {
        "format": "umbra.policy", "version": 1,
        "learner": args.learner, "opponent": args.opponent,
        "observation": env.lib.umbra_observation_version().decode(), "fingerprint": env.fingerprint,
        "actions": actions, "layers": layers,
        "training": {
            "method": "PPO with action masks", "seed": args.seed, "steps": args.steps, "envs": args.envs,
            "preference": args.preference, "lowHealth": LOW_HEALTH, "opponentLevel": args.opponent_level,
            "opponentRandomness": args.randomness, "hidden": args.hidden, "seconds": round(stats["seconds"], 1),
            "torch": torch.__version__, "python": platform.python_version(), "machine": platform.machine(),
            "finalWinRate": round(stats["win_rate"], 3),
        },
    }
    with open(path, "w") as f:
        json.dump(model, f, indent=1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--library")
    ap.add_argument("--content", default="../../projects/reference/assets/rpg/reference.rpg.json")
    ap.add_argument("--learner", default="bandit_leader")
    ap.add_argument("--opponent", default="hero")
    ap.add_argument("--opponent-level", type=int, default=5)
    ap.add_argument("--randomness", type=float, default=0.15)
    ap.add_argument("--preference", type=float, default=0.15, help="bonus for attacking a low-health opponent (0 = control model)")
    ap.add_argument("--steps", type=int, default=600_000)
    ap.add_argument("--envs", type=int, default=16)
    ap.add_argument("--rollout", type=int, default=128)
    ap.add_argument("--hidden", type=int, default=64)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    # 1. Reproducible: one seed drives torch, numpy and every fight's seed.
    torch.manual_seed(args.seed)
    rng = np.random.default_rng(args.seed)
    torch.set_num_threads(1)
    lib = load_library(args.library)
    envs = [FightEnv(lib, args.content, args.learner, args.opponent, args.randomness, True, 30, args.opponent_level) for _ in range(args.envs)]
    n_obs, n_act = envs[0].obs_size, envs[0].actions
    attack = torch.tensor(envs[0].attack)
    policy, value = mlp(n_obs, n_act, args.hidden), mlp(n_obs, 1, args.hidden)
    optimizer = torch.optim.Adam(list(policy.parameters()) + list(value.parameters()), lr=3e-4)
    train_seeds = iter(rng.permutation(5_000_000))  # held-out evaluation uses seeds >= 10,000,000

    state = [e.reset(int(next(train_seeds))) for e in envs]
    gamma, lam, clip = 0.99, 0.95, 0.2
    start, steps, update = time.time(), 0, 0
    recent = []
    while steps < args.steps:
        # 2. Rollout: every env plays `rollout` learner decisions.
        buf = {k: [] for k in ("obs", "mask", "act", "logp", "rew", "done", "val")}
        for _ in range(args.rollout):
            obs = torch.tensor(np.stack([s[0] for s in state]))
            mask = torch.tensor(np.stack([s[1] for s in state]))
            with torch.no_grad():
                dist = torch.distributions.Categorical(logits=masked_logits(policy(obs), mask))
                act = dist.sample()
                logp, val = dist.log_prob(act), value(obs).squeeze(-1)
            rewards, dones = [], []
            for k, e in enumerate(envs):
                r, d, t, w = e.step(int(act[k]))
                # The preference: a small bonus for attacking when the opponent is low. A wish, not a rule:
                # the policy may still heal or guard when that wins more (README, "Preference").
                if attack[act[k]] and obs[k, OPPONENT_HEALTH] < LOW_HEALTH:
                    r += args.preference
                rewards.append(r)
                dones.append(d or t)
                if d or t:
                    recent.append(1.0 if (d and w == 1) else 0.0)
                    state[k] = e.reset(int(next(train_seeds)))
                else:
                    state[k] = e.observe()
            for key, v in (("obs", obs), ("mask", mask), ("act", act), ("logp", logp), ("val", val)):
                buf[key].append(v)
            buf["rew"].append(torch.tensor(rewards, dtype=torch.float32))
            buf["done"].append(torch.tensor(dones, dtype=torch.float32))
        steps += args.rollout * args.envs

        # 3. Advantages (GAE) and returns.
        with torch.no_grad():
            next_val = value(torch.tensor(np.stack([s[0] for s in state]))).squeeze(-1)
        adv, last = [], torch.zeros(args.envs)
        for t in reversed(range(args.rollout)):
            nv = next_val if t == args.rollout - 1 else buf["val"][t + 1]
            delta = buf["rew"][t] + gamma * nv * (1 - buf["done"][t]) - buf["val"][t]
            last = delta + gamma * lam * (1 - buf["done"][t]) * last
            adv.insert(0, last)
        adv = torch.stack(adv).reshape(-1)
        ret = adv + torch.stack(buf["val"]).reshape(-1)
        adv = (adv - adv.mean()) / (adv.std() + 1e-8)
        obs, mask = torch.cat(buf["obs"]), torch.cat(buf["mask"])
        act, old_logp = torch.cat(buf["act"]), torch.cat(buf["logp"])

        # 4. PPO update: clipped policy loss, value loss, a little entropy.
        for _ in range(4):
            for idx in torch.randperm(len(act)).split(512):
                dist = torch.distributions.Categorical(logits=masked_logits(policy(obs[idx]), mask[idx]))
                ratio = torch.exp(dist.log_prob(act[idx]) - old_logp[idx])
                loss_pi = -torch.min(ratio * adv[idx], torch.clamp(ratio, 1 - clip, 1 + clip) * adv[idx]).mean()
                loss_v = ((value(obs[idx]).squeeze(-1) - ret[idx]) ** 2).mean()
                loss = loss_pi + 0.5 * loss_v - 0.01 * dist.entropy().mean()
                optimizer.zero_grad()
                loss.backward()
                nn.utils.clip_grad_norm_(list(policy.parameters()) + list(value.parameters()), 0.5)
                optimizer.step()
        update += 1
        if update % 10 == 0:
            recent = recent[-2000:]
            print(f"steps {steps:>8}  win rate (last 2000 fights, sampling) {np.mean(recent):.3f}  {time.time() - start:.0f}s", flush=True)

    recent = recent[-2000:]
    export(policy, envs[0], args, {"seconds": time.time() - start, "win_rate": float(np.mean(recent))}, args.out)
    print(f"wrote {args.out}")
    for e in envs:
        e.close()


if __name__ == "__main__":
    main()
