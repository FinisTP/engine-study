# The painted table (plan 4a) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **Status:** in progress (user, 2026-11-15: build 4a, 4b and 4c while away; a tile per room, a picture per event; feature rooms stand out; tiles painted over layout sketches; redo pictures freely).

**Goal:** the table's rooms, walls, doors, monsters, items, portraits, event pictures and UI skin are painted 2D art in the 1960s Saigon poster style; feature rooms stand out.

**Architecture:** pictures are made in ComfyUI on the user's PC (SDXL base; room tiles image-to-image over a sketch drawn in the page), from `tools/art/lastcurtain_prompts.json` (made by `tools/art/make_prompts.py` from the night's data). `tools/art/cut.py` resizes and stores them as JPEG/PNG under `apps/LastCurtain/assets/art/<set>/<data id>.<ext>`: no mapping file, the data ids are the names. `Art.{hpp,cpp}` (in `UmbraNightPlay`, no renderer) gives each picture's path, which rooms are featured, and checks that every picture exists. `Table` draws textured quads.

**Tech Stack:** C++20, `Renderer::CreateTexture`, the ui skin, Python 3 + Pillow, ComfyUI over HTTP (driven from the user's built-in browser).

**Spec:** `docs/design/2026-11-15-art-rooms-battles.md` (part 4a)

## Global Constraints

- 2D for rooms, walls, items, props, monsters, portraits, UI; monsters are flat painted cards.
- Every room keeps its kind's plan-2 colour as a border; featured rooms (foyer, under-stage, this night's seven start rooms, the rooms named in goals) get a gold frame.
- The Saigon Opera House keeps its real name; nothing from *Betrayal at House on the Hill*; no text in pictures.
- A missing picture falls back to today's coloured box, with one log line.
- Pictures committed small: tiles 512 px JPEG, cards/items 256 px PNG with the cream background keyed out, events and portraits 512 px JPEG [call].
- Code style per CLAUDE.md.

## Review Focus

1. A room added to `rooms.csv` without a picture → the box, and the asset test names the missing file.
2. Start rooms come from the night's seed, not the save: a restored night shows the same gold frames.
3. Dark tiles under the red hunt light → the kind border and gold frame still read.
4. Picture files missing in a package → boxes and plain panels, and `package-check` lists them.
5. A very long event picture load on a slow disk → loaded once at start, not per frame.

---

### Task 1: Art paths and featured rooms

**Files:** Create `apps/LastCurtain/Art.{hpp,cpp,md}`; Test `tests/ArtTests.cpp`

**Interfaces:** Produces `std::filesystem::path ArtPath(const std::string &set, const std::string &id)` (`"art/rooms/foyer.jpg"`: sets `rooms`, `events`, `portraits` jpg; `items`, `cards`, `ui` png), `std::vector<int> StartCells(const NightDef &, const NightState &)` (the cells of `NewNight(def, state.seed, state.player)`), `bool Featured(const NightDef &, const NightState &, int cell)`, `std::vector<std::string> MissingArt(const NightDef &, const std::filesystem::path &assets)`.

- [ ] Tests (T140): the foyer, the under-stage and every goal room are featured, a plain room is not; the start cells match the guests' cells at the night's start and are featured; a restored save gives the same start cells; `MissingArt` names a removed file.
- [ ] RED, implement, GREEN, commit.

### Task 2: The pictures

**Files:** `tools/art/make_prompts.py`, `tools/art/lastcurtain_prompts.json`, `tools/art/cut.py`, `apps/LastCurtain/assets/art/**`, `apps/LastCurtain/assets/art/README.md`

- [ ] Calibrate image-to-image strength on two rooms; queue every picture; review contact sheets; redo misses with new seeds (up to 4 tries); cut; `MissingArt` is empty (T141 asserts it on the real folder). Commit.

### Task 3: Painted tiles, walls and doors on the table

**Files:** `Table.{hpp,cpp}`, `Table.md`

- [ ] Each lit room is its tile on a thin base, with a kind-coloured border, featured ones in gold; walls are dark strips with a moulding line; doorways stay open. Captures in the evening and in the hunt. Commit.

### Task 4: Monster cards and item tokens

**Files:** `Table.cpp`, `BoardView` (`items` lying in a lit cell, if missing), `tests/BoardViewTests.cpp`

- [ ] Test (T142): the view lists the items lying in each lit cell. Monsters as standing cards facing the camera; items as small flat tokens. Capture. Commit.

### Task 5: The poster skin, portraits and event pictures

**Files:** `assets/ui/skin.json`, `hud`, `chat`, `inspect`, `event` screens, `Play::UiData` (`portrait`, `picture` paths), `sample.json`, Abril Fatface (SIL OFL) and its licence

- [ ] Test (T143): `UiData` carries the event's picture and the chat asker's portrait. `ui-preview` of hud, chat, inspect, event, menu. Commit.

### Task 6: Package and records

- [ ] `package-check` includes `art/`; `docs/testing/s30-painted-table.md`; ROADMAP, status, HANDOFF, CLAUDE.md; the art resource Claude Doc. Commit, verify, ship.
