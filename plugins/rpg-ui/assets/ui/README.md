# Reference RPG screens

Screens of the `rpg-ui` plugin (format: `engine/ui/include/UI.md`). The game fills one data
object every frame (`GameFlow::UiData`, `apps/umbra/GameFlow.md`) and handles the actions.

| Screen | Opens when | Actions |
|---|---|---|
| `hud` | always | none |
| `start` | a save exists at launch | `continue`, `other`, `new` |
| `pause` | Esc / Start while playing | `resume`, `restart`, `quit`, `open:inventory`, `open:skills`, `open:quests`, `open:hotkeys` |
| `conversation` | talking | `answer` (index of the visible answer), `leave` |
| `fight` | your turn in a turn-based fight | `attack`, `items`, `flee` |
| `fight-moves` | after Attack | `move` (skill id), `back` |
| `fight-target` | after a move | `target` (roster index; arrows stand over the fighters, `at` = their screen point), `back` |
| `fight-items` | after Item | `item` (item id), `back` |
| `death` | you died | `load-manual`, `load-auto`, `new` |
| `inventory` | I, or the pause menu | `equip` (item id), `unequip` (slot), `close` |
| `skills` | K, or the pause menu | `unlock` (skill id), `close` |
| `quests` | J, or the pause menu | `close` |
| `hotkeys` | H, or the pause menu | `slot` (0-3), `assign` (`skill:<id>`, `item:<id>`, or empty to clear), `close` |

A project changes any of them by putting a file with the same path in its own `assets/ui/` (the
editor's Game UI window does this on Save). `sample.json` is example data in the shape of
`GameFlow::UiData`, for the editor preview and `UmbraTool ui-preview`; the game never reads it.
