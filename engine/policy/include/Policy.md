# Learned turn-based enemy (`Umbra::Policy`)

A learned enemy is a small neural network that chooses one action from a fixed list on each of
its turns. It goes through exactly the same `TurnEncounter::Submit` as the player and the
conventional AI, so it can only do what the rules allow. The library is headless and turn-only:
no real-time controller, no Python, no ML runtime (`Umbra.PolicyDependencies` checks this).
Training tools live in `tools/policy` (README there).

## The contract (`PolicyContract.hpp`)

**Observation** `umbra.obs.turn1v1.v1`: 12 numbers, each in 0..1, in this order:

| # | Value | How |
|---|---|---|
| 0–1 | self health, energy | current / maximum (derived stats) |
| 2–4 | self guarded, weakened, poisoned | turns left / full duration (2, 3, 3) |
| 5 | self healing items | count / 3, capped |
| 6–7 | opponent health, energy | current / maximum |
| 8–10 | opponent guarded, weakened, poisoned | turns left / full duration |
| 11 | round | round / 30, capped |

The enemy sees only what it could see across the table. That's both bars and visible statuses,
plus its own bag. The opponent's bag is **not** in the observation (a test checks this).

**Action catalogue**: slot 0 is *wait*, then the learner archetype's turn skills (content order),
then the usable items of its starting inventory. For the bandit leader that's
`wait, strike, heavy_blow, guard, weaken, item:health_potion`. Enemy skills target the opponent,
and self skills and items target the actor itself.

**Mask**: a slot is 1 when `LegalActions()` lists that skill on the opponent (or self) or that
item right now. Waiting is always legal, so a mask is never all zero in a real turn.

**Fingerprint**: an FNV-1a 64-bit hash of a canonical text containing:

- the observation version
- both actors' stats, skills, equipment and inventory
- every skill they have: cost, target, turn flag, effects
- every item they carry or wear: slot, use effects, modifiers
- every status those effects apply: turns, damage per tick, modifiers

Names and looks are left out. A model is refused when the fingerprint, the catalogue or the
observation version differs (`CheckCompatible`, roadmap T26/T27). Same tensor shapes are not
enough: a stronger heavy blow changes what action 2 means.

## Training environment (`PolicyEnvironment.hpp`)

One opponent (team 0, index 0) against the learner (team 1, index 1) in a real `TurnEncounter`.

- **Opponent:** plays `ChooseTurnAction`, and with `opponentRandomness` probability a random
  legal action, so the learner meets more varied play.
- **Opponent level:** `opponentLevel` raises it, because the player meets the boss after
  levelling.
- **Varied starts:** `randomStart` varies both starting healths and the opponent's potions.
- **One step:** the learner acts once, then the opponent plays until it's the learner's turn
  again or the fight ends.
- **Illegal slots:** an illegal slot is played as *wait* and counted (it never reaches the rules
  as a forbidden action).
- **Reward:** `0.5 × (Δ opponent health share − Δ own health share)` per step, plus +1 for a
  win and −1 for a loss.
- **Endings:** a fight that passes 30 rounds is truncated, which is neither a win nor a loss
  (T24).
- **Repeatable:** `Reset(seed)` rebuilds everything from definitions, so the same seed and
  actions give the same fight (T23).

## The network (`PolicyModel.hpp`)

For each layer, `out[o] = act(bias[o] + Σᵢ weights[o·in + i] · in[i])`. `act` is `tanh` for
hidden layers and identity for the last one, which gives one score (logit) per action.

`Choose` takes the highest score among legal slots. It returns −1 if no slot is legal or any
score is not finite, and the caller then falls back (T25). The game plays greedily, with no
sampling, so the same situation gives the same move.

The file format is `umbra.policy` v1, JSON:

- `learner`, `opponent`, `observation`, `fingerprint`, `actions`
- `layers`: `in`, `out`, `activation`, row-major `weights`, `bias`
- `training`: recipe and seed
- `evaluation`: written by `approve.py`

`LoadPolicy` refuses wrong shapes, non-finite weights and a non-linear last layer.

## In the game (`PolicyDecider.hpp`, `apps/umbra/GameFlow.cpp`)

1. **Loading:** at start and after every content reload, each actor with `"policy"` gets its
   model if `LoadPolicy` and `CheckCompatible` pass. Otherwise the screen says it was refused and
   the standard AI plays that actor.
2. **Asking:** when that enemy's turn starts in a turn fight, the game takes an immutable
   snapshot (observation and mask) and hands it to the decider's worker thread with a fresh
   decision id.
3. **Acting:** after the usual pause (0.7 s, which is the deadline), the game takes the answer
   for that id. It uses the answer only if it exists, is ≥ 0 and is legal under the snapshot's
   mask. Otherwise the conventional AI acts and the fight log says so.
4. **Late and stale answers:** an answer for an older decision is dropped, never used (T19/T28).
   The decider runs one job at a time; a second request while busy is refused (overload),
   and that decision falls back.
5. **Shutdown:** the decider's destructor stops and joins the worker. An unfinished job is
   dropped.

Real-time fights always use the conventional real-time AI. The first learned policy is
turn-based only (C17).
