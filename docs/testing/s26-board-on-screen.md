# The Last Curtain, plan 2: the board on screen — verification record

## Record

- Date: 2026-11-14
- Commits: `issue #UMBRA-121` … `issue #UMBRA-132` (#132: fixes from the final review) (built in a cloud copy of the checkout; delivered to the
  PC with `git am`). Design `docs/design/2026-11-14-board-on-screen.md`; plan
  `docs/design/plans/2026-11-14-board-on-screen-plan.md`.
- Tester: Claude (remote session), Linux cloud workspace (GCC 13, Clang 18, GCC ASan+UBSan), every
  dependency built from source (`docs/testing/linux-headless-build.md`); pictures with SwiftShader
  (CPU Vulkan) under `xvfb-run`. MSVC, a real GPU and a person playing have not run this yet.
- State: **implemented**; the Windows check (§4) is open.
- User decisions (2026-11-14): a tabletop view first (walking later); a separate program
  `LastCurtain.exe`; the conversation rule (5 questions per guest, two answers with their own stat and
  target, 3 approvals over separate turns make an ally); End turn at any time; events roll one fixed
  stat and may push the guest back; battles in the real-size room are a later plan.

## What was built

| Part | Where |
|---|---|
| Push back; the conversation rule (`questions.csv`, approvals, bots that open chats) | `modules/night`, `modules/nightbots` (`#UMBRA-121`, `#UMBRA-122`) |
| What the player may see: light, steps, pawns; cells in metres; picking | `modules/board` (`BoardView.hpp`, `Board.md`) |
| The narrator: one action as a beat (camera, path, caption, result) | `modules/board` (`Narrate.hpp`), `assets/night/captions.json` |
| The program: window, table, camera, the loop, saves | `apps/LastCurtain` (`Main.cpp`, `Table.*`, `Table.md`, `README.md`) |
| The night as the person plays it, without a window | `apps/LastCurtain/Play.*` (library `Umbra::NightPlay`) |
| Screens: start, hud, actions, bag, caption, chat, event, dice, omen, battle, turn, end | `apps/LastCurtain/assets/ui/*.ui.json`, `sample.json` |
| Event stories | `assets/night/events.csv`, new `text` column |
| Mouse wheel | `engine/platform` `Input::Wheel()` |

## 1. Automated checks (T106-T115)

| Check | Result |
|---|---|
| Linux GCC / Clang / GCC ASan+UBSan: every CTest of the project (`verify.sh`) | **55/55** each |
| `Umbra.Night`: 102 cases, 153,353 assertions (adds T106 push back, T107 conversation, T108-T110 view and picking, T109 narration, T111-T115 play) | pass |
| A click or key does nothing outside the player's turn, off a step cell, or behind a dialog (final review) | pass |
| An event that pushes the guest back still names the room it happened in (final review) | pass |
| Allies are recruited guests only; an ally is the player's to move; a dead player only watches | pass |
| Bot beats hold 1.5 s (0.3 s after Space), 0.2 s unseen; dt 0 changes nothing | pass |
| Every chance on screen equals the rules (answers, events, the curse); the dice show the logged roll | pass |
| Battle data: both sides, expected damage, Leave from round 2; death mid-battle ends the screen | pass |
| Save and Continue give the same night; damaged or other-data saves hide Continue | pass |
| The Turn screen names the form, HP, power, minions; the player as the Taken plays on | pass |
| No chat or omen screen waits after the night ended | pass |
| `UmbraTool ui-preview` of all twelve screens with `sample.json` | 0 errors, 0 warnings |

## 2. Commands and results

```
bash /home/claude/verify.sh                         # gcc, clang, asan: BUILD OK, 55/55 each
UmbraTool night-soak --nights 1000                   # 50% company; every form 44-54%; "the data holds"
UmbraTool ui-preview hud actions --assets plugins/last-curtain/apps/LastCurtain/assets \
    --data plugins/last-curtain/apps/LastCurtain/assets/ui/sample.json --out turn.png
VK_ICD_FILENAMES=<swiftshader icd> xvfb-run -a LastCurtain --seed 3 --watch --frames 150 --capture watch.png
VK_ICD_FILENAMES=<swiftshader icd> xvfb-run -a LastCurtain --seed 3 --player lan --frames 30 --capture chat.png
```

The pictures (table, a caption, the chat, the start screen) came out as expected on SwiftShader. They are
not a GPU check.

## 3. Things to look at while playing

- Chatty bots open a chat with the player in round 1 almost every night, because everyone starts in the
  foyer; the first turns are often answers. Tune `chatty` in `styles.csv` if it feels too much.
- Seven pawns in one room overlap; spreading them by count is left for polish.
- The captions are plain templates (`captions.json`); plan 3 lets the model word them.

## 4. Windows check (open)

From the "x64 Native Tools Command Prompt for VS" (or a PowerShell after
`Set-ExecutionPolicy -Scope Process Bypass`):

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-release --target LastCurtain UmbraNightTests
ctest --preset windows-msvc-release -R Umbra.Night
& ".\out\build\windows-msvc\plugins\last-curtain\apps\LastCurtain\Release\LastCurtain.exe"
```

- [ ] MSVC builds `LastCurtain` and the plugin without warnings
- [ ] `Umbra.Night` and `Umbra.Night.Balance` pass
- [ ] Play a night from the start screen to the end screen as one guest
- [ ] Win a guest over by chat (three approvals, on separate turns)
- [ ] An event that pushes you back to the room you came from
- [ ] Take an omen and read the curse risk before you do
- [ ] A battle: strike, guard, leave from round 2
- [ ] See the Turn screen, then the hunt with the red light
- [ ] Quit mid-night, start again: Continue brings the same night back
- [ ] Resize and minimise the window during a bot turn; no beat is lost
- [ ] The wheel zooms; Space shortens the bots' turns; Esc quits from the board
