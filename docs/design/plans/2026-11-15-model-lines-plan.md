# The model's lines (plan 3) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **Status:** draft for the user's review. Not started. The open questions in the spec are answered first.

**Goal:** the built-in model rewords captions, chat questions, goal lines, the Turn and the Taken's lines in each guest's voice, with the written templates as the fallback.

**Architecture:** a new `nightmind` module in the last-curtain plugin builds requests (prompt + grammar + seed) and guards answers; it depends on the living town's `Mind` interface only. `Play` asks for lines through a small `Words` object that owns a worker thread, a bounded job list and a deadline; `Play` shows whatever is ready when the line is due. Only `LastCurtain.exe` links `LlamaMind`.

**Tech Stack:** C++20, llama.cpp (vcpkg feature `town-model-gpu`), Catch2, the scripted stand-in mind.

**Spec:** `docs/design/2026-11-15-model-lines.md`

## Global Constraints

- The model never changes `NightState`; a night with and without it ends in the same state.
- Template first, always ready; late (> 1.0 s [call]), empty or refused → template. A shown line is never swapped.
- Grammar: one line, `[A-Z]`, letters/spaces/`,'-`, 8-120 chars, ends `.!?`; no digits.
- The `result` line, chat answers, numbers and names are never reworded.
- Seed per request = night seed + beat number.
- `plugins/last-curtain/plugin.json` `requires` gains `living-town`; the rules modules (`night`, `nightbots`, `board`) do not link the model.
- Model file `models/night.gguf`, never committed; missing → one log line + pause-menu note.
- Code style: short functions, `// 1.` step comments, a `.md` beside the module; commits `issue #UMBRA-<n> ...`; C++ literals ASCII.

## Review Focus

1. A model that answers after the line was shown → the answer is dropped, not shown late (test in Task 3).
2. The night ends while jobs are queued → shutdown waits for at most the running job, drops the rest (Task 3).
3. A guest's name with a diacritic-free spelling the model "corrects" (e.g. "Thanh" → "Than") → refused (Task 2).
4. A Taken line that uses the human name instead of the form's → allowed only if the template has it (Task 2).
5. Loading a save mid-night with the model on → the next lines' seeds continue from the saved beat count (Task 4).

---

### Task 1: Voices in the data

**Files:** Modify `plugins/last-curtain/assets/night/guests.csv`, `modules/night/include/NightDef.hpp`, `modules/night/src/NightData.cpp`; Test `tests/NightDataTests.cpp`

**Interfaces:** Produces `GuestDef::voice`, `GuestDef::takenVoice` (`std::string`, required, 3-60 chars).

- [ ] Test `"Every guest has a voice and a Taken voice (T132)"`: Thanh's voice is `"theatrical, warm, proud"`; a blank `takenVoice` is refused with field `guests.csv:1:takenVoice`.
- [ ] Run, RED. Add the columns (the spec's table) and the loader lines. Run, GREEN. Commit.

### Task 2: Requests and the guard

**Files:** Create `plugins/last-curtain/modules/nightmind/{CMakeLists.txt, include/NightMind.hpp, include/NightMind.md, src/NightMind.cpp}`; Test `tests/NightMindTests.cpp`

**Interfaces:**
- Consumes `umbra::town::MindRequest` (`Mind.hpp`).
- Produces:
  - `enum class LineKind { Caption, Question, GoalDone, GoalFailed, Turn, Taken };`
  - `struct LineAsk { LineKind kind; int speaker; bool taken; std::vector<int> about; std::string line; std::uint32_t seed; };`
  - `umbra::town::MindRequest Request(const NightDef &, const LineAsk &);` (three worked examples per kind, tone from `voice`/`takenVoice`, `About:` with pronouns)
  - `std::string Guard(const NightDef &, const LineAsk &, const std::string &answer);` returns the cleaned line or `""`.

- [ ] Tests (T133): the request names the speaker's tone and every `about` guest with their pronoun; the grammar is the spec's. Guard refuses: a dropped name, an added guest name, a wrong pronoun, a digit, a Question that names "Might"/"Sanity"/"easy"/"hard", a Caption with "dies"/"misses"/"hits" not in the template; accepts a clean rewording.
- [ ] RED, implement (port the town `LineGuard` checks that apply, plus the two new ones), GREEN, commit.

### Task 3: Words: the worker, the deadline and the fallback

**Files:** Create `plugins/last-curtain/apps/LastCurtain/Words.{hpp,cpp}` (in `UmbraNightPlay`); Test `tests/WordsTests.cpp`

**Interfaces:** Produces `class Words { Words(const NightDef &, std::unique_ptr<umbra::town::Mind>); int Ask(LineAsk); std::string Take(int ticket, const std::string &fallback); void Stop(); };` — `Take` returns the guarded answer if it arrived, else the fallback, and forgets the ticket. A null mind answers every `Take` with the fallback.

- [ ] Tests (T134) with the scripted stand-in: a fast answer is taken; a slow one (sleeps past the deadline) gives the fallback and is not shown later; an empty or refused answer gives the fallback; at most 4 jobs wait (the oldest is dropped); `Stop` with jobs queued returns within the running job's time.
- [ ] RED, implement (one thread, `std::deque` of jobs, `std::condition_variable`), GREEN, commit.

### Task 4: Play asks and shows

**Files:** Modify `Play.{hpp,cpp}`; Test `tests/PlayTests.cpp`

**Interfaces:** Consumes `Words`. Produces `void Play::SetWords(Words *words)`; the beat's caption, chat question, goal line, Turn screen text and Taken captions come from `Words::Take` at the moment they are shown; tickets are asked when the beat is made (bots: at decision, so the thinking pause covers the wait).

- [ ] Tests (T135): with a stand-in that uppercases the first word, a bot's caption in light shows the model's line and the `result` line is the template's; the same night with and without `Words` ends in the same `Fingerprint(State())` (`NightSave.hpp`); the Turn screen shows two worded sentences; a Restore continues seeds from the saved beat count.
- [ ] RED, implement, GREEN, commit.

### Task 5: The real model in the program

**Files:** Modify `apps/LastCurtain/{Main.cpp, CMakeLists.txt}`, `plugins/last-curtain/plugin.json`, `ui/pause.ui.json`

- [ ] `Main` makes a `LlamaMind` from `models/night.gguf` (beside the program, else the source tree), warms it up on the worker, and falls back to no mind with one log line; the pause menu shows "Written lines: the model is not installed" when there is none.
- [ ] Capture with no model: unchanged look; log line present. Commit.

### Task 6: The real-model check tool

**Files:** Create `plugins/last-curtain/tool/NightMindCommand.cpp`: `UMBRA_TOOL_COMMAND(NightMind, "night-mind", ...)` as the living town's `tool/TownCommand.cpp` does (`--model`, `--nights 3`, `--show-prompt N`).

- [ ] `UmbraTool night-mind --model x.gguf --nights 3` plays three bot nights and prints each template, worded line, verdict and seconds. With the stand-in in a test: prints 3 nights' lines, exit 0. Commit.

### Task 7: Records

- [ ] `docs/testing/s29-model-lines.md` (section 14 template): headless results; §4 the user's Windows check: accepted count of 30, seconds per line, a read for voice. ROADMAP, current-engine-status, HANDOFF. Commit.
