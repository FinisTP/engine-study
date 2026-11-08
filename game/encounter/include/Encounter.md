# Encounter coordinator (`Umbra::Encounter`)

The only library that knows both combat modes. A game that uses only one mode links only that
controller (`Umbra::TurnCombat` or `Umbra::RealtimeCombat`); the reference RPG uses both through
this coordinator.

```
Exploration ──BeginRealtime──► Realtime ──fight decided──► Resolution ──Finish──► Exploration
Exploration ──BeginTurn─────► TurnBased ──fight decided──► Resolution ──Finish──► Exploration
                                  └──────── Finish (flee / abandon) ──────────────┘
```

## Rules (user decision 2026-11-07: statuses end with the encounter)

- **One controller at a time.** Beginning a fight while one is running or awaiting `Finish` is
  refused. The two controllers can never commit effects for the same fight.
- **Modes change only between fights.** There is no switch in the middle of a swing, so there is
  no cost/refund question at a switch and no turn ↔ second conversion. (Roadmap 4.3 allows a later,
  explicit mid-fight transition rule; it is out of scope now.)
- **Finish** ends every fighter's statuses and destroys the controller with its cooldowns, intent
  log and pending actions. Health, energy, inventory, equipment, skills and skill points stay:
  that is the shared progression between fights.
- **Every decision names its fight.** `BeginTurn` / `BeginRealtime` return a new encounter id;
  `Submit(encounter, intent)` with an older id is `Stale` and changes nothing. This protects against
  a slow decision (an AI request, a delayed input) landing in the next fight (roadmap T47). After the
  fight is decided, decisions are `EncounterOver`.

Decision ids (`ActionIntent::id`) are checked per fight. The encounter id is what separates fights,
so the same decision id may be reused in a later fight.

## What the caller still does

- Real-time: report positions every frame (`Realtime()->SetPlacement`), call `Update(seconds)`.
- Turn-based: ask whose turn it is (`Turn()->Current()`), get the player's or AI's choice, `Submit`.
- Read events from the controller (`TakeEvents`) for UI, audio and quests.
