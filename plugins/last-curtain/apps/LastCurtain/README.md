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
`--watch` lets all seven play themselves, `--seed N` picks the night, `--capture shot.png --frames N`
saves frame N and quits.

## Playing

Your turn opens with a menu (Move, fights, chats, Use item, End turn). A click on a lit square
walks there one sliding step at a time (Move first shows the path: gold for the steps you can pay
for, red beyond); the arrows take one step. A closed door asks Stay (open it) or Back. I opens the
bag; Enter ends the turn from the menu (moves may be left); the wheel zooms; Space shortens the
bots' turns; Esc quits. Quitting mid-night saves to
`<preferences>/last-curtain/night.json`; the start screen offers Continue.

## Files

| File | What |
|---|---|
| `Main.cpp` | window, renderer, the loop, the start screen, saves |
| `Play.*` | the night as the person plays it, without a window (tested in `tests/PlayTests.cpp`) |
| `Table.*`, `Table.md` | the board drawn as boxes, the light and the camera |
| `Screens.*` | `engine/ui` set up for this program |
| `assets/ui/` | the screens, skin and fonts (here, not in the plugin's `assets/`, which the Umbra game also loads) |
