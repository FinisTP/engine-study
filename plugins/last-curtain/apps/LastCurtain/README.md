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
first turn, `--match` starts a faction match at once (no café first), `--capture shot.png --frames N` saves frame N and quits. For designers, `--stage <id> [--refill n] [--seed n]` plays one campaign stage
(`assets/campaign/stages/<id>.json`; the party is the player, Bảo and Bà Mai up to its cap, at its level)
with its scenes at their moments (`StageApp.hpp`). For writers, `--scene <id>
[--backdrop <name>] [--name <text>] [--gender male|female]` plays one campaign scene
(`assets/campaign/scenes/<id>.json` of the plugin) on the dialog screen over `art/backdrops/<name>.jpg`:
Space or a click goes on, 1–4 choose, Esc skips (it stops at a choice), Nhật ký shows the lines so far.

The built-in model (plan 3): put the model file at `models/night.gguf` here (git-ignored) and build
with the vcpkg feature `town-model-gpu`; without it the night uses its written lines. The pictures
(plan 4a) are in `assets/art/`, one per room, event, guest, item and monster (`Art.md`).

## Playing

Everything is done with the mouse; keys are shortcuts. Your turn opens with a menu (Move, Room
view, fights, chats, Use item, End turn). **Room view** (or the HUD button; on your own turn only) switches between the
table and every opened room at full size, zooming in; play on in either: click a room to walk there
(Move first shows the path: gold for the steps you can pay for, red beyond), in the room view click
a guest to talk (or fight, after midnight), a monster card to fight it, an item on the floor to take
it. Moves count as the shortest way back to where your turn began, so walking back is free; opening
a room, or a room that stops you, makes the moves final. A closed door asks Stay (open it) or Back.
An event, trap or item room plays step by step at the side of the screen: the room, the card, a
tumbling die, the result in words; then you may look around before End turn. Drag to look, the wheel
zooms; a click on the portrait in the top bar brings the camera back to whoever acts. A gold arrow bobs over whoever's turn it is
(the top bar shows their stats); the bottom-left panel tells what waits in your room and how to
clear it. On a bot's turn, a deed in your sight shows its room and waits for a click (or Space);
Skip runs the bots to your turn. A right-click inspects a room. Esc goes back one step; from the
menu or a bot's turn it opens the save menu (Save, Resume, Save and quit). Quitting mid-night saves
to `<preferences>/last-curtain/night.json`; the start screen offers Continue.

## Files

| File | What |
|---|---|
| `Main.cpp` | window, renderer, the loop, the start screen, saves |
| `Play.*`, `Play.md` | the night as the person plays it, without a window: the room's flow, rest, bots' beats (tested in `tests/PlayTests.cpp`) |
| `Table.*`, `Table.md` | the board drawn as boxes, the light and the camera |
| `Options.*`, `assets/options.csv` | the pace, the waits and the camera, for design (`key,value,note`; a bad row keeps the defaults) |
| `Screens.*` | `engine/ui` set up for this program |
| `assets/ui/` | the screens, skin and fonts (here, not in the plugin's `assets/`, which the Umbra game also loads) |
