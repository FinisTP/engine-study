# Bots, the soak test and how to balance

Balancing means editing files under `plugins/last-curtain/assets/night/` and running the soak
test again. Code changes only when a new kind of mechanic is needed.

## The bots

`ChooseAction(def, state, guest)` follows rules, not the model. No dice: the same state gives
the same choice.

1. A level: the stat of its weapon.
2. In battle: heal below `healBelowPercent`, a boost in round 1, leave below
   `fleeBelowHpPercent`; otherwise strike the weakest enemy.
3. A question (guest 0 in the
   soak): the answer with the higher chance. Opening a chat with the player: when `chatty` is 5 or
   more, or the bot likes the player.
4. Hurt below `fleeBelowHpPercent`: heal, else move away from monsters and the Taken.
5. After midnight (company), when above `huntHpPercent`: attack the Taken in the room; else a
   minion in the room; else walk to the nearest minion; else, with a friend within
   `huntNearSteps`, toward the Taken (where any company light shows it, else the under-stage);
   otherwise toward the nearest guest.
7. The Taken: its power on the weakest guest it can reach; attack or approach a lone guest; keep
   away from `crowd` or more guests together.
8. Otherwise the best score: new rooms `explore`, floor items `loot`, beatable monsters `fight`,
   unused bonus rooms `bonus`, minus `avoidGuests` for walking toward a disliked guest.

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

## Tuning order

1. All forms' `takenHp` together until the hunt is near 50% overall.
2. Each form (its stats, then its power's numbers) until it is inside the band. A power's numbers
   matter less than the form's stats: change the stats first when a form is far out.
3. Monsters and level steps. More monsters also help the Taken after midnight (they take its
   side), so the forms need retuning afterwards.

Check on more than one seed range (`--seed 1`, `--seed 20001`, `--seed 50001`): a form's share
moves about 3 points either way between ranges of 2,000 nights.
