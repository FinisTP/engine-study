# Campaign plan 1: the Vietnamese model test, the voice test, the fonts — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the user the tools to decide whether a local model can word The Last Curtain's café lines in Vietnamese (Arcee-VyLinh first, then a side-by-side comparison), a script to hear the authored lines in VieNeu voices (Tí pitched up), and screen fonts that carry every Vietnamese letter.

**Architecture:** A new headless module `talkmind` in `plugins/last-curtain` holds the test set loader, the prompt, the Vietnamese line checks and the report writer; it uses the living town's `town::Mind` interface, so tests run a stand-in model and only the tool links llama.cpp. The tool command `UmbraTool talk-mind` runs a GGUF over the test set and writes one report per model, plus a comparison of several reports. The voice script is Python outside the game, talking to the user's own VieNeu at `127.0.0.1:7860`. The fonts swap is data (`skin.json`) plus one small engine helper to prove glyph coverage in tests.

**Tech Stack:** C++20, CMake, Catch2, nlohmann/json, llama.cpp (pinned, vcpkg features `town-model` / `town-model-gpu`), stb_truetype (engine UI), Python 3 with `gradio_client`, `numpy`, `soundfile`, `pyworld`.

**Spec:** `docs/design/2026-11-23-campaign-slice-1.md` §7.8 (and §8 step 1), summarised in the Project's `claude/last-curtain/campaign-slice-1-handoff-2026-11-23.md` §7 and §9.1. Voice sheets: `docs/design/cast-2-2026-11-22.md` §0b, §5 and `campaign-content-2026-11-22.md` §1.1 (Bé Tí).

## Global Constraints

- Code style (CLAUDE.md): short, simple C++; `// 1.` step comments only where needed; deeper rules in a `.md` beside the headers; `umbra::<module>` namespace, `PascalCase`, headers by bare name, one `Umbra::<Module>` library per module, Catch2 tests in `plugins/last-curtain/tests/`.
- **C++ string and char literals stay ASCII** (MSVC reads sources in the local code page). Vietnamese lives only in UTF-8 data files. Code points in C++ are written as `U'Ạ'` / `0x1EA0`.
- Every number and every word the checks use is data, not code.
- Model files go in `projects/<project>/models/` or are passed by path, and are **never committed** (`*.gguf`). The cloud cannot download models: tests use a scripted stand-in `town::Mind`.
- A newer llama.cpp is a separate, reviewed change: this plan uses the pinned one.
- Pass rule (accepted): the user is satisfied **and at least 90%** of lines pass the automatic checks. A candidate must also load in the pinned llama.cpp, fit **about 3 GB** of video memory beside the game, and answer in **under about 1.5 s a line**. **The user picks the winner.**
- Voice: VieNeu-TTS v3 Turbo only, local, at `http://127.0.0.1:7860`; no ComfyUI voice nodes (paid cloud); **no cloning of real people**. Voices: Tí and Bà Mai Central (Quang Sơn / Ngọc Trân), Bảo Southern (Thái Sơn), Lệ Thanh the user's pick between Ngọc Linh and Thục Đoan. Tí: a preset pitched **+3 to +5 semitones** with formant correction; the user judges.
- Fonts (accepted): **Be Vietnam Pro** (body) and **Playfair Display** (titles), SIL OFL 1.1, pinned to an exact upstream commit, licence file beside each font, listed in `THIRD_PARTY_NOTICES.md`. Lora stays.
- Commits: `issue #UMBRA-<n> <summary>` from **#324**, ending with the session's `Co-Authored-By` and `Claude-Session` lines; stage files by name, never `git commit -a`; never commit `projects/reference/assets/scenes/settlement.scene.json` or any `*.gguf`.
- Never report a Windows, GPU or listening check as passed from the cloud.

## Review Focus

