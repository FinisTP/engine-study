# The Last Curtain, plan 2: the board on screen — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A person plays one guest through a whole night of *The Last Curtain* in its own program, `LastCurtain.exe`, on a tabletop board, with readable bot turns, chats, events, omens, battles, the Turn and the end.

**Architecture:** Three new rule pieces go into the headless `night` module first (pushed back by an event, the conversation rule). A new headless module `board` turns a `NightState` into what the player may see and narrates bot turns, so every rule of visibility is tested without a window. The program `plugins/last-curtain/apps/LastCurtain/` draws that view with placeholder meshes made in code, shows `assets/ui/*.ui.json` screens through `engine/ui`, and turns clicks and keys into `Apply` calls.

**Tech Stack:** C++20, CMake plugin system, Vulkan renderer (`Umbra::Render`), SDL3 window and input (`Umbra::Platform`), `engine/ui` screens (`Umbra::Ui`), nlohmann-json, Catch2.

**Spec:** `docs/design/2026-11-14-board-on-screen.md` (approved 2026-11-14), with `docs/design/game-design.md` and `docs/design/cast.md`. Mockups: the claude.ai canvas "The Last Curtain — board screens (concept mockups)" (nine screens).

## Global Constraints

- Short, simple code; step comments (`// 1. ...`); a markdown file beside code for concepts (CLAUDE.md "Code style").
- Every gameplay number and every game text (questions, event texts, captions) in `plugins/last-curtain/assets/`; screens in `assets/ui/*.ui.json`. C++ string literals ASCII (MSVC).
- Rules stay in `modules/night`; `modules/board` and the program only read the state and call `LegalActions`/`Apply`.
- Nothing outside the player's light (and their allies') is shown: no pawn, monster, minion or Taken in a room that light does not reach (design, "Anti-goals").
- `Umbra.exe`, the RPG plugins and the reference project are not touched, except one generic line in the root `CMakeLists.txt` (Task 5).
- Never name identifiers `near`, `far`, `time`, `close`. Commits `issue #UMBRA-<n> <summary>` from `#UMBRA-121` on, with the Co-Authored-By and Claude-Session lines; stage files by name, never `git commit -a` (the user's own edits stay out).
- Verification: Linux `TAIL=6 /home/claude/build.sh gcc` and `bash /home/claude/verify.sh` (GCC, Clang, ASan) for everything headless; the program builds there but its GPU and play checks are the user's on Windows (x64 shell).

## Review Focus

1. **A chat or an omen choice pending when the night ends or the player dies:** the night ends cleanly; the screen closes; nothing waits for an answer that cannot come. (Tasks 2, 9)
2. **The player's guest becomes the Taken** (failed curse roll or chance): the program keeps working with the player controlling the Taken; the view shows the company only where the Taken's light reaches. (Tasks 3, 8)
3. **A save from older data or a damaged save:** Continue is hidden and a new night starts; no crash. (Task 10)
4. **A click on a room that is not a legal step** (two away, through a wall, unopened without a doorway, during a bot turn): nothing happens. (Task 6)
5. **The window is resized or minimised during a bot turn:** the turn finishes, the camera and picking use the new size. (Tasks 5, 7)

---

### Task 1: Pushed back by an event

**Files:** Modify `modules/night/include/NightDef.hpp`, `include/Night.hpp`, `src/NightData.cpp`, `src/Turn.cpp`, `assets/night/events.csv`, `include/Night.md`; Test `tests/TurnTests.cpp`

**Interfaces:**
- Produces: `Effect::Back` (events.csv word `back`); `Guest::from` (`std::int8_t`, the cell the guest stood in before its last move; the foyer at the start).

- [ ] **Step 1: Write the failing test** `TEST_CASE("An event can push a guest back to the room they came from (T106)", "[night][check]")`: a one-event copy (like `OneEvent`) with `failEffect = Effect::Back`, target 99; the guest moves from the foyer into a new event room: after the roll `guest.cell == state.foyer`, `guest.from` unchanged meaningfully, and the turn has ended. A pass effect of `Back` with target 1 does the same (any effect may be either outcome).
- [ ] **Step 2: Run** `ctest --test-dir /home/claude/build-gcc -R Umbra.Night --output-on-failure` — Expected: FAIL to build (`Effect::Back` missing).
- [ ] **Step 3: Implement:** `Back` appended to `Effect` and to the events effect word list; `Guest::from` set in `Apply(Move)` before the guest moves; `ApplyEffect` `Back` puts the guest in `from` (a passage back always exists: they just came through it). Add it to `NightSave.cpp`'s field list. Change two `events.csv` rows to use it (e.g. `cold` fail `back,0`; `bell` fail `back,0`).
- [ ] **Step 4: Run** the same ctest — Expected: PASS; update the golden fingerprint in `SaveTests.cpp`; `night-soak --nights 1000` still "holds" (`/tmp/soak` harness).
- [ ] **Step 5: Commit** `issue #UMBRA-121 last-curtain: events can push a guest back`

### Task 2: The conversation rule

**Files:** Create `assets/night/questions.csv`; Modify `NightDef.hpp`, `Night.hpp`, `src/NightData.cpp`, `src/Company.cpp`, `src/Turn.cpp`, `src/NightSave.cpp`, `assets/night/rules.json`, `assets/night/styles.csv`, `modules/nightbots/src/NightBots.cpp`, `src/NightSoak.cpp`, `include/Night.md`, `nightbots/include/Balance.md`; Test `tests/CompanyTests.cpp`, `tests/NightDataTests.cpp`

**Interfaces:**
- Produces:
  - `struct QuestionDef { int guest = 0; std::string id, text, physicalText, magicalText; int physicalTarget = 0, magicalTarget = 0; };` and `NightDef::questions`; `Rules::persuade` becomes `{ int approvals = 3, bond = 3; }`; `StyleDef::chatty` (0..10).
  - `ActionKind`: `Persuade` is replaced by `Chat` (a = the guest to talk to); new `OpenChat` (a bot asks the player: a = the player's guest) and `Answer` (a = 0 physical, 1 magical), appended at the end.
  - `struct ChatState { bool active = false; std::int8_t asker = -1, question = -1; bool byBot = false; };` in `NightState::chat`; `Guest::approvals` (`std::int8_t`), `Guest::asked` (`std::uint8_t`, bits of that guest's own pool already asked), `Guest::chatRound` (`std::int8_t`, the round it last opened a chat). `Guest::askedUntilRound` is removed.
  - `int AnswerTarget(const NightDef &def, const NightState &state, int answer);` (the open chat's target for answer 0 or 1, after the bond) and `int ChancePercent(int stat, int target);` (d20 + stat >= target, 0..100) in `Night.hpp`.
  - `Current()` returns `state.player` while `chat.active`.

Rules pinned (design, "The conversation rule"): `Chat` is legal for the player's guest only, before midnight, with a standing guest in the room who is not in group 0, while group 0 has fewer than 3; it draws a random unasked question of that guest's pool (all asked: any) and opens the chat; `Answer` rolls `RollCheck(player, Might|Sanity, AnswerTarget)`; a pass adds one approval; reaching `persuade.approvals` puts both in group 0 if there is room; after a player-opened chat the player's turn ends; after a bot-opened one the bot's turn goes on. `OpenChat` is legal for a bot when the player's guest is standing in its room, before midnight, the bot is not in group 0, its approvals are below the goal, it has not moved this turn and `chatRound != round`; a bot takes it when `chatty >= 5` or `Bond(bot, player) > 0`. In the soak (guest 0 is the player) guest 0's bot answers with the higher `ChancePercent`.

`questions.csv` columns `guest,id,text,physicalText,physicalTarget,magicalText,magicalTarget` (targets `easy|normal|hard`), five rows per guest, 35 in all, in each guest's voice from `cast.md` (Nhi blunt and corrects adults; Thanh formal and soft; Bao clinical, full names; Khang slang, "kid", "boss"; Mai proverbs, "child"; Huy narrates; Lan precise compliments with an edge), ASCII only, no text from the board game. The loader refuses a guest with a number of questions other than 5 (`night.questions`).

- [ ] **Step 1: Write the failing tests** in `CompanyTests.cpp` (replacing the persuade test): `"Chatting asks one of the guest's questions and uses the turn (T107)"` (Chat legal only for the player, only in the same room, not with the Taken, not after midnight; after `Chat` `Current() == player`, legal actions are exactly `Answer 0` and `Answer 1`; after answering, the turn has passed to the next guest); `"A passed answer is one approval; three make an ally (T107)"` (targets {1,1}: three chats on three turns, then `group == 0` for both; a fourth guest cannot join when group 0 has 3); `"The bond moves an answer's target (T107)"` (`AnswerTarget` = row target − `persuade.bond` with bond > 0, + with bond < 0); `"Questions are not asked twice while unasked ones remain (T107)"` (five chats give five different ids); `"A bot may open a chat; it does not use the player's turn (T107)"` (bot with `chatty` 10 in the player's room: `OpenChat` chosen, `Current() == player`, after `Answer` `Current()` is the bot again). In `NightDataTests.cpp`: a guest with 4 questions is refused with code `night.questions`. `ChancePercent(3, 10) == 70`, `ChancePercent(6, 16) == 55`, `ChancePercent(0, 1) == 100`, `ChancePercent(0, 21) == 0`.
- [ ] **Step 2: Run** the Night tests — Expected: FAIL to build.
- [ ] **Step 3: Implement** as pinned; write the 35 questions; `styles.csv` gains `chatty` (explorer 4, fighter 3, loner 1, protector 6, schemer 7).
- [ ] **Step 4: Run** all Night tests and the balance case; update the golden fingerprint; if `night-soak` no longer holds, retune only data (`Balance.md` order) and record the numbers in `docs/testing/s25-night-simulation.md` §6.
- [ ] **Step 5: Commit** `issue #UMBRA-122 last-curtain: winning guests over by talking`

### Task 3: What the player sees (`board` module)

**Files:** Create `modules/board/CMakeLists.txt`, `include/BoardView.hpp`, `include/Board.md`, `src/BoardView.cpp`, `tests/BoardViewTests.cpp`; Modify `plugin.json` (module `board`, link `Umbra::NightBoard`), `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `Lit`, `Stat(Light)`, `Controls`, `IsTaken`, `LegalActions`, `Neighbour`, `Doorway`.
- Produces (namespace `umbra::night`, library `Umbra::NightBoard`, links `Umbra::Night` and glm):
  - `struct CellView { bool opened = false, lit = false; RoomKind kind = RoomKind::Plain; std::uint8_t doors = 0; std::uint8_t doorways = 0; /* bits: unopened room beyond */ std::int8_t step = -1; /* index in LegalActions of the Move into it */ };`
  - `struct PawnView { int guest = -1, cell = -1; bool player = false, ally = false, taken = false; };` `struct MonsterView { int monster = -1, cell = -1; bool minion = false; };`
  - `struct BoardView { std::array<CellView, maxCells> cells; std::vector<PawnView> pawns; std::vector<MonsterView> monsters; };`
  - `BoardView MakeView(const NightDef &def, const NightState &state, int viewer);` (viewer = the player's guest; -1 = see everything, used once the player's guest is dead)
  - `glm::vec3 CellCenter(const NightDef &def, int cell);` (2.4 m squares, the board centred on the origin, y = 0, row 0 at -z)
  - `int CellAt(const NightDef &def, glm::vec3 point);` (the cell under a point of the board plane, -1 outside)

Rules: a cell is lit when any guest the viewer `Controls` (alive, standing) lights it; `step` is filled only on the viewer's own turn (or an ally's) for legal `Move` actions; pawns and monsters appear only in lit cells; the Taken's pawn is `taken`; the viewer's own pawn always shows.

- [ ] **Step 1: Write the failing tests** `"Only lit rooms show who is in them (T108)"` (a guest two rooms beyond the light is not in `pawns`; one in light is), `"Allies' light counts (T108)"`, `"Minions and the Taken show only in light (T108)"`, `"Steps are the legal moves of the viewer's turn (T108)"` (each `step` cell is the destination of `LegalActions()[step]`; none during another guest's turn), `"Cells and points agree (T108)"` (`CellAt(def, CellCenter(def, c)) == c` for all 25, -1 outside), `"A dead player sees everything (T108)"`.
- [ ] **Step 2: Run** — Expected: FAIL to build.
- [ ] **Step 3: Implement** `BoardView.cpp`; `Board.md` explains the coordinates and the light rule.
- [ ] **Step 4: Run** — Expected: PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-123 last-curtain: the board as the player sees it`

### Task 4: Narrating a turn

**Files:** Create `include/Narrate.hpp`, `src/Narrate.cpp`, `assets/night/captions.json`, `tests/NarrateTests.cpp` (in `modules/board`)

**Interfaces:**
- Produces:
  - `struct Beat { int guest = -1; int camera = -1; /* cell */ std::vector<int> path; /* cells the pawn walks, seen part only */ std::string caption; /* "" = nothing to say */ std::string result; bool seen = false; };`
  - `struct Captions { std::map<std::string, std::string> lines; };` `bool LoadCaptions(const std::filesystem::path &file, Captions &out, std::vector<core::Diagnostic> &diagnostics);`
  - `Beat Narrate(const NightDef &def, const NightState &before, const NightState &after, const std::vector<LogEntry> &entries, const Action &action, int actor, int viewer, const Captions &captions);`

Rules: `seen` when the actor's start or end cell, or any opened room, is lit for the viewer; unseen beats keep only newly opened rooms (no caption, no path). `caption` is the decision (from `captions.json` keys `move`, `open`, `fight`, `omenTake`, `omenLeave`, `chat`, `endTurn`, `power`, `strike`, `guard`, `leave`, `item`, with `{guest}`, `{room}`, `{monster}`, `{omen}` filled in); `result` is the outcome from the log (`Check` → "rolled D + S against T: passed/failed", `Item`, `Down`, `Died`, `Hit`).

- [ ] **Step 1: Write the failing tests** `"A bot's turn in light has a caption, a path and a result (T109)"`, `"A turn in the dark shows nothing but the opened room (T109)"` (caption and path empty, `seen` false), `"Every action kind has a caption line (T109)"` (the shipped `captions.json` has every key; a missing key is a `night.captions` diagnostic).
- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement.** Captions ASCII, short, in the cast's voices where a guest speaks (templates only; plan 3 replaces them with the model).
- [ ] **Step 4: Run** — Expected: PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-124 last-curtain: narrated turns`

### Task 5: The program and the table

**Files:** Modify root `CMakeLists.txt` (one line after `umbra_add_plugin_folders(tools)`: `umbra_add_plugin_folders(apps)`, desktop builds only); Create `plugins/last-curtain/apps/CMakeLists.txt`, `apps/LastCurtain/CMakeLists.txt`, `Main.cpp`, `Screens.hpp/.cpp`, `Table.hpp/.cpp`, `Table.md`, `assets/ui/skin.json`, `assets/ui/fonts/` (copies of the rpg-ui plugin's Lora and Poppins with their OFL texts)

**Interfaces:**
- Consumes: `MakeView`, `CellCenter`, `CellAt` (Task 3).
- Produces:
  - `class Screens` (adapted from `apps/umbra/GameUi`: `UiSystem` loaded from the plugin's assets, the font atlas and images uploaded, `static umbra::ui::UiInput MapInput(...)`).
  - `class Table { public: explicit Table(umbra::render::Renderer &renderer); void Draw(const NightDef &def, const BoardView &view, umbra::core::DrawList &out) const; void Aim(glm::vec3 target, float seconds); void Update(float dt); umbra::core::Camera Camera() const; int Pick(const NightDef &def, glm::vec2 mouse, glm::vec2 pixels) const; void Zoom(float steps); };`
  - `LastCurtain.exe` arguments: `--seed N`, `--player <guest id>`, `--watch` (all seven are bots), `--capture <png>` (as `Umbra --capture`), `--frames N`.

Look (design, "Look"): room colours foyer `#C9A227`, plain `#6B7280`, item `#3B6FB6`, event `#7C4DBF`, trap `#B3261E`, omen `#1F1F24`, transport `#1F8A8A`, bonus `#2F8F5B`, under-stage `#7A1020`; known-but-dark rooms `#2A2A33`; floor tiles 2.2 m wide, 0.15 m thick, wall stubs where a side has no door; pawns 0.5 m cylinders-as-boxes coloured per guest; indoor environment, one warm point light over the board, ambient tinted red after the Turn. Camera tilted 50 degrees, looking at the acting guest's cell, easing over 0.6 s.

- [ ] **Step 1: Write the failing test** `"Picking hits the cell under the cursor (T110)"` in a new `tests/TableTests.cpp` that tests only the pure math moved to `board`: `int CellUnderRay(const NightDef &def, glm::vec3 origin, glm::vec3 direction)` (Produces, in `BoardView.hpp`) — a ray from above each `CellCenter` straight down returns that cell; a ray parallel to the board returns -1.
- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement** `CellUnderRay`, then the program: window 1280x720 titled "The Last Curtain", renderer, `Screens`, `Table`, a loop that with `--watch` plays bots one action per frame, `--capture` saves a frame and exits. Assets: the plugin's `assets` folder found by walking up from the program (as `FindFolders` does) or `assets/` next to it.
- [ ] **Step 4: Run** the board tests (PASS) and `TAIL=6 /home/claude/build.sh gcc` — Expected: `BUILD OK`, all tests pass; `LastCurtain --watch --seed 3 --frames 60 --capture /tmp/table.png` writes a PNG where SwiftShader is available (else: built, not run; say so).
- [ ] **Step 5: Commit** `issue #UMBRA-125 last-curtain: LastCurtain.exe draws the table`

### Task 6: The player's turn

**Files:** Create `apps/LastCurtain/Play.hpp/.cpp`, `assets/ui/hud.ui.json`, `assets/ui/actions.ui.json`, `assets/ui/bag.ui.json`, `assets/ui/sample.json`; Modify `Main.cpp`

**Interfaces:**
- Produces: `class Play { public: Play(const NightDef &def, std::uint64_t seed, int player); bool PlayerTurn() const; void Click(int cell); void Key(umbra::platform::InputKey key); bool HandleUi(const std::string &screen, const std::string &action, const std::string &arg); nlohmann::json UiData() const; std::string Screen() const; void Update(float dt); const NightState &State() const; };`
- Screen data keys (bind names for the `.ui.json` files): `clock`, `round`, `phase`, `me{name,hp,maxHp,might,sanity,speed,level,buffs}`, `moves`, `actions[{label,action,arg}]`, `allies[{name,hp,maxHp}]`, `winning[{name,approvals,goal}]`, `inLight[{name,room}]`, `bag[{name,kind,text,canUse,slot}]`, `floor[{name,text,index}]`, `bagUsed`, `carry`.

Rules: clicks and arrow keys do something only when `PlayerTurn()` and the target is a `step` cell (Review Focus 4); `actions` lists every non-move legal action with labels (`End turn` always last); `End turn` (Enter) works with moves left; allies are played the same way on their turns (`Controls`).

- [ ] **Step 1: Write the failing tests** in `tests/PlayTests.cpp` against `Play` without a window (the class takes no renderer): `"A click on a step cell moves; elsewhere nothing (T111)"`, `"End turn works with moves left (T111)"`, `"An ally is the player's to move (T111)"`, `"UiData has the keys the screens bind (T111)"` (every key above present; `actions` ends with End turn).
- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement** `Play` (in a small library `Umbra::NightPlay` under `apps/LastCurtain/` so the tests link it) and the three screens per the mockup "Your turn" and "Bag and item use".
- [ ] **Step 4: Run** — Expected: PASS; `UmbraTool ui-preview` renders the three screens from `sample.json` without diagnostics.
- [ ] **Step 5: Commit** `issue #UMBRA-126 last-curtain: the player's turn`

### Task 7: Bot turns on screen

**Files:** Modify `Play.hpp/.cpp`, `Main.cpp`; Create `assets/ui/caption.ui.json`

**Interfaces:**
- Consumes: `ChooseAction` (nightbots), `Narrate`, `Captions`.
- Produces: `Play::Update(float dt)` plays bots: `ChooseAction`, `Apply`, `Narrate`; a seen beat holds for 1.5 s (Space: 0.3 s) with the caption screen and the camera on `Beat::camera`; an unseen beat takes 0.2 s; `UiData` gains `caption{guest,line,result}`.

- [ ] **Step 1: Write the failing tests** `"Bot turns advance with time and hold on seen beats (T112)"` (after `Update` of 0.5 s a seen beat is still showing; after 1.6 s the next bot acts), `"A minimised window does not lose a beat (T112)"` (`Update(0)` many times changes nothing).
- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement**; the camera follows `Beat::camera` via `Table::Aim`.
- [ ] **Step 4: Run** — Expected: PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-127 last-curtain: bot turns on screen`

### Task 8: Rolls, events, omens and chats on screen

**Files:** Create `assets/ui/chat.ui.json`, `event.ui.json`, `dice.ui.json`, `omen.ui.json`; Modify `Play.cpp`, `events.csv` (a `text` column with the event's short story, ASCII), `NightDef.hpp`/`NightData.cpp` (read it)

**Interfaces:**
- Produces: `UiData` keys `chat{name,question,approvals,goal,bondNote,answers[{label,stat,statValue,target,chance}]}`, `event{room,name,text,stat,statValue,target,chance,pass,fail}`, `dice{die,stat,statValue,total,target,passed,result}`, `omen{name,gift,target,chance,taken}`; `Screen()` names `chat`, `event`, `dice`, `omen` when they apply.

Rules: entering an event room shows `event` first with the roll's chance; `Roll` applies the move's resolution (the rules already rolled it: the screen shows the die from the log entry, not a new roll); every `Check` the player makes shows `dice`; the omen screen shows "below N it takes you: P%" with N = `CurseTarget` and P = `(N - 1) * 5` clamped to 0..100; a pending chat or omen closes when the night ends (Review Focus 1).

- [ ] **Step 1: Write the failing tests** `"Every chance shown matches the rules (T113)"` (chat answers `ChancePercent(stat, AnswerTarget)`; omen risk `(CurseTarget - 1) * 5` clamped; event `ChancePercent(stat, target)`), `"The dice screen shows the logged roll (T113)"`, `"No screen waits after the night ended (T113)"`.
- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement**; screens per the mockups "Chat", "Event", "Dice roll", "Omen found". Event texts for the 18 rows written to the mockup's tone.
- [ ] **Step 4: Run** — Expected: PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-128 last-curtain: rolls, events, omens and chats on screen`

### Task 9: Battles on screen

**Files:** Create `assets/ui/battle.ui.json`; Modify `Play.cpp`

**Interfaces:**
- Produces: `UiData.battle{room,round,rounds,company[{name,hp,maxHp,buffs,acting}],enemies[{name,hp,maxHp,note,strike,index}],canLeave,last}`; `strike` = `Damage(def, state, actingFighter, enemy)`.

- [ ] **Step 1: Write the failing tests** `"The battle data lists both sides and the expected damage (T114)"`, `"Leave is offered from round 2 only (T114)"`, `"The player's death mid-battle ends the screen and the watching begins (T114)"`.
- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement**; screen per the mockup "Battle panel"; buff names from `buffs.csv`.
- [ ] **Step 4: Run** — Expected: PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-129 last-curtain: battles on screen`

### Task 10: Start, the Turn, the end, and saves

**Files:** Create `assets/ui/start.ui.json`, `turn.ui.json`, `end.ui.json`; Modify `Play.cpp`, `Main.cpp`

**Interfaces:**
- Produces: `UiData.start{guests[{id,name,style,hp,might,sanity,speed}],canContinue}`, `UiData.turn{who,form,hp,power,minions,clock}`, `UiData.end{outcome,lines[]}`; saves at the start of each round and on quit to `PreferencesFolder()/last-curtain/night.json` via `SaveNight`; Continue loads with `LoadNightState` and is hidden when it refuses (Review Focus 3).

- [ ] **Step 1: Write the failing tests** `"A save and Continue give the same night (T115)"`, `"A save from other data hides Continue (T115)"`, `"The Turn screen names the form and its numbers (T115)"`, `"The player as the Taken plays on (T115)"` (marked = player: `PlayerTurn()` true on the Taken's turns; the view is the Taken's light).
- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement**; screens per the mockups "The curse takes a guest" and the design's start and end lines.
- [ ] **Step 4: Run** — Expected: PASS; `bash /home/claude/verify.sh` — Expected: `BUILD OK` and all tests passed for gcc, clang and asan.
- [ ] **Step 5: Commit** `issue #UMBRA-130 last-curtain: start, the Turn, the end and saves`

### Task 11: Records and the Windows check

**Files:** Create `docs/testing/s26-board-on-screen.md`; Modify `docs/ROADMAP.md`, `docs/current-engine-status.md`, `docs/CONTEXT.md`, `CLAUDE.md`, `docs/design/HANDOFF.md`, `apps/LastCurtain/README.md` (how to build and run)

- [ ] **Step 1: Write the record:** what was built, commands and results, the soak numbers after the conversation rule, and a Windows checklist (x64 shell): build `LastCurtain`; play a night start to end as one guest; win someone over by chat; an event push-back; take an omen; a battle; see the Turn; quit and Continue; resize during a bot turn.
- [ ] **Step 2: Update** the dashboard, status, CONTEXT, CLAUDE.md first-use line, HANDOFF.
- [ ] **Step 3: Commit** `issue #UMBRA-131 docs: plan 2 records and the Windows check`
