# The Last Curtain: goals, a bigger board and a better table (plans 2b and 2c)

> **Status:** Ready for approval — revised twice after `design-system` reviews; no blocking concern left
> **Author:** the user + Claude · **Last updated:** 2026-11-14 (night)
> **Implements pillars:** 1 Who will turn? · 2 The building is the board · 3 Prepare, then survive · 4 Every night differs
> **Quick reference** — Layer: Core · Priority: Vertical slice · Key deps: the night rules (`Night.md`), bots (`Balance.md`), `LastCurtain.exe` (plan 2)

Marks: **[call]** = Claude's fill-in for the user to change. "User" = decided by the user.

## Summary

Omens go. Each guest secretly draws one personal goal at the start of the night; at midnight the
Taken is drawn from those who failed, and the other failures drop to 1 HP. The board grows to 9×9
with 70 rooms, guests start scattered, bots work toward goals and recruit each other, and the
table gets a proper turn flow, camera and inspection.

## Overview

Plan 2 showed an aimless evening: no push before midnight, bots that take every item, monster and
event, a player who mostly watches, bots pacing back and forth and rarely talking, and 20 rooms that
run out early. This change gives every guest a reason to move and a visible stake in the evening
(the goal decides who may turn), a board that lasts until midnight, and bots whose turns read as
plans. It is built in two plans: **2b** the rules, balanced in the simulation; **2c** the screen.

## Player Fantasy

"I am racing the clock for my own reason, while watching six others race for theirs, knowing one
of us who falls short will come back as something else." The moment: round 9, the Ledger is in a
rival's bag, and you must decide whether to chase it, talk your way to it, or let someone else fail
instead.

## Approaches (the user chose A)

- **A. Rules first, then the screen (chosen).** 2b changes and re-balances the simulation; 2c builds
  the screen on the settled rules.
- B. One plan: earlier play, but balance judged on a half-changed game.

## Detailed Design

### Core rules (plan 2b)

**Goals (user)**
1. Omens, the curse roll, the early Turn and `omens.csv` are removed. The five omen rooms change
   kind: two item rooms, two event rooms, one bonus room **[call]**.
2. Each guest has **2–3 goals** in `goals.csv` that fit their story; one is drawn secretly per night
   (user). Kinds: `room` (entered at least once before midnight), `item` (carry a named keepsake at
   midnight), `ally` (a named guest in your group at midnight), `events` / `monsters` (pass N event
   rolls / destroy N monsters before midnight).
3. A guest knows its own goal. When a guest passes a chat answer, the asker's goal becomes known to
   the answerer; in a bot-to-bot chat both learn each other's (user). Known goals show on screen.
4. **Keepsakes:** unique items, each waiting in a fixed room, found on first entering it; they can
   be dropped, picked up and taken by rivals (Phoenix crown in the Costume store, the Ledger in the
   Shuttered office, the Gilded mask in the Mask cabinet, the pocket watch in the Box seats, the ring
   of keys in the Back corridor **[call]**). A keepsake is in play only when a drawn goal needs it.
   **Even difficulty (user):** every goal must be about as hard to finish within the ten evening
   rounds, and, like reaching a room, depend on the luck of the draw and the board rather than be
   a sure thing. The test is the soak: each goal, held by a bot, is finished in **45–65%** of
   nights **[call]**. Counts (N events, N monsters) and keepsake rooms are tuned in data until every
   goal sits in that band; a goal that cannot be brought into it is dropped. The full house (all
   seven finish) then follows from the band (about 0.55^7 ≈ 2% if goals were independent).

   Draft goals, one drawn per night, with starting counts for the soak to tune **[call]**:

   | Guest | Goals |
   |---|---|
   | Truong Uyen Nhi | room: the Patron's box (her father's seat) · item: her father's pocket watch · ally: Phan Thi Mai |
   | Le Thanh | item: the Phoenix crown · room: the Auditorium · events: pass 2 |
   | Dinh Quoc Bao | ally: Truong Uyen Nhi · room: the Green room (the old infirmary) · monsters: destroy 2 |
   | Vo Minh Khang | item: the patron's Ledger · monsters: destroy 3 · room: the Shuttered office |
   | Phan Thi Mai | room: the Chapel of the stage ancestors · ally: Le Thanh · item: the ring of keys she lost |
   | Hoang Gia Huy | events: pass 3 · room: the Fly loft · monsters: destroy 2 |
   | Tran Ngoc Lan | item: the Gilded mask · item: the Phoenix crown · ally: Vo Minh Khang |

   Keepsakes then: Phoenix crown, Ledger, Gilded mask, pocket watch, ring of keys.
