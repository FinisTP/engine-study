# Bots, the soak test and how to balance

Balancing means editing files under `plugins/last-curtain/assets/night/` and running the soak
test again. Code changes only when a new kind of mechanic is needed.

## The bots (the battle royale, 2026-11-16)

`ChooseAction(def, state, guest)` follows rules, not the model. No dice: the same state gives
the same choice.

1. A level: the stat of its weapon. A ghost: strike the weakest living guest in its room, else go
   after the nearest.
2. In battle: heal below `healBelowPercent`, a boost in round 1, leave below
   `fleeBelowHpPercent`; otherwise strike the weakest enemy.
3. A question to answer (any bot, and guest 0 in the soak): the answer with the higher chance.
4. Hurt below `fleeBelowHpPercent`: heal, else move away from monsters.
5. Its power on the weakest guest it can reach (damaging kinds); reflect when someone is in the room.
6. A guest in the room who would win a fight (`Edge` below 0): ask it to spare this one.
7. A chatty bot, or one fond of the player, asks the player a question first (a truce if passed).
8. Otherwise the best score: new rooms `explore`, rooms still holding an item or event, bonus
   rooms, quest goals (the quest item, the room to bring it to, the rooms to visit: weight 9; a
   quest item on the floor: 95 to pick up), prey its nature would attack in its light (its hunt
   anywhere: 15), loot, beatable monsters; minus `avoidGuests` toward a stronger guest, a ghost or
   one it dislikes. **The killer:** never into its room (-1000), and away from it within
   `senseSteps`. An attack scores `fight` x 10 + 5 + the edge (at most 40).
9. Nothing better here: a chatty bot asks someone in the room for a truce.

**Natures** (`guests.csv` `nature`; `WouldAttack`): a kind bot keeps every truce and attacks only
with a big edge (more than 8); a dirty one breaks a truce `persuade.dirtyBreakPercent` of the time
and attacks with any edge; a wicked one ignores truces and attacks unless clearly weaker (edge -3
or worse). A guest it was told to hunt it attacks unless much weaker. Natures also move how easily
it grants a request (`persuade.kindTarget`, `wickedTarget`).

## Running it

```
UmbraTool night-check [<data folder>]
UmbraTool night-run [--data <folder>] [--seed 1] [--player <guest id>] [--json]
UmbraTool night-soak [--data <folder>] [--runs 200] [--seed 1] [--strategy 0] [--json]
```

A run is one whole game: nights until one guest escapes or the house wins. `night-soak` exits 1
when the data does not hold: a fault (a stall or an illegal state), nights per run outside
`balance.nightsMin..nightsMax`, house wins above `houseMax`%, any guest winning more than
`winnerMax`%, or fewer than `deathsPerNightMin` deaths a night. Try a variant without touching the
shipped data: copy the folder, edit the copy, pass `--data <copy>`.

## What moves which line of the report

| Report line | Moved by |
|---|---|
| nights per run, deaths per night | the killer (`wakeRound`, `speed`, `speedPerHours`, `senseSteps`), `clock.rounds`, `monsters`, `spawn.minSteps`, natures |
| deaths by guests | natures, `styles.csv` `fight`, `persuade` (truces) |
| deaths by ghosts | `ghost` stats and `respawnRounds` |
| deaths by the house | the killer, monsters, `events.csv` fail amounts |
| the house won | the killer (it can kill the last two in one round only by luck) |
| winners | guests' stats, natures and styles; the quests' powers and stats |
| quests, powers used | `quests.csv` (rooms, counts), `powers.json` |

## Tuning order

1. Nights per run with the killer and the night's length.
2. Deaths by guests with natures and `persuade`.
3. The winners' spread with stats and quests.

## Tuning record

**Plan 2b (2026-11-15), goals and the 70-room board.** Start: the hunt 4-18% for the company,
room goals 95%, monster goals 5-8%, 42 rooms by midnight, 58% of rounds with a bot chat.
1. Goal rooms dealt within 65 rooms (`roomGoalRooms`, user), keepsake rooms still within 35.
2. Monsters: `chancePercent` 50 → 80, `max` 2 → 3, `stayRounds` 3 → 5; stagehand and
   understudy HP 10 → 8. Monster goals: bao and huy 1, khang 2. huy-events 3 → 2.
3. Six more event rooms in the deck for six fewer plain ones (85 entries kept); six new events
   [call]. The fly loft has two doors.
4. Styles: `explore` +2 each, `greed` added, schemer `chatty` 8, protector 7.
5. Keepsakes: mask → Orchestra room, ledger → Service stairwell, crown → Interval bar [call].
6. Speeds: bao 3 → 4, huy 6 → 5 (lan back at 4). khang distrusts lan (`bonds.csv`); khang's
   physical questions one step harder.
7. Minions HP halved, `hpShare` 25 → 30, so going for them pays again (strategy 1 wins fewer);
   then every `takenHp` by the measured share: nhi 27, thanh 18, bao 28, khang 44, mai 26,
   huy 51, lan 24.
8. Final review: bots no longer `Chat` with the player (that made the player join a bot's group)
   and the player's allies cannot be won away. Bao's goal "Nhi at his side" fell to ~7% (the
   player's group fills first) and left the band with Nhi's own goals. `takenHp` mai 23, thanh 19.
Result on seeds 1, 20001 and 80001 (2,000 nights each): the data holds; the hunt 48-51%,
every form 44-56%, rooms 47.2-47.3, bot chats 78-79%, banded goals 49-62%.

**Plan 2c, task 0 (2026-11-15): no ally goals (user).** Nhi and Bao: pass two tests; Mai: the
Royal box; Lan: the Hall of mirrors [call]. Bots: a chat before moving only once the goal is done
(it cost events goals their turns); a bot whose goal is done seeks company (+5); asking the player
needs `chatty` 7. Chatty: explorer 6, fighter 6, loner 5, protector 5; protector explore 8.
`roomGoalRooms` 70. `takenHp` re-measured: nhi 34, thanh 24, bao 42, khang 53, mai 28, huy 59,
lan 29. Result on the three seed ranges: the data holds; every form 46-60%, rooms 49, bot chats
73-75%, banded goals 47-62%.

**The battle royale, stage 6 (2026-11-16).** Start: 1.7-1.8 nights per run (band 2-4), 3.2-3.3
deaths a night, the house 0%, khang 28-29%, lan 23%. Tried, 200 runs each: killer speed 1 →
2.2 nights; monsters 30% → 1.8; spawn 6 apart → 1.7; speed 1 and wake at round 8 → 2.2; speed 1
and +1 every 3 hours → 2.3; speed 1 and 11 rounds → 2.5. Kept: killer `speed` 2 → 1,
`speedPerHours` 2 → 3 (the night keeps its 12 rounds). Result, 400 runs: the data holds; 2.3
nights per run, 2.5 deaths a night (guests 1.5, ghosts 0.2, the house 0.7), 3.2 quests and 1.7
powers a night; winners khang 29%, lan 27%, mai 13%, bao 12%, thanh 11%, nhi 3%, huy 2%; the
house 0%. Open: the kind guests (nhi, huy) rarely win, and the house almost never does (the last
two must die in the same round).
