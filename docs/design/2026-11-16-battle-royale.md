# The Last Curtain: the battle royale (design, branch `battle-royale`)

Date: 2026-11-16. The user's direction (2026-11-16 01:14-01:30) and answers; my calls are marked
**[mine]** and can be changed. The co-op version stays on `main`.

## Intent

A trapped guest competes with six bots to be the one who leaves the Opera House alive. The bots
are tactical and have natures (kind, dirty, wicked); they fight each other as much as the player.
The house adds danger that grows through the night: traps, monsters, and a wandering killer that
cannot be fought. Dying is not the end: the dead join the house as ghosts and win with it if no
guest survives. The night never ends while more than one guest lives: at dawn the survivors
compete again on a new map.

Why: the co-op version was slow before midnight (wandering to unlock rooms) and weak after it (a
lone player could not beat the Traitor and did not know minions were the way).

## Goals

- Every turn has a reason: a quest to race for, a rival to avoid or ambush, a bot to talk round.
- Growth is visible and wanted: quests give powers and stat buffs; items and levels matter.
- Bots read as characters: a kind one keeps its word, a dirty one breaks it, a wicked one hunts.
- Death keeps you playing (a ghost of the house).

## Non-goals

- Allies and groups (user: balancing cost). No co-op ending.
- Opening tiles one by one. The Taken as a guest who turns.

## Anti-goals

- Early kills by spawn luck (spawns are far apart).
- A killer that feels unfair: it is seen in the light, it is announced, it can be blocked.
- Rule changes that need a new AI: bots keep the current one-ply chooser, with new scores.

## The rules

1. **The map** is generated whole at the start of each night: about 45 rooms on the 9x9 grid,
   connected, with some loops **[mine]**. Every room is open; no doorways. Rooms keep their kind:
   item, event and trap rooms fire for the first guest to enter (persistent traps stay armed);
   bonus rooms pay once per guest. The light stays as vision **[mine]** (options.csv `light` 1 by
   default again): you see rooms within your light's steps; ghosts and the killer see everything.
2. **Spawns**: the seven guests start in rooms chosen to keep the smallest distance between any
   two as large as possible (at least 4 steps when the map allows) **[mine: farthest-point
   picking]**.
3. **Fighting**: any guest may attack another in the same room at any time (a battle as today,
   two sides of one fighter each, plus monsters there join neither side). 0 HP is death; the
   items fall on the floor. The "down" state goes.
4. **Ghosts**: a dead guest becomes a ghost of the house. Ghosts appear on a fixed tile (the
   under-stage), are weak (rules.json `ghost`: HP, Might, Sanity, Speed **[mine: 6, 2, 2, 3]**),
   can attack the living, cannot use items, quests, events or rooms, and see everything. A ghost
   brought to 0 HP comes back at the under-stage after `respawnRounds` **[mine: 3]**. The player's
   ghost is played by the player.
5. **The killer**: the Taken figure, renamed per night from the guests' Taken forms **[mine]**. It
   wakes at midnight in the under-stage (the midnight scene) and moves after every round:
   `speed` rooms, +1 every two hours **[mine: 2]**, toward the nearest living guest it can sense
   (within `senseSteps` **[mine: 5]**), else at random. Entering a room with living guests it
   kills one (the lowest HP; ties by order) **[mine: one per move]**. It cannot be attacked.
   Seal blocks it like anyone; flood halves its moves. Ghosts are never harmed by it. A guest
   within 2 steps of it hears it (a warning on screen and a sound).
6. **Time**: 12 rounds a night **[mine]**, 20:00 to dawn at 45 minutes a round. Monster chance and
   number rise each hour (rules.json); the killer speeds up.
7. **Nights**: at dawn, if two or more guests live, a new night begins on a new map: survivors keep
   items, levels, powers and buffs, and heal fully; ghosts stay ghosts. One guest left alive at any
   moment: they escape, the night ends. No guest left: the house wins.
8. **Winning (the player)**: alive and last: you escape. A ghost when the house wins: you win with
   the house. A ghost when someone escapes: you lose.
9. **Quests** (quests.csv, the user's example: the corpse from the wig room to the artists' waiting
   room): visible to all; each can be finished once per night, by the first (a race). Kinds
   **[mine]**: `carry` (an item placed at night start in room A, brought to room B), `visit`
   (stand in rooms A then B in one night), `slay` (destroy N monsters), `endure` (pass N event
   rolls). A finished quest grants its power (powers.json, the old Taken powers made for guests)
   and its stat buff. A guest holds one power at a time, the newest **[mine]**; powers have
   cooldowns. Rooms named by quests are generated into every map.
10. **Persuading** (Chat): with a guest in the same room, once a round, pick a request: `spare`
   (a truce: neither may attack the other for `truceRounds` **[mine: 3]**), `item` (they drop an
   item of yours to pick: the first they carry **[mine]**), `target` (they go after a guest you
   name for `truceRounds`). Then they ask one of their questions (questions.csv) and the answer
   is rolled as today; their nature and bond move the target. A dirty guest may break a truce
   (`breakPercent`); a kind one never does; a wicked one is harder to persuade.
11. **Natures** (guests.csv `nature`: kind, dirty or wicked) **[mine: assignments from cast.md]**
   shape the bots: aggression, truce keeping, how hard they are to persuade, whether they go for
   the weak. Styles stay for movement and loot.

## What goes (rules and screens)

The Turn, the marked guest, goals and keepsakes, approvals and groups, minions, "down", the
co-op outcomes. Saves go to version 6 (older refused, as before).

## Screens

- **Quest panel** (top left, where the goal panel was): every quest, its reward, who claimed it.
- **HUD**: your power and its cooldown; the night number; a killer warning when it is near.
- **Persuade**: Talk in the menu opens the request picker (spare, item, target + whom), then the
  question as today.
- **Ghost play**: a ghost HUD; the turn menu with Move, Attack, End turn.
- **Scenes**: opening (the map, the guests far apart), midnight (the killer wakes), dawn (the
  survivors, a new night), endings (escape, the house wins).
- **The fall screen** stages every death: who, how.

## Approaches considered

- **Rebuild the rules as a new module** next to the old one: clean, but doubles the code and the
  tests. Rejected.
- **Change the rules module in place on the branch** (chosen): keep the board, battles, events,
  items, levels, powers, chats and bots; remove what goes; add the new parts. The branch keeps
  `main` safe.
- **Only retune the co-op game** (stronger goals, explicit minions): cheaper, but not the game the
  user described. Rejected by the user's direction.

## Left to the builder

Numbers (balanced by the soak: a night should kill 2-4 guests; nights to a winner 2-4; the player
wins about 1 in 7 against equal bots), the room count and loop count, the ghost and killer
numbers, quest data (8-12 quests written from the rooms and cast; fill-ins), bot scores.
