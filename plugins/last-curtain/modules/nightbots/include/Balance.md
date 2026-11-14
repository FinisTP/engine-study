# Bots, the soak test and how to balance

Balancing means editing files under `plugins/last-curtain/assets/night/` and running the soak
test again. Code changes only when a new kind of mechanic is needed.

## The bots

`ChooseAction(def, state, guest)` follows rules, not the model. No dice: the same state gives
the same choice.

1. A level: the stat of its weapon.
2. In battle: heal below `healBelowPercent`, a boost in round 1, leave below
   `fleeBelowHpPercent`; otherwise strike the weakest enemy.
3. A question to answer (any bot, and guest 0 in the soak): the answer with the higher chance.
4. Hurt below `fleeBelowHpPercent`: heal, else move away from monsters and the Taken.
5. After midnight (company), when above `huntHpPercent`: attack the Taken in the room; else a
   minion in the room; else walk to the nearest minion; else, with a friend within
   `huntNearSteps`, toward the Taken (where any company light shows it, else the under-stage);
   otherwise toward the nearest guest.
6. The Taken: its power on the weakest guest it can reach; attack or approach a lone guest; keep
   away from `crowd` or more guests together.
7. Purpose (the evening), recomputed every action from what the guest knows:
   - pick up healing, or a keepsake its own goal or a known group member's goal needs;
   - an ally goal: `OpenChat` if the ally is the player, `Chat` if the ally is in the room;
     a monsters goal: attack a beatable monster in the room;
   - else walk the shortest path to the nearest target of its own goal (the goal room once opened;
     the keepsake on its room's floor, or a holder only while lit; the ally while lit; an unresolved
     event room; a lit beatable monster), then of a known group member's keepsake.
8. A chat with the player: when `chatty` is 5 or more, or the bot likes the player.
9. Otherwise the best score: new rooms `explore`, floor items `loot` x `greed` (greed 0: none),
   beatable monsters `fight`, unused bonus rooms `bonus`, minus `avoidGuests` for walking toward a
   disliked guest. If that is to wait, a bot with `chatty` above 0 talks with someone in the room.

In the evening no bot steps back into the cell it just left during the same turn.

## Running it

```
UmbraTool night-check [<data folder>]
UmbraTool night-run [--data <folder>] [--seed 1] [--player <guest id>] [--json]
UmbraTool night-soak [--data <folder>] [--nights 1000] [--seed 1] [--strategy 0] [--json]
```

`night-soak` exits 1 when the data does not hold: a fault, or a form with 50 or more hunts
outside `rules.json` `balance.companyWinMin..Max`. Try a variant without touching the shipped
data: copy the folder, edit the copy, pass `--data <copy>`. Strategy 1: the company leaves
minions alone (must lose clearly more often).

## What moves which line of the report

| Report line | Moved by |
|---|---|
| company wins by form | `guests.csv` `takenHp`, `takenMight`, `takenSanity`, `takenSpeed`; that form's entry in `powers.json` and `minions.csv` |
| monster kills, levels | `monsters` in `rules.json` (chance, max, from which round), `monsters.csv` HP and growth, `levels.xp` |
| minion kills, and the gap to strategy 1 | `minions.csv` (HP, stats, `hpShare`, `count`), `styles.csv` `huntHpPercent` |
| downs, deaths | `events.csv` fail amounts, `monsters.csv`, the Taken forms and minions |
| guest 0's side won | everything above |
| the Taken in % of nights | the goals: whoever fails is drawn, so harder goals for a guest make it the Taken more often |
| all finished (no hunt) | every goal's difficulty together |
| rooms open by midnight (the foyer and start rooms count) | `styles.csv` `explore`, guests' `speed`; chats and goal walks lower it |
| evening rounds with a chat between bots | `styles.csv` `chatty` (6 or more: a chat before moving; any above 0: walks toward lit guests and talks instead of waiting) |
| goals finished, per goal | room goals: `goals.roomGoalRooms`; keepsakes: the item's `room` in `items.csv` (plain rooms are harder than rooms that stop movement), `goals.earlyRooms`; events / monsters: the goal's `count`, event rooms' `count` in `rooms.csv`, `monsters` in `rules.json`; ally goals: `questions.csv` targets and `bonds.csv` of the asker |

## Tuning order

1. All forms' `takenHp` together until the hunt is near 50% overall.
2. Each form (its stats, then its power's numbers) until it is inside the band. A power's numbers
   matter less than the form's stats: change the stats first when a form is far out.
3. Monsters and level steps. More monsters also help the Taken after midnight (they take its
   side), so the forms need retuning afterwards.

Check on more than one seed range (`--seed 1`, `--seed 20001`, `--seed 50001`): a form's share
moves about 3 points either way between ranges of 2,000 nights.

Goals of guest 0, and ally goals that name guest 0, are reported but not banded: in the soak it
is the player's seat, which bots treat as the player (they ask it questions first).

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
