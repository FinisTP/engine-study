# Plan 2c: the screen — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `LastCurtain.exe` plays the turn the way the user asked: a turn menu, walking clicks that slide one step at a time, a Stay/Back confirm for new rooms, input that depends on the state, inspect, a save menu, a following camera with drag and Focus, a thinking pause for bots, walls wherever a guest cannot pass, and group numbers on the rings.

**Architecture:** The rules (`modules/night`) and bots do not change. What the player sees grows in `modules/board` (walls, paths). The turn's states live in `Play` (headless, tested in `PlayTests.cpp`): it owns the mode, the walk, the slide clock, the thinking pause and the follow flag, and says what to draw. `Main.cpp`, `Table.*` and the `ui/*.ui.json` screens only show what `Play` says and pass input in. One small engine change: a screen node's `offset` may be a binding, so labels can sit over pawns.

**Tech Stack:** C++20, engine/ui screens (JSON), Catch2, CMake; checks on Linux GCC/Clang/ASan and SwiftShader pictures.

**Spec:** `docs/design/2026-11-14-goals-and-bigger-board.md` (sections "States and transitions (plan 2c)", "Game Feel", "UI Requirements (plan 2c)", "Visual/Audio Requirements", "Acceptance Criteria" Automated (2c)).

## Global Constraints

- Short, simple code; `// 1.` step comments where code is not obvious; a `.md` beside the code for concepts (projection of labels, slides, drag) — extend `apps/LastCurtain/Table.md` and `modules/board/include/Board.md`.
- C++ string and char literals ASCII (MSVC reads the local code page).
- Commits `issue #UMBRA-<n> <summary>`, numbers from #147; stage by name or `git add -A plugins/last-curtain` plus named files; never commit `projects/reference/assets/scenes/settlement.scene.json`.
- After Task 0 the rules, the bots and `assets/night/` do not change; the soak must still say "the data holds" and the golden fingerprint must not move after Task 0.
- Slide: 0.25 s a step **[call]**. Thinking pause: `max(0.4 s, the decision's own time)` before a bot's first action of a turn, only when the player sees it (user). Beat lengths stay (1.5 s seen, 0.3 s after Space, 0.2 s unseen).
- Input table (user), exactly:

  | State | Esc | Arrows | Enter | Click on the board |
  |---|---|---|---|---|
  | menu | save menu | pick mode, step that way | End turn | left: walk to a lit square · right: inspect |
  | picking / walking | back to menu (steps taken stay) | step | walk to the hovered square | left: choose a square · right: inspect |
  | confirm (Stay/Back) | Back | — | Stay | — |
  | a dialog (event, dice, chat, Turn, bag, inspect, save menu) | close or Back where it has one | move focus | the focused button | ignored |
  | a bot's turn | save menu | — | — | left or right: inspect |

- Never report a Windows, GPU or play check as passed from Linux.

## Review Focus

1. The window is minimised mid-walk or mid-slide (dt 0): nothing moves and nothing is lost; the walk goes on after (test in Task 3).
2. Quitting and Continue in the middle of a walk: the save is the rules' state only; Continue opens the turn menu with the moves that were left (test in Task 3).
3. An event, dice or battle opens part-way along a clicked path: the walk stops there, the rest of the path is dropped, and the menu returns after the dialog (test in Task 3).
4. Right-click on an unopened cell, a dark room or outside the board: inspect shows only what the player may know — nothing for unopened or outside, name and kind for a dark room, no guests or items in the dark (test in Task 4).
5. The player's guest is dead (watching): the camera still follows beats, Focus does nothing harmful, Esc opens the save menu (test in Task 6).

---

### Task 0: No ally goals (#147) (user, 2026-11-15)

**Files:**
- Modify: `modules/night` (`GoalKind::Ally` removed: loader, `GoalDone`, tests), `modules/nightbots` (the ally parts of `Purpose`, `Seat`), `assets/night/goals.csv`, `docs/design/cast.md`, `Night.md`, `Balance.md`
- Test: `tests/NightDataTests.cpp`, `tests/GoalTests.cpp`, `tests/BotTests.cpp`

The user: an ally is not a personal goal. The four ally goals get other goals **[call]**: Nhi — pass two of the house's tests (what happened to her father); Bao — pass two tests (steady hands); Mai — destroy one creature; Lan — reach the Hall of mirrors.