1. **A model that answers in English or mixes languages** ("Dạ, hello chị!") must fail the line, not slip through because most words carry diacritics. Test in Task 3: `"Chào chị, nice to meet you."` fails with reason `english`.
2. **A model that adds a memory Tí has not regained yet** (mentions the white sand in visit 1) must fail: this is the "model invents a memory fragment" anti-goal. Test in Task 3: a visit-1 situation whose `forbid` holds `cát trắng` rejects a line containing `Cát trắng`.
3. **Upper-case or decomposed Vietnamese** in a model line (`RỨA`, or `ư` written as `u` + U+031B) must not dodge the never-say list or the dialect check. Test in Task 3: never-say matching on `"ĐỒ NGỐC"` against `đồ ngốc`; decomposed marks are counted as a `bad-text` failure (the screen can't draw them; the engine joins no marks).
4. **A model file the pinned llama.cpp can't load** (newer architecture) must give a clear report, not a crash or an empty table. Test in Task 5: the tool with a non-GGUF file prints `cannot load` and writes a report whose verdict says so, exit code 1.
5. **A broken or half-edited test set** (a situation naming an unknown speaker, a missing `authored` line) must stop with file, field and fix before any model time is spent. Test in Task 2.

---

## File structure

| File | Responsibility |
|---|---|
| `engine/ui/include/Font.hpp`, `engine/ui/src/Font.cpp` | + `MissingGlyphs`: which code points a TTF lacks |
| `tests/ui/UiTests.cpp` | its test |
| `plugins/last-curtain/apps/LastCurtain/assets/ui/fonts/` | + `BeVietnamPro-Regular.ttf`, `BeVietnamPro-Medium.ttf`, `BeVietnamPro-OFL.txt`, `PlayfairDisplay-Bold.ttf`, `PlayfairDisplay-OFL.txt`, `README.md` (source, commit, SHA-256); − Poppins and Abril Fatface files |
| `plugins/last-curtain/apps/LastCurtain/assets/ui/skin.json` | font roles point at the new files |
| `plugins/last-curtain/tests/ArtTests.cpp` | every skin font covers Vietnamese |
| `plugins/last-curtain/assets/campaign/voices/{ti,bao,mai,lethanh}.json` | voice sheets (Tí with his four fragments) |
| `plugins/last-curtain/assets/campaign/model-test/situations.json` | the fixed test set |
| `plugins/last-curtain/assets/campaign/model-test/rules.json` | thresholds, global never-say list, English words, dialect markers |
| `plugins/last-curtain/assets/campaign/model-test/prompt.txt` | the prompt template |
| `plugins/last-curtain/modules/talkmind/` | `CMakeLists.txt`, `include/TalkMind.hpp`, `include/TalkMind.md`, `src/TalkData.cpp` (loading), `src/TalkCheck.cpp` (text and checks), `src/TalkRun.cpp` (request, run, report) |
| `plugins/last-curtain/plugin.json` | + module `talkmind`, link `Umbra::TalkMind` |
| `plugins/last-curtain/tests/TalkMindTests.cpp`, `tests/CMakeLists.txt` | new test executable `UmbraTalkMindTests`, CTest `Umbra.TalkMind` |
| `plugins/living-town/modules/mind/include/LlamaMind.hpp`, `src/LlamaMind.cpp` | + `LlamaGpuFreeBytes` |
| `plugins/last-curtain/tool/TalkMindCommand.cpp` | `UmbraTool talk-mind` |
| `tools/voice/vieneu_lines.py`, `tools/voice/test_vieneu_lines.py`, `tools/voice/README.md` | the voice test |
| `docs/testing/s44-vietnamese-model.md`, `docs/ROADMAP.md`, `docs/current-engine-status.md`, `CLAUDE.md` | records |

---

### Task 1: Fonts that carry Vietnamese (#324)

**Files:**
- Modify: `engine/ui/include/Font.hpp`, `engine/ui/src/Font.cpp`, `tests/ui/UiTests.cpp`
- Create: the five font files and `fonts/README.md` listed above; delete `Poppins-Regular.ttf`, `Poppins-Medium.ttf`, `Poppins-OFL.txt`, `AbrilFatface-Regular.ttf`, `AbrilFatface-OFL.txt` from `apps/LastCurtain/assets/ui/fonts/` (only there; the rpg-ui plugin's Poppins stays)
- Modify: `apps/LastCurtain/assets/ui/skin.json`, `THIRD_PARTY_NOTICES.md`, `plugins/last-curtain/tests/ArtTests.cpp`

**Interfaces:**
- Produces: `std::u32string umbra::ui::MissingGlyphs(const std::vector<unsigned char> &ttf, const std::u32string &wanted);` — the code points of `wanted` the font has no glyph for (`stbtt_FindGlyphIndex == 0`); all of `wanted` if the file is not a font.

- [ ] **Step 1: Write the failing engine test** in `tests/ui/UiTests.cpp`:

```cpp
TEST_CASE("MissingGlyphs names the letters a font lacks (campaign plan 1)", "[ui][font]")
{
    const auto lora = ReadBytes(/* the Lora.ttf the existing font test uses */);
    CHECK(umbra::ui::MissingGlyphs(lora, U"Aượ").empty());           // A, u-horn, o-horn-dot
    CHECK(umbra::ui::MissingGlyphs(lora, U"中") == U"中");            // a CJK letter Lora lacks
    CHECK(umbra::ui::MissingGlyphs({1, 2, 3}, U"ab") == U"ab");               // not a font
}
```

- [ ] **Step 2:** Run `ctest -R Umbra.Ui` (or the UI test target) — expect a compile failure: `MissingGlyphs` not declared.
- [ ] **Step 3: Implement `MissingGlyphs`** in `Font.cpp` with the same 4-byte tag check `FontAtlas::Build` uses before `stbtt_InitFont`.
- [ ] **Step 4:** Run the UI tests — PASS.
- [ ] **Step 5: Fetch the fonts, pinned.** In the cloud, `git clone` `https://github.com/google/fonts` and take `ofl/bevietnampro/BeVietnamPro-Regular.ttf`, `BeVietnamPro-Medium.ttf`, `OFL.txt`. For titles take a **static Bold** Playfair Display from the upstream project `https://github.com/clauseggers/Playfair` (a tagged release's static TTF) if it has one; if only the variable font exists, use `google/fonts` `ofl/playfairdisplay/PlayfairDisplay[wght].ttf` as is (stb_truetype draws its default instance) and say so in the README. Before making any static instance yourself, read its `OFL.txt` for a Reserved Font Name; if there is one, don't instance it. Write `fonts/README.md`: each file, its source URL, the exact commit or tag, SHA-256, licence.
- [ ] **Step 6: Point `skin.json` at them:** `body`, `small` → `BeVietnamPro-Regular.ttf`; `strong`, `tag` → `BeVietnamPro-Medium.ttf`; `title`, `essence` → the Playfair file; `heading` stays Lora. Sizes unchanged. Remove the Poppins and Abril files from this folder; grep the LastCurtain app and its `assets/` for any other reference to them (expect none besides `skin.json`).
- [ ] **Step 7: Write the failing coverage test** in `ArtTests.cpp`:

```cpp
TEST_CASE("Every font in the Last Curtain skin draws every Vietnamese letter (campaign plan 1)", "[night][art]")
{
    // 1. The letters: 1EA0-1EF9, plus the Vietnamese ones outside that range.
    std::u32string vietnamese = U"ĂăÂâĐđÊêÔôƠơƯư"
                                U"ÀÁÃÈÉÌÍÒÓÕÙÚÝ"
                                U"àáãèéìíòóõùúýĨĩŨũ";
    for (char32_t c = 0x1EA0; c <= 0x1EF9; ++c)
        vietnamese += c;
    // 2. Each "file" in skin.json's "fonts".
    for (const auto &[role, face] : /* skin.json "fonts" */)
        CHECK(umbra::ui::MissingGlyphs(ReadBytes(assets / face["file"]), vietnamese).empty());
}
```

- [ ] **Step 8:** Run it against the old skin (temporarily, or before Step 6 if done in order) to see it FAIL on Poppins; with the new skin, PASS.
- [ ] **Step 9: Notices.** In `THIRD_PARTY_NOTICES.md` add a row for `plugins/last-curtain/apps/LastCurtain/assets/ui/fonts/BeVietnamPro-*.ttf`, the Playfair file, and the Last Curtain copy of `Lora.ttf`: origin and commit as in the README, SIL OFL 1.1. (The Abril Fatface file had no row; it is gone now.)
- [ ] **Step 10: Look at it.** Write a throwaway data file with Vietnamese in the `start` screen's text fields (`"Bức màn cuối"`, `"Trò chơi mới"`, `"Tiếp tục"`, `"Thoát"`, and the body text `"Ưu tiên: ắ ợ ậ ữ ỹ Đ"`), then run `UmbraTool ui-preview start --assets plugins/last-curtain/apps/LastCurtain/assets --data <that file> --out captures/2026-11-23-vietnamese-fonts.png`. Open the PNG: no `?` boxes, marks sit on their letters, titles read. Keep the capture; don't commit the throwaway data file.
- [ ] **Step 11:** Build `UmbraNightTests` and the UI tests; run `ctest -R "Umbra.Ui|Umbra.Night$"` — PASS.
- [ ] **Step 12: Commit** `issue #UMBRA-324 Last Curtain fonts: Be Vietnam Pro and Playfair Display carry every Vietnamese letter; MissingGlyphs checks it` (the font files, README, skin, notices, the two tests, `Font.*`, the capture; the deletions with `git rm`).

---

### Task 2: Voice sheets and the test set as data (#325)

**Files:**
- Create: `assets/campaign/voices/ti.json`, `bao.json`, `mai.json`, `lethanh.json`; `assets/campaign/model-test/situations.json`, `rules.json`, `prompt.txt`
- Create: `modules/talkmind/CMakeLists.txt`, `include/TalkMind.hpp`, `include/TalkMind.md`, `src/TalkData.cpp`
- Modify: `plugin.json`, `tests/CMakeLists.txt`; Create: `tests/TalkMindTests.cpp`

**Interfaces:**
- Produces (in `TalkMind.hpp`, namespace `umbra::talk`):

```cpp
struct Fragment { std::string id, text; };                 // one memory, Vietnamese
struct Voice
{
    std::string id, name, dialect;                         // dialect: a key of TalkRules::dialects
    std::string sheet;                                     // register, habits, topics: one paragraph for the prompt
    std::vector<std::string> markers, never;               // own dialect words; words this voice never says
    std::vector<std::string> tts;                          // VieNeu preset names to try
    std::vector<Fragment> fragments;
};
struct Situation
{
    std::string id, speaker, task, authored;               // task: what the line must do; authored: the fallback line
    std::vector<std::string> facts, must, forbid;          // facts for the prompt; words that must / must not appear
};
struct TalkRules
{
    int minLength = 8, maxLength = 160;                    // code points
    double markedShare = 0.33, passShare = 0.9;            // words with Vietnamese letters; lines that must pass
    int maxMs = 1500, maxVideoMb = 3072, seeds = 3, maxTokens = 64;
    std::vector<std::string> never, english;               // for every speaker; English words
    std::map<std::string, std::vector<std::string>> dialects; // "central" -> {"răng", ...}
};
struct TestSet
{
    std::vector<Voice> voices;
    std::vector<Situation> situations;
    TalkRules rules;
    std::string prompt;                                    // prompt.txt
    [[nodiscard]] const Voice *Find(const std::string &id) const;
};
// Reads assets/campaign (voices/*.json, model-test/*). False with diagnostics naming file, field and fix.
bool LoadTestSet(const std::filesystem::path &campaign, TestSet &set, std::vector<core::Diagnostic> &diagnostics);
```

**Data shapes** (format/version on every JSON file: `"format": "umbra.lc-voice"` / `"umbra.lc-model-test"` / `"umbra.lc-model-rules"`, `"version": 1`):

```json
// voices/ti.json
{ "format": "umbra.lc-voice", "version": 1, "id": "ti", "name": "Bé Tí", "dialect": "central",
  "sheet": "Cậu bé 12 tuổi ở Quảng Ngãi, chạy liên lạc giữa các xóm. Câu ngắn, tò mò, hơi lém. Gọi người chơi là {anh/chị}. Hay nói về món mạ nấu, Tết, biển.",
  "markers": ["tui", "răng", "rứa", "mạ", "ni"], "never": ["giết", "chửi"],
  "tts": ["Quang Sơn", "Ngọc Trân"],
  "fragments": [ { "id": "mother", "text": "..." }, { "id": "tet", "text": "..." },
                 { "id": "message", "text": "Tụi nó tới lúc rạng đông." }, { "id": "sand", "text": "..." } ] }

// model-test/situations.json: { "format": ..., "version": 1, "situations": [
  { "id": "ti-visit1-mother", "speaker": "ti",
    "task": "Tí vừa nhớ ra tên mạ, kể cho {anh/chị} nghe, vui mừng.",
    "facts": ["<fragment mother text>"], "must": ["mạ"], "forbid": ["cát trắng", "rạng đông"],
    "authored": "Chị ơi, con nhớ ra rồi! Mạ con tên ..." } ] }
```

The test set: **5 situations per speaker, 20 in all**, spread over the café cases the slice has — a greeting, Tí regaining each of two fragments per visit (`forbid` holds the fragments not yet restored), Tí mentioning how he was freed (a fight fact), Bảo deflecting a question about the 1944 night, Bà Mai scolding kindly ("con"), Lệ Thanh as hostess and one narration line about her dance. Bảo: `dialect` `southern`, markers e.g. `nghen`, `vậy đó`, `tui`; Bà Mai and Lệ Thanh as their sheets say (Mai Central living in Saigon: markers `con`, `mô`, `răng`; Lệ Thanh: the implementer proposes, flagged for the user). `rules.json` holds the numbers above, a global `never` list (swearing, `AI`, `trò chơi`, `người chơi`), the `english` list (common English words that are **not** also Vietnamese spellings: `the, and, you, your, is, are, was, what, this, that, with, have, please, sorry, thank, thanks, yes, hello, okay, ok, my, me, it, of` — never `to`, `no`, `in`, `an`, `con`), and `dialects` (`central`: `răng, rứa, mô, tê, ni, chi, mạ, hỉ`; `southern`: `nghen, hông, nè, dzậy, ổng, bả`; `northern`: `thế, nhỉ`). **All Vietnamese here is Claude's draft for the user's native review**; mark it so in `TalkMind.md`.

`prompt.txt` is the template, in Vietnamese, with placeholders `{name}`, `{sheet}`, `{facts}`, `{task}`, `{authored}`: ask for one line, in the speaker's voice and dialect, Vietnamese only, keeping every fact, adding none, keeping `{player}` and `{anh/chị}` tokens exactly, no quotes.

- [ ] **Step 1: Write the failing tests** in `TalkMindTests.cpp` (copy the repo's data to a temp folder for the broken cases, as `MatchDataTests.cpp` does):

```cpp
TEST_CASE("The model test set loads: four voices, twenty situations (campaign plan 1)", "[talk]")
{
    TestSet set; std::vector<umbra::core::Diagnostic> d;
    REQUIRE(LoadTestSet(UMBRA_CAMPAIGN_DATA, set, d));
    CHECK(set.voices.size() == 4);
    CHECK(set.situations.size() == 20);
    CHECK(set.Find("ti")->fragments.size() == 4);
    CHECK(set.rules.passShare == Catch::Approx(0.9));
    CHECK(set.prompt.find("{authored}") != std::string::npos);
}
TEST_CASE("A broken test set names the file, the field and the fix", "[talk]")
{
    // 1. speaker "nobody" -> a diagnostic naming situations.json, "speaker", and the known ids
    // 2. a situation without "authored" -> situations.json, "authored"
    // 3. a voice whose dialect is not in rules.json "dialects" -> voices/<id>.json, "dialect"
    // 4. a prompt.txt missing "{authored}" -> prompt.txt
    // each: LoadTestSet returns false
}
```

- [ ] **Step 2:** Configure and build `UmbraTalkMindTests` — expect FAIL (module missing).
- [ ] **Step 3: Implement `LoadTestSet`** in `src/TalkData.cpp` with nlohmann/json, the module's `CMakeLists.txt` modelled on `nightmind`'s (PUBLIC `Umbra::Core`, `Umbra::TownMind`; PRIVATE `Umbra::Warnings`, `Umbra::Sanitizers`), `plugin.json` (+ `talkmind`, + `Umbra::TalkMind`), and the test target in `tests/CMakeLists.txt` with `UMBRA_CAMPAIGN_DATA="${CMAKE_CURRENT_SOURCE_DIR}/../assets/campaign"` and `add_test(NAME Umbra.TalkMind COMMAND UmbraTalkMindTests)`. Write the data files.
- [ ] **Step 4:** Run `ctest -R Umbra.TalkMind` — PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-325 Campaign model test: voice sheets for Ti, Bao, Ba Mai and Le Thanh, a 20-line test set and its loader (Vietnamese drafted for the user's review)`.

---

### Task 3: The Vietnamese line checks (#326)

**Files:** Create `modules/talkmind/src/TalkCheck.cpp`; modify `TalkMind.hpp`, `TalkMind.md` (section "The checks", in order), `tests/TalkMindTests.cpp`.

**Interfaces:**
- Produces:

```cpp
[[nodiscard]] std::string Lower(const std::string &utf8);           // Vietnamese-aware lower case
[[nodiscard]] std::vector<std::string> Words(const std::string &utf8); // letter runs, lower case, {tokens} dropped
[[nodiscard]] std::string Tidy(std::string line);                    // trims spaces and wrapping quotes (ASCII and U+201C/U+201D)
// "" = passes; else the first objection's code: "empty", "bad-text", "length", "no-marks",
// "english", "never", "dialect", "token", "missing-fact", "added-memory".
[[nodiscard]] std::string Check(const TestSet &set, const Situation &situation, const std::string &line);
```

`Lower` maps ASCII A–Z, Latin-1 `0xC0–0xDE` (not `0xD7`) +0x20, the Latin Extended pairs Vietnamese uses (`Ă/ă 0x102`, `Đ/đ 0x110`, `Ĩ 0x128`, `Ũ 0x168`, `Ơ 0x1A0`, `Ư 0x1AF`: upper → upper+1), and `0x1EA0–0x1EF9` (even → +1). Matching of `never`, `english`, `must`, `forbid` and markers is on whole words (a phrase matches a run of whole words) after `Lower`.

The checks, in order (first objection wins): `empty`; `bad-text` (invalid UTF-8, a newline, a combining mark U+0300–U+036F, a digit run longer than 4, or any of `<>*#_`); `length` (code points outside `minLength..maxLength`); `no-marks` (fewer than `markedShare` of words hold a code point ≥ 0xC0); `english` (a word in `rules.english`, or an ASCII word containing `f`, `j`, `w` or `z`); `never` (rules' or the speaker's); `dialect` (a marker of another dialect that is not in the speaker's own `markers`); `token` (a `{…}` other than `{player}`, `{anh/chị}`, `{Anh/Chị}`); `missing-fact` (a `must` entry absent); `added-memory` (a `forbid` entry present).

- [ ] **Step 1: Write the failing tests** — one `TEST_CASE` per check, with lines in a UTF-8 data file `tests/data/talk-lines.json` (C++ stays ASCII), keyed by name, e.g.:

```cpp
CHECK(Check(set, tiMother, line("good")) == "");                     // "Chị ơi, tui nhớ ra rồi! Mạ tui tên ..."
CHECK(Check(set, tiMother, line("english-mixed")) == "english");     // "Chào chị, nice to meet you."
CHECK(Check(set, tiMother, line("no-diacritics")) == "no-marks");    // "Chi oi, tui nho ra roi, ma tui ten ..."
CHECK(Check(set, tiMother, line("sand-too-early")) == "added-memory"); // contains "Cát trắng"
CHECK(Check(set, tiMother, line("upper-insult")) == "never");        // "ĐỒ NGỐC" vs never "đồ ngốc"
CHECK(Check(set, tiMother, line("southern-ti")) == "dialect");       // Tí says "nghen"
CHECK(Check(set, tiMother, line("decomposed")) == "bad-text");       // u + U+031B
CHECK(Check(set, tiMother, line("bad-token")) == "token");           // "{ten}"
CHECK(Check(set, tiMother, line("no-mother")) == "missing-fact");
CHECK(Lower(line("upper-insult")) == line("upper-insult-lower"));
CHECK(Tidy(line("quoted")) == line("good"));
// every situation's own authored line passes its own checks:
for (const Situation &s : set.situations) CHECK(Check(set, s, s.authored) == "");
```

- [ ] **Step 2:** Run `ctest -R Umbra.TalkMind` — FAIL (functions missing).
- [ ] **Step 3: Implement** `Lower`, `Words`, `Tidy`, `Check` in `TalkCheck.cpp`; write the checks' list in `TalkMind.md`.
- [ ] **Step 4:** Run — PASS. If an authored line fails its own checks, fix the data (or the rule, if the rule is wrong), not the test.
- [ ] **Step 5: Commit** `issue #UMBRA-326 Campaign model test: the Vietnamese line checks (diacritics, no English, never-say, dialect, tokens, no added memory)`.

---

### Task 4: The request, the run and the report (#327)

**Files:** Create `modules/talkmind/src/TalkRun.cpp`; modify `TalkMind.hpp`, `TalkMind.md` ("The report"), `tests/TalkMindTests.cpp`.

**Interfaces:**
- Consumes: `umbra::town::Mind`, `MindRequest` (`Mind.hpp`); Task 2 and 3 names.
- Produces:

```cpp
[[nodiscard]] umbra::town::MindRequest Request(const TestSet &set, const Situation &situation, std::uint32_t seed);
struct LineResult { std::string situation, speaker, authored, line, objection; std::uint32_t seed = 0; long long ms = 0; };
struct ModelRun
{
    std::string model, loadError;                 // loadError non-empty: the pinned llama.cpp could not load it
    long long loadMs = 0, warmMs = 0;
    int videoMb = -1;                             // -1: not measured (no graphics card in this build)
    std::vector<LineResult> lines;
    [[nodiscard]] double PassShare() const;
    [[nodiscard]] long long AverageMs() const;
    [[nodiscard]] long long SlowestMs() const;
};
// Every situation x rules.seeds seeds (seed = base + i), timed, tidied and checked.
void RunTest(const TestSet &set, umbra::town::Mind &mind, std::uint32_t base, ModelRun &run);
// The verdict line: "passes the automatic checks" or each failed rule (share, speed, memory, load).
[[nodiscard]] std::string Verdict(const TestSet &set, const ModelRun &run);
void WriteReport(const TestSet &set, const ModelRun &run, std::ostream &markdown); // for the user to read
[[nodiscard]] nlohmann::json Summary(const TestSet &set, const ModelRun &run);   // "umbra.lc-model-run" v1
void WriteComparison(const std::vector<nlohmann::json> &summaries, std::ostream &markdown);
```

`Request`: `prompt.txt` with its placeholders filled (`{facts}` one per line; the speaker's voice sheet as `{sheet}`, `{name}`), `grammar` = `root ::= [^\n\r]+` (one line), `maxTokens` and `seed` from rules and the argument. The markdown report: a header (model, load and warm-up time, video memory, lines, share passing, average and slowest time, the verdict, and a reminder that **the user's judgement is half of the pass**), then per speaker a table `situation | seed | authored | model | check | ms`, then the count of each objection. The comparison: one row per summary (model, loads, share, average ms, slowest ms, video MB, verdict).

- [ ] **Step 1: Write the failing tests** with two stand-in minds:

```cpp
struct Authored final : umbra::town::Mind  // answers each request with its own authored line, quoted
{ std::string Name() const override { return "authored"; }
  std::string Complete(const umbra::town::MindRequest &r) override; /* finds "{authored}" text after the prompt's marker */ };
struct English final : umbra::town::Mind { /* always "Hello, how are you?" */ };

TEST_CASE("The request carries the voice, the facts, the authored line, one-line grammar and the seed", "[talk]")
{   // prompt contains Ti's name, his sheet, the mother fragment text, the authored line; no "{" placeholder left
    // except {player} and {anh/chị}; grammar == "root ::= [^\\n\\r]+"; seed == 42; maxTokens == rules.maxTokens
}
TEST_CASE("A run checks every situation for every seed and gives a verdict", "[talk]")
{   ModelRun good; RunTest(set, authoredMind, 1, good);
    CHECK(good.lines.size() == 60);                   // 20 x 3 seeds
    CHECK(good.PassShare() == Catch::Approx(1.0));
    CHECK(Verdict(set, good) == "passes the automatic checks");
    ModelRun bad; RunTest(set, englishMind, 1, bad);
    CHECK(bad.PassShare() == Catch::Approx(0.0));
    CHECK(Verdict(set, bad).find("90%") != std::string::npos);
    bad.videoMb = 3500;  CHECK(Verdict(set, bad).find("3072") != std::string::npos);
    ModelRun none; none.loadError = "unknown model architecture: 'gemma3n'";
    CHECK(Verdict(set, none).find("cannot load") != std::string::npos);
}
TEST_CASE("The report and the comparison name every model and every line", "[talk]")
{   // WriteReport: header has the model name and "90%", one table row per line, the objection counts;
    // Summary(...)["format"] == "umbra.lc-model-run"; WriteComparison of two summaries: two rows.
}
```

- [ ] **Step 2:** Run `ctest -R Umbra.TalkMind` — FAIL.
- [ ] **Step 3: Implement** in `TalkRun.cpp` (timing with `std::chrono::steady_clock` around `Complete`; then `Tidy`, then `Check`).
- [ ] **Step 4:** Run — PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-327 Campaign model test: requests, the timed run, the verdict, the report and the side-by-side comparison`.

---

### Task 5: `UmbraTool talk-mind` and the video memory measure (#328)

**Files:** Create `plugins/last-curtain/tool/TalkMindCommand.cpp`; modify `plugins/living-town/modules/mind/include/LlamaMind.hpp`, `src/LlamaMind.cpp`, `Mind.md` (one line); the tool's link list if `Umbra::TalkMind` must be added there (follow how `NightMindCommand.cpp` gets `Umbra::NightMind`).

**Interfaces:**
- Produces: `[[nodiscard]] long long umbra::town::LlamaGpuFreeBytes();` — free memory summed over llama.cpp's GPU devices (`ggml_backend_dev_count` / `ggml_backend_dev_type == GGML_BACKEND_DEVICE_TYPE_GPU` / `ggml_backend_dev_memory`), `-1` when none.
- The command (usage string exactly):

```
talk-mind --model <gguf> [--data <campaign folder>] [--out <folder>] [--seed 1] [--threads n] [--gpu layers] [--show-prompt 1]
talk-mind --compare <summary.json>... [--out <file.md>]
```

Steps of the command: 1. `LoadTestSet` (default data `UMBRA_LASTCURTAIN_CAMPAIGN_DATA`, the plugin's `assets/campaign`), report diagnostics and stop on failure; 2. free GPU bytes before; load with `LoadLlamaMind`, timed; on null, set `run.loadError` to `"cannot load <file> with this build's llama.cpp: <error>"`, still write the report and summary, exit 1; 3. warm up, timed; free bytes after → `videoMb` (−1 if either is −1); 4. `RunTest`; 5. write `<out>/<model file stem>.md` and `<out>/<model file stem>.json` (default out `talk-mind-reports/`), print the verdict; exit 0 if the run passes the automatic checks, else 2. `--compare` reads each summary and writes the comparison to `--out` or stdout. Without llama.cpp in the build, `--model` prints the same "built without llama.cpp" message as `night-mind`; `--compare` works in every build.

- [ ] **Step 1: Write the failing check** (a CTest that runs the tool, added beside the other UmbraTool tests if the repo has them; otherwise a manual check recorded in Task 7): `UmbraTool talk-mind --compare a.json b.json` on two summaries written by the Task 4 test prints two rows; and, in a llama build, `UmbraTool talk-mind --model README.md` exits 1 with `cannot load` and writes `talk-mind-reports/README.md` holding that verdict.
- [ ] **Step 2:** Build `UmbraTool`; run the check — FAIL (unknown command).
- [ ] **Step 3: Implement** `LlamaGpuFreeBytes` and `TalkMindCommand.cpp` (model it on `NightMindCommand.cpp`; register with `UMBRA_TOOL_COMMAND(TalkMind, "talk-mind", "<usage above>", TalkMindCommand)`).
- [ ] **Step 4:** Run the check — PASS. Build everything and run all CTests in the cloud (about 25 min) — all pass, 59 tests (58 + `Umbra.TalkMind`); note pre-existing failures separately if any.
- [ ] **Step 5: Commit** `issue #UMBRA-328 UmbraTool talk-mind: runs a model over the Vietnamese test set, measures video memory and speed, writes one report per model and compares them`.

---

### Task 6: The voice test script (#329)

**Files:** Create `tools/voice/vieneu_lines.py`, `tools/voice/test_vieneu_lines.py`, `tools/voice/README.md`.

**Interfaces:**
- `python tools/voice/vieneu_lines.py [--url http://127.0.0.1:7860] [--campaign plugins/last-curtain/assets/campaign] [--out voice-test] [--player "Lan"] [--gender female|male] [--ti-pitch 3,4,5] [--ti-formant 1.08] [--only ti] [--show-api]`
- Functions (tested): `fill_tokens(text: str, player: str, gender: str) -> str` (`{player}`, `{anh/chị}` → `anh`/`chị`, `{Anh/Chị}` capitalised); `jobs(campaign: Path) -> list[Job]` (one per situation's authored line × the speaker's `tts` voices; Tí's jobs also carry each `--ti-pitch` value); `shift(samples: np.ndarray, rate: int, semitones: float, formant: float) -> np.ndarray` (WORLD via `pyworld`: `harvest` → `cheaptrick` → `d4c`; F0 × 2^(semitones/12); spectral envelope warped along frequency by `formant` (1.0 keeps the formants; a little above 1 sounds younger without the chipmunk sound); `synthesize`); `file_name(job) -> str` (`<situation>__<voice>[__+4st].wav`, ASCII only: Vietnamese letters stripped of marks).

The run: one `gradio_client.Client(url)` (one queue session), `predict(api_name="/load_model")` once, then per job `predict(text, voice, api_name="/wrapper")`, copy the returned audio to `out/`, pitch-shift Tí's copies, write `out/index.md` (file, speaker, voice, pitch, the line). `--show-api` prints `client.view_api()` and stops: the first thing to run if VieNeu's parameter names differ from these. Nothing is sent anywhere but `--url`; the script refuses a URL whose host is not `127.0.0.1` or `localhost`.

- [ ] **Step 1: Write the failing tests** (`python -m unittest tools/voice/test_vieneu_lines.py`): `fill_tokens("Chào {anh/chị} {player}!", "Lan", "female") == "Chào chị Lan!"`; `jobs()` on the repo data gives 20 lines × voices, with Tí's carrying pitches 3, 4, 5; `shift` of a 1 s 150 Hz synthetic voiced tone at +4 st measures F0 within 3% of 150 × 2^(4/12) (measure with `pyworld.harvest`); `file_name` is ASCII; a non-local `--url` exits with an error before any network call; the Gradio client is replaced by a fake that records calls: `load_model` once, then `wrapper` once per job.
- [ ] **Step 2:** Run — FAIL. (In the cloud: `pip install --break-system-packages gradio_client numpy soundfile pyworld`.)
- [ ] **Step 3: Implement** the script; `README.md`: install line, the user's run, what to listen for (Tí young, not sped up; each speaker's region), and that results are the user's judgement.
- [ ] **Step 4:** Run — PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-329 Voice test: a script that voices the test set's authored lines through the user's local VieNeu, Ti pitched up with formant correction`.

---

### Task 7: Records and the user's checks (#330)

**Files:** Create `docs/testing/s44-vietnamese-model.md` (ROADMAP §14 template); modify `docs/ROADMAP.md` (dashboard: campaign plan 1 implemented/unverified), `docs/current-engine-status.md`, `CLAUDE.md` (first-use paragraph: where the test set, `talk-mind` and the voice script live; next: plan 2 after the user's verdict); copy this plan to `docs/design/plans/` if not already there.

- [ ] **Step 1:** Write the record: revision, commands, cloud results (tests, the capture), and **§4 the user's checks, not run**: (a) read and correct the Vietnamese in `voices/` and `situations.json`; (b) build with `town-model-gpu` and run `UmbraTool talk-mind --model <Arcee-VyLinh Q4_K_M .gguf>`, paste the report; (c) the same for Qwen3 4B and any candidate, then `talk-mind --compare talk-mind-reports/*.json`; (d) note each candidate's licence (VyLinh and Sailor: Qwen research licence, not shippable as is; SEA-LION: Gemma terms); (e) run `vieneu_lines.py --show-api`, then the full run, listen, pick Lệ Thanh's voice and Tí's pitch; (f) look at Vietnamese text in the Last Curtain screens on Windows. **The user picks the model or the hand-written fallback**; plan 2 can start in parallel since it doesn't depend on the model.
- [ ] **Step 2:** Ship the commits to the PC as `CLAUDE.md` §10 describes (tarballs into `Temp\lcship\` with new names, `apply.sh`); the font deletions need `git rm` on the PC, which needs the user's delete permission for those five files — ask, or leave them to the user. Compare `git rev-parse HEAD^{tree}` both sides.
- [ ] **Step 3: Commit** `issue #UMBRA-330 Campaign plan 1 recorded: the Vietnamese model test, voice test and fonts; the user's runs are open (s44 §4)`.

---

## Self-review notes

- Spec coverage: model test tool and test set (Tasks 2–5), Arcee-VyLinh first and side by side (Task 5 `--compare`, Task 7 §4), load/memory/speed/licence criteria (Task 4 `Verdict`, Task 7 licence notes — licence is the user's reading, not automated), automatic checks incl. diacritics, English, never-say, length, dialect (Task 3), the 90% pass share (rules.json), report for the user (Task 4), voice script with Tí's pitch shift (Task 6), fonts pinned and in the notices (Task 1). The fallback "every line hand-written" needs no code now; it is the user's decision after §4.
- Left open for the user: Lệ Thanh's dialect markers and voice, all Vietnamese wording, the winner.
