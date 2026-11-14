# The Last Curtain, plan 2c: the screen — verification record

## Record

- Date: 2026-11-15
- Commits: `issue #UMBRA-146` (the plan) and `#UMBRA-147` … `#UMBRA-156` (#156: these records) (built in a cloud copy of
  the checkout; delivered to the PC with `git am`). Design `docs/design/2026-11-14-goals-and-bigger-board.md`;
  plan `docs/design/plans/2026-11-14-goals-plan-2c.md`.
- Tester: Claude (remote session), Linux cloud workspace (GCC 13, Clang 18, GCC ASan+UBSan), every
  dependency built from source; pictures with SwiftShader (CPU Vulkan) under `xvfb-run`. MSVC, a
  real GPU, a mouse and a person playing have not run this yet.
- State: **implemented**; the Windows play check (§4) is open.
- User decisions (2026-11-15): the plan as written; no ally goals (an ally is never a personal
  goal), done first as task 0.

## What was built

| Part | Where |
|---|---|
| No ally goals; four new goals; bots and balance re-measured | `#UMBRA-147`, `goals.csv`, `NightBots.cpp`, `Balance.md` |
| Walls on every side a guest cannot pass | `#UMBRA-148`, `BoardView` `walls`, `Table.cpp` |
| Paths to a lit square (`PathTo`) | `#UMBRA-149`, `modules/board`, `Board.md` |
| The turn menu, picking, walking with slides, Stay/Back | `#UMBRA-150`, `Play.*`, `menu`, `picking`, `confirm` screens |
| Input by state, inspect, the save menu | `#UMBRA-151`, `inspect`, `pause` screens, `Main.cpp` |
| Bots think (max(0.4 s, the decision's time)) and slide | `#UMBRA-152` |
| The camera follows; drag to look; Focus | `#UMBRA-153`, `Table::Pan`, the HUD's Focus button |
| Labels over pawns ("thinking...", group numbers), coloured group rings, the goal's progress, the "told" caption | `#UMBRA-154`, `labels` screen, `Table::ScreenPoint` |
| Final review: an arrow with no move left asks nothing | `#UMBRA-155` |

## 1. Automated checks (T123-T129)

| Check | Result |
|---|---|
| Linux GCC / Clang / GCC ASan+UBSan: every CTest of the project (`verify.sh`) | **55/55** each |
| `Umbra.Night`: 165 cases, 372,512 assertions | pass |
| `Umbra.Night.Balance` (1,000 nights, every band; going for minions pays) | pass |
| Walls: a door facing a wall, a sealed passage; doorways are not walls (T123) | pass |
| Paths: cardinal steps through passages, through a doorway at the end; none to a dark, walled-off, barred or own cell (T124) | pass |
| A click 3 squares away walks 3 sliding steps, 1 move left; a square too far is ignored and its path red past the moves; illegal squares ignored; Stay/Back at a door; a dialog mid-walk drops the rest; dt 0 freezes; Continue mid-walk opens the menu (T125) | pass |
| Every cell of the input table (Esc, arrows, Enter, left and right click in the menu, picking, confirm, a dialog, a bot's turn); inspect shows only what the player may know (T126) | pass |
| A seen bot thinks 0.4 s / 0.9 s for a 0.1 s / 0.9 s decision, once a turn; an unseen bot does not; a bot's step slides (T127) | pass |
| The camera follows each step; a drag stops following until Focus; a dead player still follows beats, Focus and Esc are safe (T128) | pass |
| Rings numbered by group, the player's first; the goal's progress "1 of 2"; a passed answer tells the goal (T129) | pass |
| `UmbraTool ui-preview` of `menu`, `picking`, `confirm`, `inspect`, `pause`, `labels`, `hud` with `sample.json` | 0 errors, 0 warnings |

## 2. Commands and results

```
bash /home/claude/verify.sh                                   # gcc, clang, asan: BUILD OK, 55/55 each
UmbraTool night-soak --nights 2000 --seed 1 / 20001 / 80001     # "the data holds" on each
VK_ICD_FILENAMES=<swiftshader icd> xvfb-run -a LastCurtain --seed 3 --player nhi --frames 40 --capture board.png
VK_ICD_FILENAMES=<swiftshader icd> xvfb-run -a LastCurtain --seed 3 --watch --frames <n> --capture watch.png
```

Soak after task 0, seed 1, 2,000 nights: the hunt 52%, every Taken form 46-60% over the three seed
ranges; rooms by midnight 49; bot-to-bot chats in 73-75% of evening rounds; every banded goal
47-62%. The plan's later tasks do not touch the rules, and the golden fingerprint did not move
after task 0.

The pictures show walls on every closed side, the HUD with the goal, a bot's caption, and the
labels screen drawing nothing over the board when there is nothing to say. SwiftShader's first
frames are slow (a frame can be longer than the 0.4 s pause), so no picture caught "thinking...";
the pause is covered by T127 and is on the Windows checklist. These are not a GPU check.

`verify.sh` ran before the last fix (`#UMBRA-155`): GCC, Clang and ASan 55/55; the GCC build and all
55 CTests were run again after it: 55/55.

## 3. Things to look at while playing

- Board clicks now happen when the left button is let go without a drag (a press may become a drag).
- "Save and quit" goes back to the start screen (Continue is offered); the window's close button quits.
- The thinking pause shows only for bots you can see, once a turn; inside a battle or a chat there is none.
- New goals and the chattiness numbers are fill-ins [call] (`cast.md`, `Balance.md` tuning record).

## 4. Windows check (open)

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-release --target LastCurtain UmbraNightTests UmbraTool
ctest --preset windows-msvc-release -R Umbra.Night
& ".\out\build\windows-msvc\plugins\last-curtain\apps\LastCurtain\Release\LastCurtain.exe"
```

- [ ] MSVC builds `LastCurtain`, `UmbraTool` and the plugin without warnings; `Umbra.Night` and `Umbra.Night.Balance` pass
- [ ] A full night as one guest, from the start screen to the end screen
- [ ] A goal finished and one failed (the HUD's "Your goal", its progress, "Done")
- [ ] A bot-to-bot recruit seen ("X tells Y what they came for", "X joins Y", numbered rings)
- [ ] Walking: a click on a lit square slides step by step; a far square shows red and is ignored; a closed door asks Stay/Back
- [ ] The camera follows a walk and the bots' beats; a drag frees it; Focus brings it back
- [ ] The save menu (Esc from the menu or a bot's turn): Save, Resume, Save and quit; Continue brings the night back
- [ ] Esc in each state does what the input table says; right-click inspects a room and a guest
- [ ] "thinking..." over a bot you can see before its turn
- [ ] Walls: no gap shows where a guest cannot pass