- [ ] **Step 1: Write the failing test** `"A goal of kind ally is refused (T117)"` (`night.goals`, the kind column).
- [ ] **Step 2: Run** — Expected: FAIL (ally loads).
- [ ] **Step 3: Implement**; replace the four rows; update the golden fingerprint; re-run the soak on seeds 1, 20001, 80001 and tune only the new goals' counts if one is out of band.
- [ ] **Step 4: Run** the dev tests and the balance test — Expected: pass; "the data holds".
- [ ] **Step 5: Commit** `issue #UMBRA-147 last-curtain: no ally goals`.

### Task 1: Walls wherever a guest cannot pass (#148)

**Files:**
- Modify: `modules/board/include/BoardView.hpp`, `modules/board/src/BoardView.cpp`, `modules/board/include/Board.md`, `apps/LastCurtain/Table.cpp`, `apps/LastCurtain/Table.md`
- Test: `tests/BoardViewTests.cpp`

**Interfaces:**
- Produces: `CellView::walls` (`std::uint8_t`, NESW bit 0 = N): a side of an opened cell that is neither a passage (`Passage`) nor a doorway. `Table::Draw` draws a wall stub on every `walls` bit (not on "no door" only).

- [ ] **Step 1: Write the failing test** `"A door facing a wall is drawn as a wall on both sides (T123)"`: from `FoyerNight(def, 1)`, place two opened neighbours by hand (`state.cells[...] = {room, doors, true}`) where cell A has a door east and cell B (east of A) has no door west; `MakeView(def, state, -1)`: `A.walls & (1 << 1)` and `B.walls & (1 << 3)`; a real passage (both doors) has no wall bit; a doorway (door to an unopened cell) has `doorways` set and no `walls` bit; a sealed passage (`state.sealed` mark) has a wall bit.
- [ ] **Step 2: Run** `timeout 110 /home/claude/dev/b/tests/UmbraNightTests "*T123*"` — Expected: FAIL to compile (no `walls`).
- [ ] **Step 3: Implement** in `MakeView`: `walls` bit when `!Passage(def, state, c, d) && !(doorways bit)`; Table draws walls from `walls` (the old "no door" test goes). Board.md and Table.md: one line each.
- [ ] **Step 4: Run** the dev tests — Expected: all pass.
- [ ] **Step 5: Commit** `issue #UMBRA-148 last-curtain: walls wherever a guest cannot pass`.

### Task 2: Paths to a lit square (#149)

**Files:**
- Modify: `modules/board/include/BoardView.hpp`, `modules/board/src/BoardView.cpp`, `Board.md`
- Test: `tests/BoardViewTests.cpp`

