"""Evaluates umbra.policy models on held-out fights and checks them against declared thresholds.

Everything a shipped model does in the game is measured here with the engine's own C++ network
code (UmbraPolicyEnv's umbra_model_*), not with PyTorch. Report sections are explained in
tools/policy/README.md ("Evaluation").

    python evaluate.py --library <UmbraPolicyEnv> --model m.json --control c.json \
                       --acceptance acceptance.json --report report.json
"""
import argparse
import ctypes
import json

import numpy as np

from umbra_env import FightEnv, load_library

HELD_OUT = 10_000_000  # training never uses seeds this large
LOW, HIGH = 0.2, 0.8   # matched probes: opponent health set to these, everything else unchanged
OPPONENT_HEALTH, SELF_HEALTH = 6, 0


def numpy_forward(model, obs):
    """Reference forward pass from the file alone (for the parity check)."""
    x = np.asarray(obs, dtype=np.float64)
    for layer in model["layers"]:
        w = np.asarray(layer["weights"], dtype=np.float64).reshape(layer["out"], layer["in"])
        x = w @ x + np.asarray(layer["bias"], dtype=np.float64)
        if layer["activation"] == "tanh":
            x = np.tanh(x)
    return x


class Native:
    """A model loaded by the engine's C++ code (also checks it fits the content)."""

    def __init__(self, lib, path, env):
        self.lib, self.handle = lib, lib.umbra_model_load(path.encode(), env.handle)
        if not self.handle:
            raise RuntimeError(f"{path}: the engine refused the model (see stderr)")
        self.n = env.actions
        self._out = (ctypes.c_float * self.n)()

    def choose(self, obs, mask):
        o = (ctypes.c_float * len(obs))(*obs)
        m = (ctypes.c_ubyte * len(mask))(*mask)
        return self.lib.umbra_model_choose(self.handle, o, m, self.n)

    def logits(self, obs):
        o = (ctypes.c_float * len(obs))(*obs)
        self.lib.umbra_model_logits(self.handle, o, self._out)
        return np.frombuffer(self._out, dtype=np.float32).copy()


def play(env, chooser, episodes, record=None):
    """Held-out fights. chooser(obs, mask) -> slot (-1 = fallback to the conventional AI)."""
    wins, rounds, fallbacks, decisions = 0, [], 0, 0
    for ep in range(episodes):
        obs, mask = env.reset(HELD_OUT + ep)
        turns = 0
        while True:
            a = chooser(obs, mask)
            if a < 0:
                fallbacks += 1
                a = env.conventional()
            if record is not None:
                record.append((obs, mask, a))
            decisions += 1
            turns += 1
            r, d, t, w = env.step(a)
            if d or t:
                wins += int(d and w == 1)
                rounds.append(turns)
                break
            obs, mask = env.observe()
    return {"winRate": wins / episodes, "meanTurns": float(np.mean(rounds)), "fallbackRate": fallbacks / max(decisions, 1),
            "illegal": env.illegal()}


def attack_rates(env, decisions):
    """In real play: how often an attack was chosen, when one was legal, by opponent health."""
    low = [env.attack[a] for o, m, a in decisions if o[OPPONENT_HEALTH] < 0.35 and m[env.attack].any()]
    high = [env.attack[a] for o, m, a in decisions if o[OPPONENT_HEALTH] >= 0.35 and m[env.attack].any()]
    return {"lowHealth": float(np.mean(low)) if low else None, "highHealth": float(np.mean(high)) if high else None,
            "lowDecisions": len(low), "highDecisions": len(high)}


