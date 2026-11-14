# The Last Curtain, plan 1: the night as a simulation — verification record

## Record

- Date: 2026-11-14
- Commits: `issue #UMBRA-104` … `issue #UMBRA-115` (built in a cloud copy of the checkout at
  `#UMBRA-103`; the user commits them on the PC). ADR 0012; plan
  `docs/design/plans/2026-11-14-night-simulation.md`.
- Tester: Claude (remote session), Linux cloud workspace (GCC 13, Clang 18, GCC ASan+UBSan), every
  dependency built from source (`docs/testing/linux-headless-build.md`). MSVC has not built this yet.
- State: **implemented**; the Windows check (§4) is open.
- User decisions (2026-11-14): the night has its own small data-driven battle rules (Might and
  Sanity as two damage types, the starter first, 3 rounds); the plan runs task by task in one session.

## What was built

| Part | Where |
|---|---|
| Plugin, data files, loader with file/row/column diagnostics | `plugins/last-curtain` (`plugin.json`, `assets/night/*`, `modules/night` `NightDef.hpp`, `src/NightData.cpp`, `src/Csv.cpp`) |
| The night: board, turn, checks, battles, company, omens, the Turn, the hunt, seven powers | `modules/night` (`Night.hpp`, `src/*.cpp`); `include/Night.md` |
| Saves and fingerprint | `NightSave.hpp`, `src/NightSave.cpp` |
| Bots and the soak test | `modules/nightbots` (`NightBots.hpp`, `NightSoak.hpp`); `include/Balance.md` |
| Tools | `UmbraTool night-check`, `night-run`, `night-soak` (`plugins/last-curtain/tool/NightCommand.cpp`) |
| Tests | `plugins/last-curtain/tests` (CTest `Umbra.Night`, and `Umbra.Night.Balance` for the slow balance case) |

## 1. Automated checks (T94-T105)

| Check | Result |
|---|---|
| Linux GCC: every CTest of the project | **55/55** (incl. `Umbra.Night` 66 cases, `Umbra.Night.Balance`) |
| Linux Clang / GCC ASan+UBSan | **55/55 / 55/55** (same golden fingerprint on GCC and Clang; no sanitizer reports) |
| Data loads; ten kinds of bad data refused with file, row and column | pass |
| Board connected and never sealed on 500 + 300 seeds; under-stage 3+ steps out; room counts kept | pass |
| Order, movement, rooms that stop you, bonus once, transport pair, items, 20 rounds | pass |
| Checks, events once, traps down not dead, relics, item rooms | pass |
| Damage formula from data, starter first, 3 rounds, buffs, XP and levels, monsters | pass |
| Guest fights (slow, no XP, seen), persuading, allies, bystanders | pass |
| Omens: gift, rising target with pre-gift Sanity, secret mark, the early win | pass |
| The Turn, chance mark, full-strength return, death after midnight, contact battles, three endings | pass |
| Seven powers both ways, cooldown, the seal never cuts the company off, numbers from data | pass |
| Same seed same night (golden fingerprint), saves at any point (mid-battle, the hunt, a pending level), damaged or other-data saves refused | pass |
| Bots finish 300 nights; use only what they could see; hunt what they saw; no attack without cause; styles differ; heal or flee | pass |
| Soak report adds up; 1,000 nights without faults; every guest marked 20+ times; 12+ rooms a night | pass |
| The shipped data is balanced (1,000 nights) and the strategies (500 nights each) | pass |

Bugs found and fixed, each with a test that failed first: a battle whose starter fell left the
board stuck; the under-stage could be missing at midnight (now opened at the Turn); after a guest
chose a level the next guest started with no moves (found in the final review); neighbouring
seeds gave alike nights (splitmix64 on the seed).

## 2. Commands and results

```bash
TAIL=6 /home/claude/build.sh gcc      # BUILD OK, 100% tests passed, 0 tests failed out of 55
bash /home/claude/verify.sh           # gcc, clang, asan: see below
UmbraTool night-check                 # 11 file(s): 0 error(s), 0 warning(s)
UmbraTool night-soak --nights 1000    # "the data holds", exit 0
```

