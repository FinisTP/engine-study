# The battle royale: build plan

Spec: `docs/design/2026-11-16-battle-royale.md`. Branch `battle-royale` (cloud `/home/claude/umbra`,
PC `E:\Projects\EngineStudy`). Inline execution, task by task, TDD; ship each stage with
`/home/claude/ship.sh` + `git am` on the PC branch (compare trees).

## Global constraints

- CLAUDE.md: short code, `// 1.` step comments, `.md` for concepts; ASCII C++ literals; commits
  `issue #UMBRA-<n> <summary>` with the session's attribution lines; never commit
  `projects/reference/assets/scenes/settlement.scene.json` on the PC; all gameplay numbers in
  `plugins/last-curtain/assets/night/` data.
- Rules stay whole-number and inside `NightState` (no pointers); bots copy states.
- Update the golden fingerprint in `tests/SaveTests.cpp`; save version 6.
- Numbering: commits from `#UMBRA-217`; tests from T228.

## Stage 1: the rules core (`modules/night`)

1. Map: `BuildBoard` opens rooms until `rules.board.rooms` (45) are placed, all opened; no move
   opens a room (Doorway moves gone). Quest rooms and the under-stage always placed. Spawns by
   farthest-point picking. Tests: every placed cell reachable; spawn min distance >= 4 on seeds
   1-50; no Doorway move legal.
2. PvP and death: Attack any living guest in the room; 0 HP = dead, items fall, `Died` logged;
   no "down". Outcome {Running, Escaped, House}; `state.winner`. Tests: battle between guests;
   the last living guest ends the night as Escaped; none: House.
3. Ghosts: `Guest::ghost`, `returnRound`; spawn at the under-stage with rules.ghost stats; act in
   the order; attack the living; no items/rooms/events; killed: back after respawnRounds.
4. Nights: 12 rounds; at dawn with 2+ living a new map (`night` counter), survivors keep gains and
   heal, ghosts to the under-stage. Log `Dawn`.
5. Remove the Turn, Taken, goals, keepsakes, groups/approvals joining, minions, down/relic; data
   files goals.csv/minions.csv dropped; save v6; fingerprint.
   Ship: `#UMBRA-217`.

## Stage 2: the killer and the growing danger

`killer` in rules.json (wakeRound, speed, speedPerHours, senseSteps, hearSteps, name per night from
guests' Taken forms). It moves after each round, kills one living guest per room entered (lowest
HP), never attacked; seal blocks, flood halves. Monster chance/max per hour. Log `Killer`
(moved/killed). Tests: wakes at midnight; hunts within sense; kills one; blocked by a seal; ghosts
safe. Ship `#UMBRA-218`.

## Stage 3: quests and powers for guests

quests.csv (kind carry/visit/slay/endure, rooms, item, count, power, stat, amount, text); quest
items placed at night start; claim once per night; reward power (guest's `power`, cooldown) and
stat. Powers re-targeted: any guest's power hits other living guests (and ghosts). Tests per kind;
race (second finisher gets nothing); power use and cooldown. Ship `#UMBRA-219`.

## Stage 4: persuading and natures

Chat with a request (spare / item / target + whom) -> question -> roll; truces (`Truce` marks),
target orders; guests.csv `nature`; break chance; bots score attacks, quests, flight from the
killer, truces by nature. Soak: nights per run, deaths per night, winners spread, no stalls.
Ship `#UMBRA-220`.

## Stage 5: screens and scenes

Quest panel; power button; ghost HUD; killer warning + sound; persuade picker; dawn screen and
scene; endings; fall screen for every death; cutscenes.json updated. Ship `#UMBRA-221`.

## Stage 6: balance and records

Soak bands, docs (Night.md, Play.md, Balance.md), testing record `s38`, ROADMAP row. Ship `#UMBRA-222`.

## Ledger

(appended as stages finish)

Stage 1: complete (#UMBRA-218; RoyaleTests T228-T234, suite 181/182, the balance soak open: 2 faults in 400 runs, Khang wins 50%)
Stage 2: complete (#UMBRA-219; T236-T240; soak 0 faults, 1.7 nights per run: too deadly, for stage 6)
Stage 3: complete (#UMBRA-220; T241-T245; quests.csv, 9 quests; powers for guests; bots race quests and use powers: 3.3 quests and 2.1 powers a night; soak 0 faults, 1.6 nights per run)
Stage 4: complete (#UMBRA-221; T246-T249; requests spare/item/target, truces, hunts, natures kind/dirty/wicked; Play tests use calm, even guests)