def probes(env, native, states):
    """Matched probes: the same decision state with only the opponent's health changed (T30)."""
    at_low, at_high, heal_when_dying, attack_when_both_low = [], [], [], []
    for obs, mask, _ in states:
        if not mask[env.attack].any() or mask[~env.attack].sum() <= 1:
            continue  # no real choice between attacking and something else
        lo, hi = obs.copy(), obs.copy()
        lo[OPPONENT_HEALTH], hi[OPPONENT_HEALTH] = LOW, HIGH
        at_low.append(env.attack[native.choose(lo, mask)])
        at_high.append(env.attack[native.choose(hi, mask)])
    # Counterexample: nearly dead with a potion. Does it still prefer attacking a healthy opponent?
    potion = env.names.index("item:health_potion") if "item:health_potion" in env.names else -1
    for obs, mask, _ in states:
        if potion >= 0 and mask[potion]:
            o = obs.copy()
            o[SELF_HEALTH], o[OPPONENT_HEALTH] = 0.15, HIGH
            heal_when_dying.append(native.choose(o, mask) == potion)
            # The conflict case: nearly dead, but so is the opponent. Preference and survival disagree.
            o[OPPONENT_HEALTH] = LOW
            attack_when_both_low.append(env.attack[native.choose(o, mask)])
    return {"attackAtLow": float(np.mean(at_low)), "attackAtHigh": float(np.mean(at_high)),
            "gap": float(np.mean(at_low) - np.mean(at_high)), "pairs": len(at_low),
            "healsWhenNearlyDeadVsHealthyOpponent": float(np.mean(heal_when_dying)) if heal_when_dying else None,
            "attacksWhenBothNearlyDead": float(np.mean(attack_when_both_low)) if attack_when_both_low else None}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--library")
    ap.add_argument("--content", default="../../projects/reference/assets/rpg/reference.rpg.json")
    ap.add_argument("--model", required=True)
    ap.add_argument("--control", help="same recipe trained with --preference 0")
    ap.add_argument("--acceptance", required=True)
    ap.add_argument("--episodes", type=int, default=4000)
    ap.add_argument("--report", required=True)
    args = ap.parse_args()

    lib = load_library(args.library)
    model_file = json.load(open(args.model))
    t = model_file["training"]
    # Evaluation opponent: the plain conventional AI (no randomness), as in the game.
    env = FightEnv(lib, args.content, model_file["learner"], model_file["opponent"], 0.0, True, 30, t["opponentLevel"])
    native = Native(lib, args.model, env)
    rng = np.random.default_rng(0)

    # 1. Competence: the model vs the conventional enemy AI and a random one, same held-out fights.
    decisions = []
    report = {"model": args.model, "episodes": args.episodes, "heldOutSeedsFrom": HELD_OUT, "opponentLevel": t["opponentLevel"]}
    report["learned"] = play(env, native.choose, args.episodes, decisions)
    report["conventional"] = play(env, lambda o, m: env.conventional(), args.episodes)
    report["random"] = play(env, lambda o, m: int(rng.choice(np.flatnonzero(m))), args.episodes)

    # 2. Preference in real play, and in matched probes, for the model and the control.
    report["learned"]["attackRates"] = attack_rates(env, decisions)
    report["learned"]["probes"] = probes(env, native, decisions)
    if args.control:
        control = Native(lib, args.control, env)
        control_decisions = []
        report["control"] = play(env, control.choose, args.episodes, control_decisions)
        report["control"]["attackRates"] = attack_rates(env, control_decisions)
        report["control"]["probes"] = probes(env, control, decisions)  # the same states as the model

    # 3. Parity: the engine's C++ network vs a NumPy forward pass from the file.
    worst = 0.0
    for obs, mask, _ in decisions[:2000]:
        worst = max(worst, float(np.max(np.abs(native.logits(obs) - numpy_forward(model_file, obs)))))
    report["parityMaxError"] = worst

    # 4. Acceptance against thresholds declared before evaluation.
    acc = json.load(open(args.acceptance))
    checks = {
        "competence": report["learned"]["winRate"] >= report["conventional"]["winRate"] + acc["minWinRateOverConventional"],
        "preference": report["learned"]["probes"]["gap"] >= acc["minPreferenceGap"],
        "fromTraining": (not args.control) or report["learned"]["probes"]["gap"] - report["control"]["probes"]["gap"] >= acc["minGapOverControl"],
        "noIllegal": report["learned"]["illegal"] <= acc["maxIllegal"],
        "noFallback": report["learned"]["fallbackRate"] <= acc["maxFallbackRate"],
        "parity": worst <= acc["maxParityError"],
    }
    report["acceptance"] = {"thresholds": acc, "checks": checks, "accepted": all(checks.values())}
    with open(args.report, "w") as f:
        json.dump(report, f, indent=2)
    print(json.dumps({k: report[k] for k in ("learned", "conventional", "random")}, indent=1))
    if args.control:
        print("control:", json.dumps(report["control"], indent=1))
    print("parity max error:", worst)
    print("ACCEPTED" if report["acceptance"]["accepted"] else "REJECTED", checks)


if __name__ == "__main__":
    main()