5. **Goal rooms come out early (user):** every room that a drawn goal or keepsake needs is
   shuffled into the first 35 rooms opened, and is never a start room.
6. **Midnight (user):** if every living guest finished, the night ends at once in a win for the
   company (no hunt). Otherwise the Taken is drawn at random from the failed guests, every other
   failed guest drops to 1 HP, and the Turn goes on as now (company in the foyer, Taken under the
   stage, minions).

**Board (user: 9×9, about 70 rooms, more special rooms)**
7. Grid 9×9, 70 rooms: the 28 current, about 26 new special rooms (item, event, trap, bonus, a
   second transport pair) and about 16 fillers (hallway, empty room, passageway, landing...); six new
   events and three new traps; names and texts **[call]**.
8. **Start rooms (user):** before round 1 the foyer is placed in the middle and seven rooms are
   opened outward from it at random, one per guest; each guest starts in its own; a start room gives
   nothing on that first entry.
9. A side is passable only when both rooms have a door there and it is not sealed (unchanged rule;
   the screen will now draw it that way).

**Moving (user)**
10. In a turn, stepping back into the room you just left gives the step back (an undo of the last
    step). Only a plain step can be undone: not one that opened a room, rolled, started a fight or
    picked something up. Scouting this way is allowed (user), but **no pacing (user):** a room
    left by an undo cannot be entered again in the same turn, so A → B → back to A → B is refused.

