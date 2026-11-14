# The Last Curtain, plan 2b: goals and a bigger board — verification record

## Record

- Date: 2026-11-15
- Commits: `issue #UMBRA-137` … `issue #UMBRA-145` (built in a cloud copy of the checkout; delivered to
  the PC with `git am`). Design `docs/design/2026-11-14-goals-and-bigger-board.md`; plan
  `docs/design/plans/2026-11-14-goals-plan-2b.md` (the rules; plan 2c, the screen, comes next).
- Tester: Claude (remote session), Linux cloud workspace (GCC 13, Clang 18, GCC ASan+UBSan), every
  dependency built from source (`docs/testing/linux-headless-build.md`); pictures with SwiftShader
  (CPU Vulkan) under `xvfb-run`. MSVC, a real GPU and a person playing have not run this yet.
- State: **implemented**; the Windows check (§4) is open.
- User decisions (2026-11-14/15): omens removed; each guest has two or three goals and draws one a
  night; at midnight the Taken is drawn from the guests who failed, the other failed guests drop to
  1 HP, and if everyone finished the company wins with no hunt; goals are secret and a passed chat
  answer reveals them; groups help their members; a 9×9 board of 70 rooms; seven start rooms, one per
  guest; bots chat with and recruit each other; a free step back but no pacing; every goal finished
  in 45-65% of nights; goal rooms dealt within the first 65 rooms (2026-11-15, after the soak showed
  room goals at 95%).

## What was built

| Part | Where |
|---|---|
| Omens removed; their rooms retyped | `#UMBRA-137` |
| 9×9 board, 70 rooms, a deck of 85 | `rules.json`, `rooms.csv` (`#UMBRA-138`) |
| Goals: `goals.csv`, keepsakes in `items.csv`, the draw at midnight | `modules/night` (`#UMBRA-139`) |
| Seven start rooms; goal and keepsake rooms dealt early | `Board.cpp` (`#UMBRA-140`, windows `earlyRooms` 35 and `roomGoalRooms` 65) |
| A free step back; the left room barred for the turn | `Turn.cpp` (`#UMBRA-141`) |
| Chats between any two guests; groups of up to three; goals told on a pass | `Company.cpp` (`#UMBRA-142`) |
| Bots with a purpose: goal targets, `greed`, chats, no stepping back | `NightBots.cpp`, `Balance.md` (`#UMBRA-143`) |
| The soak measures every band; the data tuned | `NightSoak.cpp`, `assets/night/` (`#UMBRA-144`) |
| Save version 2; the HUD shows your goal and the goals you know | `NightSave.cpp`, `Play.cpp`, `hud.ui.json` (`#UMBRA-145`) |

## 1. Automated checks (T116-T122)

| Check | Result |
|---|---|
| Linux GCC / Clang / GCC ASan+UBSan: every CTest of the project (`verify.sh`) | **55/55** each |
| `Umbra.Night`: 134 cases, 359,342 assertions | pass |
| `Umbra.Night.Balance` (1,000 nights, every band; going for minions pays) | pass |
| Board: 70 rooms fit; seven start rooms, one guest each; keepsake rooms within 35, goal rooms within 65 and sometimes after 35 (T116, T118) | pass |
| Goals: kinds, progress, the midnight draw, the company win with no hunt, 1 HP for the other failed (T117) | pass |
| A step back is free, the room left is barred; no pacing (T119) | pass |
| Two bots chat; three passes make a group; both learn the other's goal; groups hold three (T120) | pass |
| Bots: never step back in a turn; walk toward a known goal room; greed 0 leaves a worn item; an ally goal naming the player opens a chat; help a member's keepsake; ignore a holder they cannot see (T121) | pass |
| Bots never recruit the player through Chat (they ask through OpenChat); the player's allies cannot be won away (final review) | pass |
| A mid-evening save keeps goals, groups, approvals and the undo; out-of-range values and version-1 saves are refused; the HUD carries the player's goal (T122) | pass |
| `UmbraTool ui-preview hud actions` with `sample.json` | 0 errors, 0 warnings |

## 2. Commands and results

```
bash /home/claude/verify.sh                                   # gcc, clang, asan: BUILD OK, 55/55 each
UmbraTool night-soak --nights 2000 --seed 1 / 20001 / 80001     # "the data holds" on each
UmbraTool ui-preview hud actions --assets plugins/last-curtain/apps/LastCurtain/assets \
    --data plugins/last-curtain/apps/LastCurtain/assets/ui/sample.json --out hud.png
VK_ICD_FILENAMES=<swiftshader icd> xvfb-run -a LastCurtain --seed 3 --player nhi --frames 30 --capture board.png
```

Soak, seed 1, 2,000 nights: the company wins 51% of hunts, every Taken form 45-56%; each guest is
the Taken in 8-19% of nights; 1% of nights end with everyone's goal done; 47.3 rooms open by
midnight (the foyer and start rooms count); 79% of evening rounds have a chat between bots; every
banded goal is finished in 49-62% of the nights it is drawn. Seeds 20001 and 80001 are within a few
points. The tuning steps are in `modules/nightbots/include/Balance.md` ("Tuning record").

The pictures (the board with the goal panel and a bot-to-bot caption; the HUD with known goals) came
out as expected on SwiftShader. They are not a GPU check.

## 3. Things to look at while playing

- The table itself is unchanged until plan 2c: pawns still jump, the camera does not follow, and
  there is no move menu or confirm for a new room. Judge the rules, not the screen.
- Rooms by midnight sit near the floor of their band (47 of 45-65).
- Goals that hang on the player are not banded: Nhi's (she is the player's seat in the soak) and
  Bao's "have Nhi at his side". In the soak the player's group fills early (Mai and others ask
  Nhi first), so Bao finishes that goal in only about 7% of nights and Nhi-Mai in about 89%. In
  play this is the player's choice; say if Bao's goal should change.
- Six new events and the keepsakes' rooms (crown in the Interval bar, Ledger on the Service
  stairwell, mask in the Orchestra room) are fill-ins [call].

## 4. Windows check (open)

From the "x64 Native Tools Command Prompt for VS" (or a PowerShell after
`Set-ExecutionPolicy -Scope Process Bypass`):

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-release --target LastCurtain UmbraNightTests UmbraTool
ctest --preset windows-msvc-release -R Umbra.Night
& ".\out\build\windows-msvc\tools\umbratool\Release\UmbraTool.exe" night-soak --nights 2000
& ".\out\build\windows-msvc\plugins\last-curtain\apps\LastCurtain\Release\LastCurtain.exe"
```

- [ ] MSVC builds `LastCurtain`, `UmbraTool` and the plugin without warnings
- [ ] `Umbra.Night` and `Umbra.Night.Balance` pass; `night-soak` says "the data holds" with the
      same numbers as Linux
- [ ] Play a night: the HUD shows your goal; a chat you pass shows the asker's goal under "Goals you know"
- [ ] Bots talk with each other (captions "X talks with Y"; "Y joins X" when one is won over)
- [ ] Midnight: someone who failed turns; if you failed and were not drawn, you drop to 1 HP
- [ ] An old save (plan 2) is refused and Continue is hidden
