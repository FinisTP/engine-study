# Training a learned turn-based enemy

Developer-only tools. Players never need them, and the game and the editor never load them. The
game runs the finished model with plain C++ (`engine/policy/include/Policy.md`).

## What you need

- The engine built (any configuration). The bridge library is
  `UmbraPolicyEnv.dll` (Windows) / `libUmbraPolicyEnv.so` (Linux) under `tools/policy` in the
  build folder. A Release build trains several times faster.
- Python 3.10+ with `numpy` and `torch` (`pip install numpy torch`). A CPU is enough: one model
  takes about 3 minutes.
- `set UMBRA_POLICY_ENV=<path to the library>` (or pass `--library` to each script).

## The recipe (preset)

```text
cd tools/policy
python train.py    --preference 1.0 --seed 301 --out runs/pref.json      # the model
python train.py    --preference 0   --seed 301 --out runs/control.json   # the same without the preference
python evaluate.py --model runs/pref.json --control runs/control.json --acceptance acceptance.json --report runs/report.json
python approve.py  --model runs/pref.json --report runs/report.json --out ../../assets/policies/bandit_leader.policy.json
```

Defaults:

| Setting | Default |
|---|---|
| Learner / opponent | the bandit leader against the hero at level 5 |
| Opponent randomness | 15% random moves |
| Training length | 3 million decisions |
| Training environments | 16 in parallel |
| Network | two hidden layers of 64 |

Change the learner or opponent with `--learner`/`--opponent`, and the opponent's level with
`--opponent-level`.

`approve.py` copies a model into the game **only if** the report says accepted. Otherwise it
prints the failed checks and stops, so a rejected model never reaches the game. Then add
`"policy": "policies/<name>.policy.json"` to the actor in the RPG content, or pick it in the
editor's actor form under *Learned AI*. `UmbraTool policy-check <file> --content <rpg>` runs
the game's own compatibility check.

When skills, items, statuses or either actor's stats change, the model stops fitting
(fingerprint). The game then refuses it and the standard AI plays that actor. Retrain and
re-approve.

## How training works

**PPO** (proximal policy optimization) with action masks:

1. The network outputs one score per action. Illegal actions get −10⁹ before the softmax, so
   they are never sampled or learned.
2. Each update collects 128 decisions from each of the 16 fights.
3. It computes advantages with GAE (γ = 0.99, λ = 0.95).
4. It runs 4 passes of clipped updates (clip 0.2, learning rate 3·10⁻⁴, entropy bonus 0.01).
5. A separate value network estimates expected return. Only the policy network is exported.

The **reward** comes from the environment (see `Policy.md`): health swings plus ±1 for winning
or losing.

**Preference** (roadmap C18): when the player (opponent) is below 35% health, choosing an attack
earns an extra `--preference`. That is a wish, not a rule:

- Nothing forces an attack.
- The model still heals or guards whenever that wins more.
- The evaluation checks that it does (counterexample probes).

The control model is the same recipe with `--preference 0`, to show that the preference comes
from training and not from the game's dynamics.

**Seeds:** one `--seed` drives torch, numpy and the seed of every training fight (0–5,000,000).
Evaluation uses seeds from 10,000,000 upwards (held out).

## Evaluation (`evaluate.py`)

Everything is measured with the **engine's C++ network** loaded through the bridge, on held-out
fights. The evaluation opponent is the plain conventional AI with no randomness, as in the game.

| Section | Meaning |
|---|---|
| competence | Win rate of the model, the conventional enemy AI, a random enemy and the control, on the same fights |
| attack rates | In real play, how often an attack was chosen when one was legal, with the player below / above 35%. These are confounded by situation (late in a fight energy is gone). |
| matched probes | The same decision states with only the player's health set to 20% or 80%; the difference in attack choice is the **preference gap**. Same probes for the control. |
| counterexamples | Nearly dead with a potion against a healthy player (does it heal?); nearly dead against a nearly dead player (does the preference win?) |
| fallback / illegal | Decisions with no usable answer; illegal choices |
| parity | Largest difference between the C++ scores and a NumPy forward pass from the file |

`acceptance.json` holds the thresholds. They were declared before the final runs and are kept in
every approved model's `evaluation` block.
