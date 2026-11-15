# LastCurtain.exe

The Last Curtain played on a table: one night in the Saigon Opera House, the player as one guest,
the other six played by bots. Design: `docs/design/2026-11-14-board-on-screen.md`.

## Build and run (Windows, x64 developer shell)

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-release --target LastCurtain
& ".\out\build\windows-msvc\plugins\last-curtain\apps\LastCurtain\Release\LastCurtain.exe"
```

Options: `--player <guest id>` starts at once as that guest (ids in `assets/night/guests.csv`),
`--watch` lets all seven play themselves, `--seed N` picks the night, `--look` opens Look on your
first turn, `--capture shot.png --frames N` saves frame N and quits.

The built-in model (plan 3): put the model file at `models/night.gguf` here (git-ignored) and build
with the vcpkg feature `town-model-gpu`; without it the night uses its written lines. The pictures
(plan 4a) are in `assets/art/`, one per room, event, guest, item and monster (`Art.md`).

## Playing

Your turn opens with a menu (Move, Look around, fights, chats, Use item, End turn). Look around
shows your room at full size from a corner of the stalls: click the floor to walk, walk through a
doorway to take the passage, Back returns to the table. A battle you fight, or see fought, plays in
its room at full size. A click on a lit square
walks there one sliding step at a time (Move first shows the path: gold for the steps you can pay
for, red beyond); the arrows take one step. A closed door asks Stay (open it) or Back. I opens the
bag; Enter ends the turn from the menu (moves may be left); the wheel zooms. A gold arrow bobs over
whoever's turn it is; the bottom-left panel tells what waits in your room and how to clear it. On a
bots' turn, a beat where something happens in your sight shows its room at full size and waits for
Continue (or Space); Skip runs the bots to your turn. A right-click (or any click on a bot's turn) inspects a room and who is in it. Esc
goes back one step (a door: Back; picking: the menu; a dialog: closed); from the menu or a bot's
turn it opens the save menu (Save, Resume, Save and quit to the start screen). The start screen's
Esc quits. Quitting mid-night saves to
`<preferences>/last-curtain/night.json`; the start screen offers Continue.

## Files

| File | What |
|---|---|
| `Main.cpp` | window, renderer, the loop, the start screen, saves |
| `Play.*`, `Play.md` | the night as the person plays it, without a window: the room's flow, rest, bots' beats (tested in `tests/PlayTests.cpp`) |
| `Table.*`, `Table.md` | the board drawn as boxes, the light and the camera |
| `Options.*`, `assets/options.csv` | the pace, the waits and the camera, for design (`key,value,note`; a bad row keeps the defaults) |
| `Screens.*` | `engine/ui` set up for this program |
| `assets/ui/` | the screens, skin and fonts (here, not in the plugin's `assets/`, which the Umbra game also loads) |
