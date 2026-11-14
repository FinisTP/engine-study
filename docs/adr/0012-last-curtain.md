# ADR 0012: The Last Curtain — one night of exploration and betrayal replaces the living town's example game

- Status: **accepted; planned** (user, 2026-11-14). Nothing implemented yet.
- Supersedes: ADR 0011's phase S25 ("the town", an example game slice). S21-S24 stay in the
  repository as the `living-town` plugin and are not extended.
- Design: `docs/design/game-design.md` and `docs/design/cast.md` (approved 2026-11-14).
- Plan: `docs/design/plans/2026-11-14-night-simulation.md` (plan 1 of 4).

## Context

After S24 the user chose the example game's theme. Instead of an open-ended town it is a
single-player digital game on the core loop of the board game *Betrayal at House on the Hill*:
explore a building tile by tile, then one of the party turns on the rest. The user judged the
open-ended town too costly to finish alone and asked that every mechanic that serves no purpose
be cut.

## Decisions (the user's)

| # | Decision |
|---|---|
| 1 | One night, 19:00 to 5:00, 20 rounds; seven guests, the player picks one, six are bots. |
| 2 | The place is the Saigon Opera House under its real name and history; only the guests and the night's events are invented. |
| 3 | Omens (cursed keepsakes with great power and a rising d20 roll) decide who is marked; the marked guest turns at midnight and may be the player. |
| 4 | Bringing the marked guest down by a guest's hand before midnight wins the night. |
| 5 | Dice (d20 + stat) on the board; battles without dice, at most 3 rounds. |
| 6 | The built-in model is required; it words lines and tales and never decides rules. |
| 7 | The whole night is first built as a simulation without graphics, for balancing. |
| 8 | All gameplay data is JSON or CSV; balancing must not need code changes. |

The full list with dates is the decision log in `game-design.md` section 21.

## Consequences

- From the town the game reuses the approach (one copyable whole-number state, seeded, forked by
  bots, soak-tested), the built-in model with its guard, and the idea of bonds. Needs, schedules,
  wages, rumours, the chronicle and the storyteller's outcome plan are not used.
- New: a board of hidden rooms, the night's rules, bots that play to win, light and dark, the
  seven powers, and later the 3D board, UI and art.
- The night has its own small battle rules (two damage types, starter first, 3 rounds); the
  engine's turn-combat rules are not used by it (to be confirmed by the user with the plan).
- No text, names, scenarios or art of the board game are used; the game has its own terms.
- A real public monument is named: everything invented is labelled as fiction in the game, and
  permission to use the name and likeness should be checked before sale (Claude is not a lawyer).

## Revisit when

- The soak test cannot bring all seven Taken forms into one win-rate band through data alone.
- A night in the simulation is far from 140 turns or bots stall.