verify.sh: gcc, clang and asan each `BUILD OK`, `100% tests passed, 0 tests failed out of 55`; no new warnings in
`plugins/last-curtain`. The sanitizer run found tests that parked guests on unopened cells (an
out-of-range read); the tests now use a doorless room, and saves with a guest on an unopened cell are refused.

## 3. The first balance pass (`night-soak`, seed 1)

Changes made, only in `assets/night/`, in this order:

1. Powers (`powers.json`): line 8 → 10; flood 3 → 2; pull 6 dmg, range 3, every turn → 2, 2, every
   3rd turn; reflect every turn → every 3rd.
2. Taken forms (`guests.csv`, HP / Might): Folded Girl 90 → 83; Opera Specter 90 → 71; Drowned
   Physician 90 → 69; Iron Debtor 90 / 11 → 56 / 5 (the pull mattered less than the form's Might);
   Rooted Caretaker 90 → 81; the Lens 90 / Sanity 8 → 98 / 11; Silvered Reflection 90 → 54.
3. Omens: no change needed (refusing every omen is not better than the bot; taking every omen
   keeps the hunt at 50%).
4. Monsters and levels (`rules.json`): monsters from round 2 (was 3), 50% a round (was 35); level
   steps 1, 3, 6, 10 kills (was 2, 5, 9, 14). More monsters also strengthen the Taken after
   midnight, so step 2 was redone after this.
5. Bot rule (code, `NightBots.cpp`): a bot takes an omen while `omen * 2 + Sanity` reaches the
   curse target; before, the protector never took one and the Drowned Physician never appeared.

The report after the pass:

```
1000 nights, 0 faults
won before midnight 281 (28%), company won the hunt 353, the Taken won 366; guest 0's side won 64%
per night: rooms 16.9, omens 2.9, monster kills 2.7, levels 2.2, downs 0.8, deaths 2.0, guest attacks 1.2
  The Folded Girl (line): 48% of 156 hunts      The Opera Specter (area): 47% of 113
  The Drowned Physician (flood): 53% of 56      The Iron Debtor (pull): 43% of 72
  The Rooted Caretaker (seal): 47% of 63        The Lens (swap): 50% of 139
  The Silvered Reflection (reflect): 52% of 120
the data holds
```

Other seed ranges (2,000 nights each): seeds 1-2000: 48-53% for every form; seed 20001: 44-50%;
seed 50001: 44-56%.

Strategies for guest 0 (500 nights; "guest 0's side won"): its bot 63%; attacking the nearest
guest every turn 49%; refusing every omen 57%; taking every omen 63% (the company wins 48% of
those hunts).

Open points for the user (not decided here):

- Nearly 3 nights in 10 end before midnight (a seen failure, then the hunters bring the marked
  guest down). If the hunt should be the usual ending, `omens.targets` or `light.steps` move it.
- Level 5 "in about 15 fighting turns" is not reached: a whole table kills about 2.7 monsters a
  night. More monsters make the hunt harder, so this needs a decision (more evening monsters, or
  monsters that leave at midnight, which would be a new rule).

## 4. Windows check (open)

From an x64 Developer PowerShell:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release -R Umbra.Night
& ".\out\build\windows-msvc\tools\umbratool\Release\UmbraTool.exe" night-check
& ".\out\build\windows-msvc\tools\umbratool\Release\UmbraTool.exe" night-soak --nights 1000
```

- [ ] MSVC builds the plugin without warnings
- [x] `Umbra.Night` and `Umbra.Night.Balance` pass (the golden fingerprint must match: same night on MSVC) (user, 2026-11-14, MSVC Release: 66 cases / 215,434 assertions, and the balance case, all passed)
- [x] `night-soak --nights 1000` prints the same numbers as §3 and "the data holds" (user, 2026-11-14, MSVC Release: every line identical to the Linux run, 1,002 ms)

## 5. Revision `#UMBRA-116`: the curse turns at once; minions (2026-11-14, evening)

User decisions after the first pass: no hidden marked guest; a failed curse roll turns that
guest at once and everyone wakes at midnight; the roll is a plain d20 (same odds for all); with no
failure by midnight chance picks an omen holder, all alike, else anyone; no fights between guests
before midnight; after midnight a fixed set of the form's own minions (one kind per form) replaces
ordinary monsters, and killing one takes a share of the Taken's HP.

Built: `TakeOmen` / `TurnNow` / `CurseTarget` (`Company.cpp`, `Hunt.cpp`), `minions.csv` (loaded
after the monsters, `MonsterDef::taken`, `hpShare`, `count`), minions placed in the rooms farthest
from the foyer at the Turn, the HP share in `Battle.cpp`; cut: `Knows`, the early win, evening
attacks between guests, bystanders joining, the `temper` column, `attackedBy`, `sawOmen`/`sawFail`.
Bots go for minions first; soak strategy 1 is now "the company leaves minions alone".

Data changed: `omens.targets` 6, 9, 12, 15, 18 (+ Sanity) → 1, 1, 1, 2, 3 + `perHour` 2 (plain d20);
`minions.csv` new (HP 8-9, Might/Sanity 2-3, 25% of the Taken's HP each, 3 per form); Taken HP:
Folded Girl 96, Opera Specter 60, Drowned Physician 82, Iron Debtor 94 (Might 7), Rooted Caretaker
60, the Lens 90, Silvered Reflection 68.

```
1000 nights, 0 faults
company won the hunt 488 (48%), the Taken won 512; guest 0's side won 47%
the curse struck early in 13% of nights, on average in round 3.9 of 10; otherwise chance chose at midnight
per night: rooms 18.5, omens 3.1, monster kills 3.1, minion kills 0.8, levels 3.1, downs 0.2, deaths 2.6
  Folded Girl 53% (198 hunts), Opera Specter 47% (170), Drowned Physician 50% (95), Iron Debtor 51% (77),
  Rooted Caretaker 47% (105), the Lens 45% (192), Silvered Reflection 46% (163); the data holds
```

Seeds 20001 and 50001 (2,000 nights each): every form 42-57%. Strategies (500 nights): the bots
49%; the company leaving minions alone 42% (hunting them is worth 7 points); guest 0 refusing every
omen 48%; taking every omen 49%.

Tests: 64 cases in `Umbra.Night` (new: no guest fights before midnight, the curse target rising
with omens and hours, a failed roll turning at once, chance among holders all alike, minions
placed and ordinary monsters gone, a minion's HP share, bots going for minions; removed: the
cases of the cut rules) and the balance case (now also: hunting minions pays 5+ points; the curse
strikes early in at most 25% of nights, not before round 3 on average). The new tests were written
alongside the code, not watched failing first.

Windows (user, 2026-11-14, MSVC Release): `UmbraNightTests.exe` 64 cases / 151,881 assertions and the
balance case passed (so the golden fingerprint matches MSVC). `night-soak --nights 1000`
then printed every line of the block above, identical to Linux, "the data holds" (1,594 ms). (The
first attempt failed to link in an x86 developer shell: build from an x64 shell.)

Level 5 out of reach (about 3 level-ups a night for the whole table) is intended (user, 2026-11-14).

## 6. Plan 2 rules: pushed back, and talking (`#UMBRA-121`, `#UMBRA-122`, 2026-11-14)

Events may push a guest back to the room they came from (`back`; `cold` and `bell` fail that
way now). The single persuasion roll is replaced by the conversation rule (design
`2026-11-14-board-on-screen.md`): 35 questions in `questions.csv`, `persuade` = 3 approvals and a
bond of 3, `styles.csv` `chatty`. In the soak guest 0 is the player, so chatty bots ask guest 0
questions and it gains allies; the company got stronger and the forms were retuned (Taken HP: Folded
Girl 100, Drowned Physician 90, Iron Debtor 106, Rooted Caretaker 64, Silvered Reflection 74).

```
1000 nights, 0 faults
company won the hunt 507 (50%), the Taken won 493; guest 0's side won 48%
the curse struck early in 15% of nights, on average in round 3.8 of 10
seeds 1 / 20001 / 80001 (2,000 each): every form 42-54%; seed 50001: the Iron Debtor 62% (109 hunts)
strategies (500): bots 50%; leaving minions alone 40%; refusing omens 47%; taking every omen 50%
```

The Iron Debtor has the fewest hunts (Khang rarely takes omens) and swings most between seed
ranges; the shipped test range holds.
