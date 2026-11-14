# Bots, the soak test and how to balance

Balancing means editing files under `plugins/last-curtain/assets/night/` and running the soak
test again. Code changes only when a new kind of mechanic is needed.

## The bots

`ChooseAction(def, state, guest)` follows rules, not the model, and reads only what its guest can
know: its light, what it saw (`Knows`), and whether it is marked itself. No dice: the same state
gives the same choice.

1. A level: the stat of its weapon. A bystander: joins the defender if it likes them better.
2. In battle: heal below `healBelowPercent`, a boost in round 1, a fleeing temper leaves a fight
   with a guest, anyone leaves below `fleeBelowHpPercent`; otherwise strike the weakest enemy.
3. An omen: take it while `omen * 2 + Sanity` reaches the current curse target.
4. Hurt below `fleeBelowHpPercent`: heal, else move away from monsters and attackers.
5. Before midnight: hunt a guest it saw fail; hide (keep away from everyone) if marked and seen;
   answer an attacker in the room by temper (`fight` strikes back, `flee` moves away).
6. After midnight (company): attack the Taken in the room when above `huntHpPercent`; move toward
   it (where any company light shows it, else the under-stage) with a friend within
   `huntNearSteps`; else toward the nearest guest.
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
data: copy the folder, edit the copy, pass `--data <copy>`. Strategies swap guest 0's chooser:
1 attacks the nearest guest every turn, 2 refuses every omen, 3 takes every omen.

## What moves which line of the report

| Report line | Moved by |
|---|---|
| company wins by form | `guests.csv` `takenHp`, `takenMight`, `takenSanity`, `takenSpeed`; that form's entry in `powers.json` |
| won before midnight | `omens.targets` (more failures, more marks), `light.steps` (more witnesses), `styles.csv` `omen` |
| omens per night | `styles.csv` `omen`, `omens.targets` |
| monster kills, levels | `monsters` in `rules.json` (chance, max, from which round), `monsters.csv` HP and growth, `levels.xp` |
| downs, deaths | `events.csv` fail amounts, `monsters.csv`, the Taken forms |
| guest attacks | `styles.csv` thresholds, `light.steps` |
| guest 0's side won (by strategy) | everything above; the strategies check that attacking without cause and refusing omens are not winning plans |

## Tuning order

1. All forms' `takenHp` together until the hunt is near 50% overall.
2. Each form (its stats, then its power's numbers) until it is inside the band. A power's numbers
   matter less than the form's stats: change the stats first when a form is far out.
3. Omens (`omens.csv`, `omens.targets`) for the omen strategies.
4. Monsters and level steps. More monsters also help the Taken after midnight (they take its
   side), so the forms need retuning afterwards.

Check on more than one seed range (`--seed 1`, `--seed 20001`, `--seed 50001`): a form's share
moves about 3 points either way between ranges of 2,000 nights.
