# The painted table (plan 4a) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **Status:** ready; the style board's direction is approved (user, 2026-11-15). Not started; plan 3 comes first. Plans 4b (a room at full size) and 4c (battles in the room) are written after 4a works, as the spec says.

**Goal:** the table's rooms, walls, doors, monsters, items, portraits, event picture and UI skin are painted 2D art in the 1960s Saigon poster style.

**Architecture:** pictures are made in ComfyUI on the user's PC (SDXL), cut and colour-checked by one Python script, and stored as JPEG/PNG under `apps/LastCurtain/assets/art/`. `art.json` maps room kinds and named rooms to tiles, guests to portraits, monsters and item kinds to cards and tokens. `Table` draws textured quads with those textures; the ui skin uses the frame as a 9-slice panel.

**Tech Stack:** C++20, the engine's `Renderer::CreateTexture` and ui skin (`skin.json`), Python 3 + Pillow for `tools/art/`, ComfyUI (SDXL base) driven over its HTTP API.

**Spec:** `docs/design/2026-11-15-art-rooms-battles.md` (part 4a)

## Global Constraints

- 2D for rooms, walls, items, props, monsters, portraits, UI; props and monsters are flat painted cards.
- Each room tile keeps its kind's plan-2 colour as a border, so kinds still read at a glance.
- The Saigon Opera House keeps its real name and 1960s look; nothing from *Betrayal at House on the Hill*.
- Generated pictures: prompts and seeds recorded in `art/README.md`; no text in pictures (titles are drawn by the ui).
- A missing picture falls back to today's coloured box, with one log line.
- Code style per CLAUDE.md; picture files are committed (JPEG ≤ 300 KB each [call]).

## Review Focus

1. A room with no tile in `art.json` (a new row in `rooms.csv`) → its kind's tile, then the box (Task 1 test).
2. Very dark tiles under the red hunt ambient → the room kind border still visible (Task 3 capture).
3. A 9-slice frame stretched to a wide panel → corners not smeared (Task 5 ui-preview).
4. Portrait for a guest added later → a plain silhouette card, not a crash (Task 1 test).
5. Picture files missing in a package → the boxes and flat panels, and `UmbraTool package-check` lists them (Task 6).

---

### Task 1: `art.json` and its check

**Files:** Create `apps/LastCurtain/assets/art/art.json`, `apps/LastCurtain/Art.{hpp,cpp}` (in `UmbraNightPlay`, no renderer); Test `tests/ArtTests.cpp`

**Interfaces:** Produces `struct ArtDef { std::map<std::string, std::string> rooms, kinds, portraits, monsters, items; std::string event, frame; };` `bool LoadArt(const std::filesystem::path &, const NightDef &, ArtDef &, std::vector<core::Diagnostic> &);` and `std::string TileFor(const ArtDef &, const RoomDef &)` (named room, else kind, else `""`).

- [ ] Tests (T140): every room kind has a tile and every file named exists; `TileFor` picks the named room's tile, else the kind's; an unknown guest id in `portraits` is refused with its field.
- [ ] RED, implement, GREEN, commit.

### Task 2: Make the pictures

**Files:** Create `tools/art/comfy_batch.json` (prompt list), `tools/art/cut.py` (square crop, resize 1024/512, colour-check against the palette, JPEG), `apps/LastCurtain/assets/art/README.md` (prompts, seeds, model)

- [ ] Queue the 4a set in ComfyUI (about 55: 20 tiles, 4 wall/door pieces, 3 monster cards, 12 item tokens, 7 portraits, 1 event, 6-8 ui pieces), with the style suffix and negative prompt of the style board.
- [ ] Review each; redo misses with a new seed (keep a list of rejects in the README). Run `cut.py`. Commit the pictures and README.

### Task 3: Painted tiles, walls and doors on the table

**Files:** Modify `Table.{hpp,cpp}`, `Table.md`; Main passes `ArtDef`

**Interfaces:** Consumes `ArtDef`, `TileFor`. `Table` gains a textured quad mesh and a `std::map<std::string, core::TextureHandle>` loaded once.

- [ ] A capture (`--seed 7 --player huy`) with painted tiles, the kind border, wall strips and doors; a capture in the hunt (red ambient). Compare with today's capture in the record. Commit.

### Task 4: Monster cards and item tokens

**Files:** Modify `Table.cpp`, `BoardView` (items lying in a cell, if not already in the view), `tests/BoardViewTests.cpp`

- [ ] Test (T141): the view lists the items lying in each lit cell.
- [ ] Monsters as standing cards facing the camera (billboard about Y); items as small flat tokens. Capture. Commit.

### Task 5: The poster skin, portraits and the event picture

**Files:** Modify `assets/ui/skin.json`, `hud`, `chat`, `inspect`, `event` screens, `sample.json`; add Abril Fatface (SIL OFL) and its licence for titles; Poppins stays for body text

- [ ] `UmbraTool ui-preview` of hud, chat, inspect, event, menu with the new skin; portraits in hud/chat/inspect; the event picture on `applause` only. Commit.

### Task 6: Package and records

- [ ] `UmbraTool package-check` includes `art/`; a package capture. `docs/testing/s30-painted-table.md` with captures and the Windows look check (§4); ROADMAP, status, HANDOFF; update the art resource Claude Doc with what was made. Commit.
