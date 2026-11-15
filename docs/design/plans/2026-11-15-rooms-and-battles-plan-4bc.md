# A room at full size and battles in it (plans 4b and 4c) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **Status:** in progress (user, 2026-11-15: build 4a, 4b and 4c while away).

**Goal:** Look opens the player's room at full size, seen from a fixed corner like a stage, where the player's figure can walk and leave through a doorway; battles the player fights or sees fought play in that room with the figures' attack, hit and death clips.

**Architecture:** `Play` decides what the stage shows (headless, tested): `Looking()`, `StageCell()` and `StageActors()`; `LookDoor(d)` leaves through a doorway as the board's Move. `Table::DrawStage` draws the room (the room's tile as an 8 m floor, painted back walls, low front walls, door gaps), the figures at full size and monster cards; `Table::React` plays clips from new log entries (on the table too). Main switches between the table and the stage and routes clicks.

**Tech Stack:** C++20, the existing skinned figures, animation graphs and pictures.

**Spec:** `docs/design/2026-11-15-art-rooms-battles.md` (parts 4b and 4c)

## Global Constraints

- The stage never changes the night except by the board's own actions (Move through a doorway).
- Fixed corner camera (user); rooms open at full size only for Look (user); battles the player fights, and bots' battles in the player's light, play at full size (user); events stay on the table.
- Figures: the shared guest and Taken models, tinted (`#UMBRA-159`); monsters as painted cards.
- Code style per CLAUDE.md.

## Review Focus

1. Look on a bot's turn or in a dialog → refused (only the player's own turn, in the menu).
2. A doorway with no legal Move (barred, no moves left) → nothing happens, the stage stays.
3. A battle in a room the player cannot see → stays instant on the table.
4. A figure downed in battle stays down (dead flag) until woken; a new night clears it.
5. Save during Look → the save is the night; Continue opens the table.

---

### Task 1: Look and the stage in Play

**Files:** `Play.{hpp,cpp}`, `assets/ui/menu.ui.json` (Look button), new `assets/ui/look.ui.json`; Test `tests/PlayTests.cpp`

**Interfaces:** Produces `bool Looking() const`; `int StageCell() const` (Look: the player's cell; a battle in a lit cell: its cell; else -1); `struct Actor { int guest = -1, monsterKind = -1, side = 0; bool taken = false; }; std::vector<Actor> StageActors() const`; `bool LookDoor(int side)`; ui actions `menu/look`, `look/back`; `Screen()` `"look"`.

- [ ] Tests (T144): Look opens only in the player's menu; the night waits while looking; Back returns to the menu; a door with a legal Move leaves the look and moves one cell (moves left drop by one); a barred side does nothing; StageCell is the battle's cell while a battle in light runs and -1 for one in the dark; StageActors lists both sides of a battle with their sides.
- [ ] RED, implement, GREEN, commit.

### Task 2: The stage drawn

**Files:** `Table.{hpp,cpp}`, `Table.md`, `Main.cpp`

**Interfaces:** `void Table::DrawStage(const NightDef &, const BoardView &, int cell, const std::vector<Play::Actor> &, umbra::core::DrawList &) const`; `bool Table::StagePick(glm::vec2 mouse, glm::vec2 pixels, glm::vec3 &floor, int &door) const`; `void Table::WalkTo(glm::vec3)`; `void Table::React(const std::vector<LogEntry> &)`.

- [ ] Capture of Look (SwiftShader) and of a battle in the room. Commit.

### Task 3: Records

- [ ] `docs/testing/s31-rooms-and-battles.md`; ROADMAP, status, HANDOFF, CLAUDE.md. Commit, verify, ship.
