# P8A — Learned turn-based enemy: verification record

## Record

- Date: 2026-11-07
- Commits: `issue #UMBRA-53` (P8A.1–P8A.4: contract, environment, network, model files,
  training tools, accepted model) and `issue #UMBRA-54` (P8A.5–P8A.6: game integration, tool
  check, editor field, workflow, records)
- Tester: Claude (remote session). Training and evaluation ran in the cloud workspace (Linux,
  2 CPU cores, no GPU).
- State: **implemented. One model accepted under thresholds declared before the final runs.
  The recipe produced an acceptable model for 1 of 2 seeds. In-game behavior on Windows not run
  yet.**

User decisions (2026-11-07): P8A before P8B; a tiny built-in C++ network (no ONNX Runtime);
training in Claude's workspace on CPU.

## 1. Automated checks (Linux)

| Configuration | Result |
|---|---|
| GCC 13.3 / Clang 18 / GCC ASan+UBSan | Build OK, 0 new warnings, 35/35 CTest entries pass |

New entries:

- `Umbra.Policy`: contract, environment, masks, network, model files, decider, parity on
  120 cases, 200 native fights.
- `Umbra.PolicyDependencies`: turn-only, no Python or ML runtime.
- `Umbra.Tool.PolicyCheck`: the shipped model fits the shipped content.

## 2. Training and evaluation

**Scenario:**

- The bandit leader (now strike, heavy blow, guard, weaken, 2 health potions) against the hero
  at level 5.
- The hero plays the conventional AI, plus 15% random moves during training.
- Varied starting health on both sides.

**Recipe:**

- PPO with action masks, 3 M decisions, 16 environments, 2×64 tanh network.
- Preference bonus 1.0 for attacking when the player is below 35% health. Control = the same
  recipe with bonus 0.
- About 3 minutes per model at about 65,000 environment steps/s through the C++ bridge.

**Evaluation:**

- 4,000 held-out fights each (seeds ≥ 10,000,000; training used < 5,000,000).
- Plain conventional opponent.
- The engine's C++ network makes every decision.

**Pilot (before thresholds were fixed), seeds 100/200:**

- Bonus 0.15: no preference (gap −0.09).
- Bonus 0.4: gap +0.12.
- Bonus 1.0: gap +0.21.
- Every model won exactly as often as the conventional AI (see "Competence" below).

**Acceptance thresholds** (`tools/policy/acceptance.json`, declared after the pilot, before the
final runs):

| Check | Threshold |
|---|---|
| Win rate vs the conventional AI | ≥ conventional − 2 points |
| Preference gap (matched probes) | ≥ 0.15 |
| Gap minus the control's gap | ≥ 0.15 |
| Illegal actions, fallback rate | 0, 0 |
| C++ vs NumPy parity | ≤ 1e-4 |

**Final runs:**

| | Seed 301 (shipped) | Seed 300 |
|---|---|---|
| Win rate: learned / conventional / control / random | 25.3% / 25.7% / 25.7% / 1.4% | 25.7% / 25.7% / 25.7% / 1.4% |
| Matched probes: attack at player 20% / 80% health | 75.4% / 50.4% | 62.7% / 63.4% |
| Preference gap (control on the same states) | **+0.25** (−0.13) | −0.01 (−0.04) |
| Real play: attack rate with the player below / above 35% | 74.3% / 68.4% (control 76.1% / 67.4%) | 64.2% / 72.2% |
| Heals when nearly dead against a healthy player | 100% | 100% |
| Attacks when both are nearly dead | 0% (survival wins) | 0% |
| Illegal actions / fallbacks | 0 / 0 | 0 / 0 |
| Parity (largest C++ vs NumPy difference) | 6.6e-6 | 6.0e-6 |
| Verdict | **accepted** | **refused** (preference, fromTraining) |

Full reports: `docs/testing/p8a/final-report-301.json`, `final-report-300.json`.
`approve.py` refused seed 300 and approved seed 301 into
`assets/policies/bandit_leader.policy.json`. The evaluation summary and thresholds are stored
inside the model.

## 3. What the results mean, and what they don't

