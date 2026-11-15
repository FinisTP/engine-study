# The table's pictures (plan 4a)

Every picture is named by the data id it shows, so adding a room or an event to the data only needs
its picture in the right folder. `MissingArt` lists what is not there (the asset test and
`package-check` use it).

| Folder (under `assets/`) | One per | Format |
|---|---|---|
| `art/rooms/` | room in `rooms.csv` | 512 px JPEG, top-down |
| `art/events/` | event in `events.csv` | 512 px JPEG |
| `art/portraits/` | guest in `guests.csv` | 512 px JPEG |
| `art/items/` | item in `items.csv` | 256 px PNG, background keyed out |
| `art/cards/` | monster and minion (`monsters.csv`, `minions.csv`) | 256 px PNG, background keyed out |
| `art/ui/` | `panel`, `paper`, `title` | PNG |
| `art/plaques/` | room in `rooms.csv`: its floor plaque | 1024 x 192 PNG, `tools/art/make_plaques.py` |
| `art/sky/` | `before` and `after` midnight | 1536 x 640 JPEG |

## Featured rooms

Drawn with a gold frame: the foyer, the under-stage, every room named in a goal (`goals.csv`, kind
`room`), and tonight's seven start rooms. Start rooms are dealt by chance when a night begins and are
not saved; `StartCells` deals the night again from its seed (`NewNight(def, seed, player)`) and reads
where the guests began, so a restored night shows the same frames.

## How the pictures are made

`tools/art/make_prompts.py` writes one prompt per picture from the data; ComfyUI paints them (room
tiles over a simple top-down sketch, so they stay flat); `tools/art/cut.py` sizes and stores them.
Prompts, seeds and rejects: `art/README.md`.

The plaques are drawn, not painted: `python3 tools/art/make_plaques.py` writes one per room from
`rooms.csv` (a near-black plate, a gold border and screws, the name in Lora capitals sized to fit).
Run it again when a room is added or renamed.