**Interfaces:**
- Produces: `std::vector<int> PathTo(const NightDef &def, const NightState &state, int guest, int cell)` — the cells of the shortest walk from the guest's cell to `cell`, the start left out, cardinal steps only, through passages, never into a cell barred for the guest this turn (`Guest::barred`); the last step may go through a doorway into an unopened cell; empty when there is no such walk, when `cell` is the guest's own, or when `cell` (opened) is not lit for `guest` (`Lit` with the guest's Light). Transport rooms are not used by paths (the menu has "Take the passage").

- [ ] **Step 1: Write the failing tests** `"A path walks cardinal steps through passages (T124)"`: a chain of 3 opened rooms from the foyer (use `Chain` as in BotTests, copied or moved to `NightTestData.hpp`): `PathTo(def, state, g, chain[2]) == chain`; `"A path may end through a doorway (T124)"`: the unopened cell beyond a doorway of `chain[0]` gives `{chain[0], thatCell}`; `"No path to a dark, walled-off, barred or own cell (T124)"`: a `Closet` cell → empty; the guest's own cell → empty; after barring `chain[0]` → empty to `chain[2]`.
- [ ] **Step 2: Run** `"*T124*"` — Expected: FAIL to compile.
- [ ] **Step 3: Implement** a breadth-first search from the guest's cell (Board.md: one paragraph).
- [ ] **Step 4: Run** the dev tests — Expected: all pass.
- [ ] **Step 5: Commit** `issue #UMBRA-149 last-curtain: paths to a lit square`.

### Task 3: The turn: menu, picking, walking, Stay/Back (#150)

**Files:**
- Modify: `apps/LastCurtain/Play.hpp`, `Play.cpp`, `Main.cpp`, `Table.hpp`, `Table.cpp`, `Table.md`, `README.md`; rename `assets/ui/actions.ui.json` → `menu.ui.json`; create `assets/ui/picking.ui.json`, `assets/ui/confirm.ui.json`; `assets/ui/sample.json`
- Test: `tests/PlayTests.cpp`

**Interfaces:**
- Consumes: `PathTo` (Task 2).
- Produces, in `Play`:
  - `enum class Mode { Menu, Picking, Confirm };` and `Mode TurnMode() const`.
  - `void Click(int cell, bool right = false)`; `void Hover(int cell)`.
  - `struct Slide { int guest = -1, from = -1, to = -1; float t = 0.0F; };` (`t` 0..1) and `Slide Sliding() const` (guest -1: none).
  - `struct Preview { std::vector<int> path; int affordable = 0; };` and `Preview Previewed() const` (picking: the hovered square's path; cells past `affordable` are drawn red).
  - `static constexpr float slideSeconds = 0.25F;`
  - `Screen()` returns `"menu"` (was `"actions"`), `"picking"`, `"confirm"` on the player's turn; HandleUi actions `"move"` (menu → picking), `"stay"`, `"back"`.
  - `Table::Draw(def, view, preview, slide, out)`: path markers gold up to `affordable`, red after; the sliding pawn drawn at `mix(from, to, smoothstep(t))`.

- [ ] **Step 1: Write the failing tests** (rename "actions" to "menu" in the existing PlayTests; tests that clicked a step cell now click and call `Update(Play::slideSeconds)` per step):
  - `"A click walks a lit square three steps away, one sliding step at a time (T125)"`: 4 moves left, a lit cell 3 steps away along opened rooms; `Click(cell)`; after each `Update(0.25F)` the guest is one cell further and `Sliding().guest` was the player during it; after 3 steps `movesLeft == 1` and `TurnMode() == Mode::Menu`.
  - `"A square beyond the moves left is ignored; its path shows red past the last affordable step (T125)"`: `Hover(cell)` in picking: `Previewed().affordable == movesLeft`, `path.size() > affordable`; `Click(cell)` changes nothing.
  - `"Illegal squares are ignored in the menu and in picking (T125)"`: a `Closet` cell, the own cell, -1 and 81: nothing changes.
  - `"The walk stops at a new room's door with Stay/Back (T125)"`: the path ends through a doorway; after the steps before it, `Screen() == "confirm"`; `HandleUi("confirm", "back", "")` → picking, no room opened, moves kept; again with `"stay"` → the room opens (one `Opened` log entry), mode Menu after the slide.
  - `"A dialog part-way along a walk drops the rest of the walk (T125)"` (Review Focus 3): a path whose first room has an event; after the first step `Screen() == "event"`; after the event and dice are dismissed `TurnMode() == Mode::Menu` and the guest has not moved again.
  - `"dt 0 changes nothing mid-walk (T125)"` (Review Focus 1): `Update(0)` mid-walk leaves the cell, the slide and the mode as they were; the walk then finishes.
  - `"Continue mid-walk opens the menu with the moves that were left (T125)"` (Review Focus 2): `Restore(Save())` after one of three steps; a new `Play` from it: `Screen() == "menu"`, `movesLeft` as saved.
- [ ] **Step 2: Run** `timeout 110 /home/claude/dev/b/tests/UmbraNightTests "[play]"` — Expected: FAIL.
- [ ] **Step 3: Implement.** Walk = the `PathTo` cells; `Update(dt)` (player's turn): when no slide runs and no dialog waits, take the next cell — unopened: mode Confirm and stop; else apply the legal Move toward it and start a slide; a move that wrote any log entry other than a plain step, or left 0 moves, ends the walk. Stay applies the opening move (with its slide) and ends the walk; Back returns to picking. Menu entries: "Move" (when any Move is legal), then the old action labels (Fight the X, Chat with X, Take the passage, the power), "Use item" (opens the bag), "End turn". `menu.ui.json`, `picking.ui.json` ("Choose a lit square. Esc: back.") and `confirm.ui.json` ("A closed door. Open it?" with Stay / Back; the room behind is unknown until opened). Main: the mouse position goes to `Hover` each frame; draws `Previewed()` and `Sliding()`. Table.md: the slide's smoothstep in one line.
- [ ] **Step 4: Run** the dev tests and `UmbraTool ui-preview menu picking confirm` with `sample.json` — Expected: all pass; 0 errors.
- [ ] **Step 5: Commit** `issue #UMBRA-150 last-curtain: the turn menu, walking clicks and Stay/Back`.

### Task 4: Input by state, inspect, the save menu (#151)

**Files:**
- Modify: `Play.hpp`, `Play.cpp`, `Main.cpp`; create `assets/ui/inspect.ui.json`, `assets/ui/pause.ui.json`; `sample.json`
- Test: `tests/PlayTests.cpp`

**Interfaces:**
- Produces: `Key(InputKey)` follows the input table (Global Constraints) exactly; `Click(cell, true)` (right) opens inspect at any time but behind a dialog; on a bot's turn a left click inspects too. `Screen()` adds `"inspect"` and `"pause"`; `UiData()["inspect"] = {room: {name, kind, dark}, guests: [{name, hp, maxHp, might, sanity, speed, level, group, bag: [names], goal}], items: [names], monsters: [names]}`; HandleUi `"close"` (inspect), `"resume"` (pause). `"save"` and `"savequit"` are Main's (it writes the file; savequit then goes back to the start screen).

- [ ] **Step 1: Write the failing tests**, one per cell of the input table (named `"Input: <state>, <key> (T126)"`, e.g. `"Input: menu, Esc opens the save menu (T126)"`, `"Input: picking, Esc goes back to the menu and keeps the steps taken (T126)"`, `"Input: confirm, Enter is Stay (T126)"`, `"Input: a dialog ignores clicks on the board (T126)"`, `"Input: a bot's turn, a left click inspects (T126)"` …), and:
  - `"Inspect shows only what the player may know (T126)"` (Review Focus 4): an unopened cell and -1 → no inspect screen; a dark opened room → name and kind, `dark == true`, no guests, items or monsters; a lit room with a bot → that bot's HP, stats, level, bag; its `goal` is `""` unless the player's `knows` bit for it is set; the player's own goal always shows.
- [ ] **Step 2: Run** `"*T126*"` — Expected: FAIL.
- [ ] **Step 3: Implement**; Main stops quitting on Esc (Esc goes to `Play::Key`; the window's close button still quits, saving as now); right button → `Click(cell, true)`.
- [ ] **Step 4: Run** the dev tests and `ui-preview inspect pause` — Expected: pass; 0 errors.
- [ ] **Step 5: Commit** `issue #UMBRA-151 last-curtain: input by state, inspect and the save menu`.

### Task 5: Bots on screen: thinking and sliding (#152)

**Files:**
- Modify: `Play.hpp`, `Play.cpp`
- Test: `tests/PlayTests.cpp`

**Interfaces:**
- Produces: `using Decider = std::function<umbra::night::Action(const umbra::night::NightState &, int guest, float &seconds)>;` and `void SetDecider(Decider)` — the default calls `ChooseAction` and measures it with `std::chrono::steady_clock`. `float Thinking() const` (seconds of "thinking..." left, 0 when none) and `int ThinkingGuest() const`. A bot's move slides like the player's (`Sliding()`), inside its beat.

- [ ] **Step 1: Write the failing tests** `"A bot in view thinks max(0.4 s, its decision time) before its first action (T127)"`: a decider reporting 0.1 s → `Thinking()` starts at 0.4 and the bot's action is applied only after 0.4 s of `Update`; 0.9 s → 0.9; the bot's second action in the same turn has no pause; a bot the player cannot see has none. `"A bot's step slides (T127)"`: a seen bot's Move → `Sliding().guest == bot` for 0.25 s.
- [ ] **Step 2: Run** `"*T127*"` — Expected: FAIL.
- [ ] **Step 3: Implement**: the decision is made, timed and held as the pending action; the pause counts down; then Apply, the beat and the slide as now.
- [ ] **Step 4: Run** the dev tests — Expected: pass.
- [ ] **Step 5: Commit** `issue #UMBRA-152 last-curtain: bots think and slide`.

### Task 6: The camera follows; drag and Focus (#153)

**Files:**
- Modify: `Play.hpp`, `Play.cpp`, `Table.hpp`, `Table.cpp`, `Table.md`, `Main.cpp`, `assets/ui/hud.ui.json` (a Focus button)
- Test: `tests/PlayTests.cpp`

**Interfaces:**
- Produces: `int CameraCell() const` — the cell to look at: the sliding pawn's `to`, else the last seen beat's camera, else the acting player's pawn; -1 when nothing. `void Drag()` (following stops), HandleUi `"focus"` (following again), `bool Following() const`; `UiData()["focus"]` true while not following (the button shows). `Table::Pan(glm::vec2 pixels)` moves the look-at point across the table by the drag (Table.md: the pixel-to-metre scale with the distance). Main: the left button held and moved more than 4 px is a drag, not a click; while following, the table aims at `CameraCell()` whenever it changes.

- [ ] **Step 1: Write the failing tests** `"The camera follows each step of a walk (T128)"`: during a 3-step walk `CameraCell()` equals each next cell in turn. `"A drag stops following until Focus (T128)"`: `Drag()` → `Following() == false`, `UiData()["focus"] == true`; `HandleUi("hud", "focus", "")` → following. `"A dead player still follows the beats; Focus and Esc are safe (T128)"` (Review Focus 5): player dead, a seen bot beat → `CameraCell()` is the beat's camera; `"focus"` and `Key(Escape)` (→ "pause") do not throw or act.
- [ ] **Step 2: Run** `"*T128*"` — Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the dev tests — Expected: pass.
- [ ] **Step 5: Commit** `issue #UMBRA-153 last-curtain: the camera follows; drag and Focus`.

### Task 7: Labels over the table: thinking, group numbers; the goal's progress (#154)

**Files:**
- Modify: `engine/ui/src/Screen.cpp`, `engine/ui/src/Ui.cpp`, `engine/ui/include/Screen.hpp`, `engine/ui/include/UI.md`; `modules/board/src/Narrate.cpp`, `assets/night/captions.json`; `Play.hpp`, `Play.cpp`, `Table.hpp`, `Table.cpp`, `Table.md`, `Main.cpp`; create `assets/ui/labels.ui.json`; `hud.ui.json`, `sample.json`
- Test: `tests/ui/` (the screen tests), `tests/PlayTests.cpp`, `tests/NarrateTests.cpp`

**Interfaces:**
- Produces (engine): a node's `"offset"` may be a binding string such as `"{l.at}"` that resolves to `[x, y]` pixels; a bad or missing value means `[0, 0]`; a literal pair works as before.
- Produces (game): `UiData()["labels"]` is filled by Main: `[{text, at: [x, y]}]` — "thinking..." over the thinking bot, and the group number over each ringed pawn. `Play::Rings() const` → `std::vector<std::pair<int, int>>` of (guest, number): groups numbered 1, 2, 3 in order of their lowest member, the player's group always 1. `Table::Draw` colours rings by number and `Table::Above(def, view, guest)` gives the pawn's top point; Main projects it to pixels (Table.md: the projection). `UiData()["goal"]["progress"]`: `"1 of 2"` for events and monsters goals, `""` otherwise. The narrator's result line gains `told` ("{other} tells {who} what they came for") for a passed chat answer (captions.json and its key list).

- [ ] **Step 1: Write the failing tests**: in `tests/ui`, `"An offset can come from the data"` — a screen with `"offset": "{p.at}"` and data `{"p": {"at": [40, 30]}}` places the node 40 px right and 30 px down of its anchor; a missing binding places it at the anchor. In PlayTests: `"Rings are numbered by group, the player's first (T129)"` and `"The HUD shows a goal's progress (T129)"` (an events goal with `goalCount` 1 of 2 → `"1 of 2"`); in `NarrateTests.cpp`, `"A passed answer tells the goal (T129)"` (a Persuade entry with a pass → the result has the `told` line).
- [ ] **Step 2: Run** the ui tests and `"*T129*"` — Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** every CTest in the dev build and `ui-preview labels hud` — Expected: pass; 0 errors.
- [ ] **Step 5: Commit** `issue #UMBRA-154 last-curtain: labels over pawns and the goal's progress`.

### Task 8: Records, checks, ship (#155)

**Files:**
- Create: `docs/testing/s28-the-screen.md` (ROADMAP §14 template; the Windows checklist from the spec: a full night as one guest, a goal finished and one failed, a bot-to-bot recruit seen, the camera follow and drag, the save menu, Esc in each state)
- Modify: `docs/ROADMAP.md`, `docs/current-engine-status.md`, `docs/design/HANDOFF.md`, `CLAUDE.md`, `apps/LastCurtain/README.md` (the keys)

- [ ] **Step 1: Run** `/home/claude/verify.sh` (gcc, clang, asan) — Expected: every CTest passes.
- [ ] **Step 2: Run** the soak on seed 1 (2,000 nights) — Expected: "the data holds", same numbers as plan 2b (the rules did not change).
- [ ] **Step 3: Capture** with SwiftShader: `LastCurtain --seed 3 --player nhi --frames 30` and a `--watch` run long enough to catch a slide and a "thinking..." label — Expected: walls on every closed side, the menu, a label over a pawn.
- [ ] **Step 4: Write the records**; **commit** `issue #UMBRA-155 last-curtain: plan 2c records`; ship to the PC with `git am`; the final whole-branch review with the Review Focus above; the final message lists every ledger "Ruling:" and deferred minor.