- **Competence.** No model, learned or control, beats the conventional AI. With this content,
  only the totals decide a fight (three heavy blows, two potions). Every model that spends those
  resources wins the same fights, in a different order. A hand-written "weaken first" strategy
  did worse (9% vs 27%) because a non-attacking turn costs more damage than it saves, even with
  weaken boosted to −50%. The learned enemy is therefore **as strong as** the conventional one,
  not stronger. A content design that rewards tactics would be needed to show more.
- **Preference.** In the shipped model, attacking a low-health player is a measurable learned
  tendency. Changing only the player's health in otherwise identical situations moves the
  attack choice by 25 points, while the control moves the other way. It is not a rule:
  - nearly dead against a healthy player, it always heals;
  - when both are nearly dead, survival still wins (0% attack).
  - The code has no health-threshold branch (T41: the only threshold is in the training
    reward, `train.py`).
- **Real play vs probes.** In real fights the preference shows only weakly (74% vs 68%). Low
  player health happens late in fights, when the boss's energy is gone and its options differ.
  The matched probes remove that confound. The real-play figures are reported so it isn't
  hidden.
- **Robustness.** The same recipe gave the preference for one seed and not the other. The
  shipped model passed the declared tests on its own held-out evaluation. The recipe itself is
  **not** shown to be reliable. Next steps: more seeds, a stronger or annealed bonus, or more
  training. This stays open (T54).

## 4. Roadmap test cases

| Case | Evidence | Status |
|---|---|---|
| T19 decision after load/restart | Answers carry the decision id; stale ids are dropped (`PolicyTests`, decider); a world change mid-fight already ends it (P6) | Passed |
| T23 reset leaks state | Same seed + actions → identical fight after other episodes (`PolicyTests`) | Passed |
| T24 timeout vs terminal | `done` (winner) and `truncated` (round 30) are separate; truncation counts as neither | Passed |
| T25 all-invalid / non-finite | `Choose` returns −1 → conventional AI; tested | Passed |
| T26/T27 order or meaning changed | Observation version, catalogue and fingerprint checked; changed skill power refused (tests, `UmbraTool policy-check`) | Passed |
| T28 late model | Slow-decider test: nothing before the deadline, late answer never used for the next decision | Passed |
| T29 export/native disagreement | 120 parity cases ≤ 1e-4 (6.6e-6 measured) and same legal choice; 200 complete native fights | Passed |
| T30 reward exploit | Held-out fights, control model, counterexample probes; mechanism reported (resource totals decide fights) | Passed (reported) |
| T31 inference starves the game | 3.9 µs per decision (including the Python call), one worker thread, one job at a time | Passed (measured on Linux) |
| T41 forced branch | Inspection (no threshold in game/game code) + paired probes + control | Passed |
| T47 legal in snapshot, illegal at commit | The answer is applied through `Submit` (shared rules) and also checked against the snapshot mask; otherwise fallback | Passed by design |
| T52 overload / shutdown | One job; busy refuses; destructor joins mid-job (tested) | Passed |
| T54 leakage / cherry-picking | Separate seed ranges; opponent's bag hidden (tested); 2 final seeds reported, including the refused one | Partly: recipe reliability open |
| T55 training rules drift | Training steps call the real `TurnEncounter` through the C bridge | Passed |
| T59 untrusted model package | JSON only, 16 MiB and 1,024-width limits, shape/finite checks, no code execution | Passed |
| T60 decision timing mismatch | The learner acts only on its turns; the opponent plays until the learner's turn again; the game uses the same observation and mask per enemy turn | Passed by design |

## 5. Windows checklist (open)

| # | Do | Expect | Result |
|---|---|---|---|
| 1 | `scripts\build-and-run.bat` | 36 CTest entries pass (35 + the Windows-exception test) | |
| 2 | Turn mode, fight the bandit leader (purple bandit, dungeon end) | Status line shows "(learned AI)"; it heals when low, uses heavy blows; no "gave no usable answer" lines | |
| 3 | Edit `heavy_blow` power in the editor, Ctrl+S with the game running | "Learned AI for Bandit Leader refused (… retrain); standard AI plays it" | |
| 4 | Optional: train on your PC (`tools/policy/README.md`) | Same recipe runs; approve refuses a failing model | |
