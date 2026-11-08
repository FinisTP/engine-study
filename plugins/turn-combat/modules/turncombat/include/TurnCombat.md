# Turn combat (`Umbra::TurnCombat`)

Turn order and turn ownership only. Damage, costs, statuses and death come from the shared rules
(`game/rpg/include/Rpg.md`). This library links `Umbra::Rpg` and nothing else: no real-time
controller, world, physics or renderer. `Umbra.TurnOnlyDependencies` fails if that changes (roadmap T39).

## Order (user decision 2026-11-07: speed rounds, one action)

```
round starts -> living fighters sorted by derived speed, fastest first (ties: roster order)
  for each fighter in that order:
     skip if dead
     TurnStarted; statuses tick (may kill -> skipped)
     wait for Submit: one action or "wait"   -> turn used up
round ends -> next round (order recomputed, so a speed buff counts from the next round)
```

The order is fixed for the round: a fighter slowed mid-round still acts in its slot.

## Submit

1. Over? `EncounterOver`. Not the current fighter? `NotYourTurn`.
2. "Wait" (no skill, no item) is always legal, so a fighter with no affordable action can never
   block the encounter.
3. Otherwise `rpg::CheckAction` in turn mode. Any rejection changes nothing and the turn stays.
4. A decision id seen before is `DuplicateIntent`.
5. Commit: pay the cost, apply the effects, check for a winner, move to the next turn.

The encounter ends when only one team has living fighters (`Winner()`), or `rpg::noSurvivors`
when a status tick kills the last fighters of both teams.

## Legal actions

`LegalActions()` lists every action `Submit` would accept for the current fighter, in a fixed
order: unlocked skills sorted by id (self skills once, enemy skills once per living enemy), then
each usable item once, then wait. This is the action set a training environment or an enemy AI
picks from (roadmap P5.8).

## Conventional enemy

`ChooseTurnAction` picks from `LegalActions()`:

1. Below 35% health: use an item whose first effect heals.
2. Otherwise attack the living enemy with the lowest health, using the skill with the most
   expected damage against it.
3. Nothing to attack with: wait.

It submits through `Submit` like the player, so it can never do anything the rules forbid.

## Headless environment (P5.8)

`TurnEnvironment` is the seam a training tool or scripted test drives, with no training framework
inside the engine:

| Call | Meaning |
|---|---|
| `Reset()` | Rebuild every fighter from its archetype (`ScenarioSlot{actor, id, team}`), new encounter: round 1, empty decision log, no statuses, step counter 0 |
| `Current()` | Who must decide now (nothing when done) |
| `LegalActions()` | The exact set `Step` accepts |
| `Step(intent)` | One decision; the environment numbers it. Returns the rejection (if any), the events it caused, `done`, `winner` |

It wraps the same `TurnEncounter` the game uses, so there is no second copy of the rules to drift
(roadmap T55): a test replays a recorded episode through a plain `TurnEncounter` and gets identical
fighters. One accepted step is one fighter's turn; a rejected step uses nothing (T60). Reset destroys
the old encounter before rebuilding characters, so no counter, status or log survives into the next
episode.

Not here yet, on purpose: observation encoding, action masks as tensors, rewards and seeds. Those
belong to the P8A observation/action contract. There is no randomness in the rules, so an episode
is fully determined by its decisions.