**Bots**
11. **Purpose:** at the start of its turn a bot picks one target and walks a shortest path to it,
    never back the way it came: its goal (a known room, a keepsake it has *seen* or its holder, the
    guest to win over, an event room), then a known goal of a group member (user: groups help
    members' goals), then what its style wants, then the nearest doorway. It stops at the target.
12. **Less greed:** bots pick up healing, keepsakes their side needs, or what `greed` in `styles.csv`
    allows **[call]**; they fight monsters when a goal or the style asks.
13. **Bot-to-bot chats (user):** a bot may open a chat with another guest in its room at the start of
    its turn (once a round); the other answers by its own choice; three approvals over separate turns
    bring it into the asker's group.
14. Bots act only on what their guest knows (seen rooms, items, holders; known goals).
15. After midnight bots play as now.

### States and transitions (plan 2c: the player's turn)

```
menu ──Move──▶ picking ──lit square──▶ (new room?) ──yes──▶ confirm ──Stay──▶ moving ──▶ menu
  │              │  └──Esc / Back──▶ menu                     └──Back──▶ picking
  ├──Fight / Use item / Chat ──▶ that screen ──▶ menu
  └──End turn ──▶ (bots' turns)
A dialog (event, dice, Turn) can open from moving; it closes back to menu.
```

- **Clicking a lit square (user):** the pawn walks there one cardinal step at a time along the
  shortest legal path, each step costing a move and shown sliding; it never jumps. It stops early
  where a step opens a room (the Stay/Back confirm), rolls, meets a monster or runs out of moves.
  A square with no legal path is ignored; one beyond the moves left is ignored too, and while
  picking its path is drawn red past the last step you can afford (user).
- **Right-click inspects (user):** left-click walks (or chooses a square in pick mode); a right-click
  on a room or pawn opens its info at any time. On a bot's turn a left-click inspects too.
- **Input depends on the state (user):** every key and click means what the current state or the
  open screen says, and nothing else:

  | State | Esc | Arrows | Enter | Click on the board |
  |---|---|---|---|---|
  | menu (your turn, idle) | save menu (Save, Resume, Save and quit) | pick mode, step that way | End turn | left: walk to a lit square · right: inspect |
  | picking / walking | cancel the move (back to menu; steps already taken stay) | step | confirm the chosen square | left: choose a square · right: inspect |
  | confirm (Stay/Back) | Back | — | Stay | — |
  | a dialog (event, dice, chat, the Turn, bag, inspect) | close or Back where it has one | move focus | the focused button | ignored |
  | a bot's turn | save menu | — | — | left or right: inspect |

### Interactions with other systems

- **Saves:** goals, known goals, keepsakes and start rooms join the save; the save version rises,
  so plan-2 saves are refused and Continue hides (no migration) **[call]**.
- **Narrator:** new caption lines for bot-to-bot chats, recruiting, goal reveals and goal done.
- **Battles, items, events:** unchanged apart from fewer greedy bots; keepsakes are ordinary items
  to the bag.

## Formulas

The **midnight draw** is defined as:
`P(guest g turns) = 1 / F` for each failed guest g; `F` = failed guests at midnight (1 ≤ F ≤ 7).
No draw when F = 0 (the company wins).

| Variable | Type | Range | Source | Description |
|---|---|---|---|---|
| F | int | 0–7 | calculated | living guests whose goal is not done at midnight |

**Output range:** 1/7 to 1. **Example:** Huy and Lan failed → each turns with 1/2; the other one drops to 1 HP.

The **failure penalty** is defined as: `hp = min(hp, failHp)` for each failed guest not drawn.

| Variable | Type | Range | Source | Description |
|---|---|---|---|---|
| failHp | int | 1–(max HP) | data file (`rules.json`) | HP a failed guest keeps at midnight (user: 1) |

**Output range:** 1 under the user's value; a downed guest stays down. Intended loop: more failures
→ a weaker company → a likelier Taken win; damped by healing in the hunt and by goals being
reachable (rule 5) **[call]**.

The **room budget** is defined as: `opened by midnight ≤ 7 start + Σ(rooms opened per turn)`, at
most one per guest per turn → `≤ 7 + 7 × 10 = 77` against `rooms = 70`; the soak reports the real
count.

| Variable | Type | Range | Source | Description |
|---|---|---|---|---|
| rooms | int | 30–81 | data file | rooms in the deck that can be opened |
| earlyGoalRooms | int | 10–rooms | data file | goal and keepsake rooms come out within this many (user: 35) |

**Example:** the soak's first pass (plan 2) opened 18.5 of 20 rooms a night; with 70 the aim is
about 45–65 by midnight **[call]**.

Hunt distances on 9×9 (foyer to under-stage ≥ 3 steps, unchanged) are left to the soak; if the
company rarely meets the Taken in ten rounds, `light.steps` or the under-stage distance is retuned.

## Edge Cases

- **If a goal's room or keepsake is gone** (sealed off, the keepsake carried by the Taken): the goal
  can still fail; that is play. Rule 5 only guarantees the room is placed.
- **If the goal's target guest is dead** (ally goal): the goal fails.
- **If an ally goal names the player's guest:** allowed — the bot must win the player over; the
  player decides **[call]**.
- **If a failed guest is down or dead at midnight:** a downed guest can be drawn as the Taken (it
  returns at full strength as now); the 1-HP rule does nothing to the dead or the downed.
- **If two goals need the same keepsake:** both can be drawn; only one can hold it at midnight.
- **If the player becomes the Taken through failing:** as now (the player plays the Taken).
- **If every living guest finished:** the night ends at midnight in a company win.
- **If a keepsake is in a bag when its holder dies:** it drops in that room (the existing drop rule).
- **If undo would cross a room opened this turn:** not allowed (only plain steps).

## Dependencies

| System | Direction | Hard/soft | Interface |
|---|---|---|---|
| Night rules (`modules/night`) | this changes it | hard | `NightDef` gains goals and keepsakes; `NightState` gains goals, known goals, path for undo |
| Bots (`modules/nightbots`) | this changes it | hard | goal-driven targets; bot-to-bot chats |
| Saves (`NightSave`) | this changes it | hard | new fields, version + 1 |
| Board view and narrator (`modules/board`) | depended on by 2c | soft | known goals, groups, new caption lines |
| `LastCurtain.exe` (plan 2) | 2c changes it | hard | turn flow, camera, inspect |

## Tuning Knobs

| Knob | Where | Safe range | Too high | Too low |
|---|---|---|---|---|
| `board.width/height`, `rooms` | `rules.json` | 7–9, 40–75 | long walks, few meetings | rooms run out before midnight |
| `goals.earlyRooms` | `rules.json` | 20–50 | goals hard to reach | goal rooms crowd the start |
| goal counts (`events`/`monsters` N) | `goals.csv` | 1–3 | the guest turns too often | goal trivial |
| `failHp` | `rules.json` | 1–half max | failing costs nothing | — |
| `chatty`, `greed` | `styles.csv` | 0–10 | chat or loot spam | silent, empty-handed bots |
| `persuade.approvals` | `rules.json` | 2–4 | groups never form | everyone allied by round 3 |
| slide time, minimum think pause, beat lengths | `LastCurtain` (plan 2c) | 0.15–0.4 s, 0.4 s floor (user) | slow evenings | unreadable bot turns |
| goal counts and keepsake rooms | `goals.csv` | until each goal is 45–65% | the holder turns too often | the goal is a formality |

`failHp` and the hunt's balance interact: a lower `failHp` needs a weaker Taken.

## Visual/Audio Requirements

No art bible yet (plan 4). Placeholder boxes stay. Group membership shows as a ring colour **and**
a small group number on the ring (not colour alone); a goal reveal and a goal done each get a
caption; the thinking pause shows "thinking..." over the pawn.

## Game Feel

- Pawns slide room to room (about 0.25 s a step **[call]**).
- **Thinking pause (user):** a bot in view shows "thinking..." before its first action for
  `max(0.4 s, the time its decision really took)`. Today a decision takes microseconds, so it is
  0.4 s; when the built-in model makes decisions (plan 3), the pause is the model's own time.
- While focused, the camera follows whoever moves, step by step; dragging frees it until Focus.

## UI Requirements (plan 2c)

- The turn menu, picking mode, the Stay/Back confirm, and the input table (states above).
- Drag to look; a **Focus** button; the save menu (Save, Resume, Save and quit).
- Inspect: right-click a room → name, kind, what is known; right-click a pawn → HP, stats, level,
  group, bag, and goal if known.
- HUD: your goal and its progress ("Pass 2 events: 1 of 2"); "Goals you know" list.
- Walls drawn where a guest cannot pass; doorway marks only where a room can still open.

## Cross-References

- `game-design.md` §1, §4, §6, §12, §13, §16 and the decision log are rewritten for goals in 2b's
  first task (omens removed).
- `cast.md` gains each guest's goals.
- `Night.md`, `Balance.md`, `Board.md`, `Table.md` updated with their code.

## Acceptance Criteria

Automated (2b):
- **GIVEN** `goals.csv` with an unknown room, item, guest or kind, **WHEN** the night loads, **THEN** it is refused with file, row and column.
- **GIVEN** a seed, **WHEN** a night starts, **THEN** each guest has one goal drawn from its own rows, and the same seed draws the same goals.
- **GIVEN** each goal kind met or not at midnight, **WHEN** the Turn comes, **THEN** done/failed matches (one test per kind).
- **GIVEN** two failed guests, **WHEN** midnight comes, **THEN** the Taken is one of them and the other has 1 HP; over 1,000 seeds each is drawn 40–60%.
- **GIVEN** every living guest finished, **WHEN** midnight comes, **THEN** the night ends in a company win and no Taken exists.
- **GIVEN** a downed failed guest, **WHEN** midnight comes, **THEN** its HP is untouched unless drawn.
- **GIVEN** a new night, **WHEN** the board is laid, **THEN** seven start rooms are connected to the foyer, one guest each, none a goal or keepsake room.
- **GIVEN** drawn goals, **WHEN** 35 rooms have opened, **THEN** every goal and keepsake room is among them (500 seeds).
- **GIVEN** a plain step, **WHEN** the guest steps back, **THEN** the move is refunded; **GIVEN** a step that opened a room, **THEN** stepping back costs a move.
- **GIVEN** a passed chat answer, **WHEN** it resolves, **THEN** the answerer knows the asker's goal (both ways between bots).
- **GIVEN** a keepsake dropped in a room, **WHEN** another guest picks it up, **THEN** it counts for that guest's item goal only.
- **GIVEN** a plan-2 save, **WHEN** Continue is checked, **THEN** it is refused.
- Soak (1,000 nights): no bot steps back mid-path; every bot turn ends at its target or with the target gone; ≥ 1 bot-to-bot chat a round in ≥ 70% of evening rounds; every Taken form 40–60%; each guest the Taken in 5–30% of nights; all-finished nights ≤ 10% (user); each goal finished by its bot holder in 45–65% of nights **[call]**; 45–65 rooms open by midnight **[call]**.

Automated (2b, added): **GIVEN** a guest that undid a step from A back out of B, **WHEN** it tries to enter B again that turn, **THEN** the move is refused. **GIVEN** 1,000 nights, **WHEN** the soak runs, **THEN** every goal held by a bot is finished in 45–65% of the nights it was drawn.

Automated (2c): **GIVEN** the menu and a lit square 3 steps away with 4 moves left, **WHEN** it is clicked, **THEN** the pawn takes 3 cardinal steps, each sliding, and 1 move is left; **GIVEN** the path crosses an unopened room, **THEN** the walk stops at its door with the Stay/Back confirm. **GIVEN** each state of the input table, **WHEN** each listed key is pressed, **THEN** exactly the listed thing happens (one test per cell). **GIVEN** a bot decision that took 0.1 s / 0.9 s, **WHEN** it is in view, **THEN** "thinking..." shows 0.4 s / 0.9 s. Menu and picking ignore illegal squares; Stay opens the room, Back cancels; a slide
takes its time and a dialog waits for it; a drag stops following until Focus; inspect data for a
room and a guest matches the state.

Screenshot / play (2c, Windows): the Windows checklist in the 2c record — a full night as one guest,
a goal finished and one failed, a bot-to-bot recruit seen, the camera follow and drag, the save menu, Esc in each state.

## Open Questions

- None blocking. The **[call]** items above are Claude's choices for the user to change: goal lists,
  keepsake rooms, new room names, `greed`, timings, the goal band 45–65%, the shared Phoenix crown between Lan and Thanh.
