# Campaign plan 3: dialog scenes — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A generic RPG conversation for the café, before and after a fight and inside one: scene files, a headless scene player, the dialog screen with portraits, and the game's first scene (the prologue).

**Architecture:** A new headless module `campaign` (`plugins/last-curtain/modules/campaign`, `umbra::campaign`) starts with the cast sheet and scenes; plans 4–6 add stages, the save and the café to it. `ScenePlayer` is a value with no drawing: line, who stands where, choices, facts. The app's `DialogView` turns it into `ui/dialog.ui.json` data each frame. The engine's `image` widget gains `flip` and `tint` for facing portraits and dimming the listener.

**Tech Stack:** C++20, nlohmann/json, Catch2, the engine UI (`engine/ui`), SwiftShader captures.

**Spec:** `docs/design/2026-11-23-campaign-slice-1.md` §7.11 (confirmed as written by the user), handoff §4.

## Global Constraints

- Code style (CLAUDE.md); C++ string literals ASCII; Vietnamese only in UTF-8 data. Screen words in data.
- Scenes: **0–2 portraits left, 0–2 right**; a line brings one in, takes one out or swaps; portraits **face the middle** (right side mirrored); the **name tag** on the speaker's side; the speaker lit, others dimmed; a line with no speaker is **narration**.
- Faces: calm, talk, happy, angry, afraid, scheming, near-dead (`face_<name>.png` in `art/sprites/<sprite>/`), set per line; portraits drawn in the character's **form** (strung specter red, ally blue, ghost pale, player solid, Lệ Thanh faint warm). The player's portrait and name come from the save (plan 5; here passed in).
- Text appears **letter by letter**; a click or Space completes the line, the next goes on. **Bỏ qua** skips to the end but **stops at a choice**; **Nhật ký** shows the lines so far. Auto-advance later.
- **Choices**: 2–4 at the end of a line; a choice sets a saved fact and may jump within the scene.
- Lines are authored; a café line may later be worded by the model (plan 6), the authored line as fallback. A line may name a sound file (plays if present).
- Data: `assets/campaign/scenes/<id>.json`, versioned, validated: unknown character, a third portrait on a side, a jump to a missing line → file, line, fix.
- Tokens `{player}`, `{anh/chị}`, `{Anh/Chị}` filled from the player's name and gender.
- Commits `issue #UMBRA-<n>` from **#341** (plan 2 ends at #340).

## Review Focus

1. **A choice at the very last line** followed by Skip: Skip must stop at the choice, not end the scene. Test in Task 3.
2. **A jump loop** (line A jumps to B, B to A with no choice): the validator refuses a cycle without a choice in it. Test in Task 2.
3. **Long Vietnamese lines** in the box: wrapped by the box width, never cut mid-letter (the typewriter counts code points, not bytes). Test in Task 3 (a line with "ử" shown 3 letters at a time never splits a letter).
4. **A face the sprite lacks** (the Rat has only `face_calm`): falls back to `face_calm`, then to no portrait, never a missing-file error. Test in Task 4.
5. **A scene started with no player name** (developer run): `{player}` becomes the cast sheet's default name. Test in Task 3.

---

### Task 1: The image widget mirrors and tints (#341)

**Files:** `engine/ui/include/UI.md`, `engine/ui/src/Ui.cpp` (image drawing and parsing), `tests/ui/UiTests.cpp`.

**Interfaces:** `image` gains `flip` (a binding or `true`: mirror left–right) and `tint` (a binding or `"#rrggbbaa"`, multiplied with the picture; default white). Bad tint text → a diagnostic naming the field.

- [ ] Failing test `"An image can be mirrored and tinted"`: a screen with two images of a 2×1 test picture (left red, right blue): with `flip` the first vertex's u is 1; with `tint: "#80808080"` the vertex colour is half; a bad tint `"#zz"` → `ParseScreen` false, field `root.tint`.
- [ ] Implement (swap the u coordinates of the image quad; vertex colour from the tint); `UI.md` table row.
- [ ] `Umbra.Ui` passes; commit `issue #UMBRA-341 Engine UI: images can be mirrored and tinted`.

### Task 2: The cast sheet and scene files (#342)

**Files:** create `modules/campaign/` (`CMakeLists.txt`, `include/Campaign.hpp`, `include/Campaign.md`, `src/Cast.cpp`, `src/Scene.cpp`), `assets/campaign/cast.csv`, `plugin.json`, `tests/CampaignTests.cpp` (new executable `UmbraCampaignTests`, CTest `Umbra.Campaign`).

**Interfaces:**

```cpp
namespace umbra::campaign
{
    enum class Form : std::uint8_t { Player, Ally, Strung, Ghost, Host, Zodiac }; // how the engine tints it (plan 7)
    struct CastMember { std::string id, name, sprite, bound; Form form = Form::Ghost; std::string voice; };
    struct Cast { std::vector<CastMember> members; const CastMember *Find(const std::string &id) const; };
    bool LoadCast(const std::filesystem::path &file, Cast &cast, std::vector<core::Diagnostic> &d);

    enum class Side : std::uint8_t { Left, Right };
    struct Stage { std::string who; Side side = Side::Left; int slot = 0; std::string face; }; // a portrait brought in
    struct Choice { std::string text, fact, value, jump; };
    struct Line
    {
        std::string id, speaker, face, text, sound, jump; // speaker "" = narration; jump: the next line's id
        std::vector<Stage> enter;                          // brought in (or moved/swapped) before the line
        std::vector<std::string> leave;                    // taken out before the line
        std::vector<Choice> choices;                       // 0 or 2-4
        bool model = false;                                // plan 6: the model may reword it
    };
    struct Scene { std::string id; std::vector<Line> lines; };
    bool LoadScene(const std::filesystem::path &file, const Cast &cast, Scene &scene, std::vector<core::Diagnostic> &d);
}
```

`cast.csv`: `id,name,sprite,bound,form,voice` — rows `player` (name `{player}`, sprite chosen by gender at run time: `player_female`/`player_male`), `lethanh` (Lệ Thanh, host), `ti` (Bé Tí, sprite `ti`, bound `ti_bound`, form strung until freed), `bao` (Bác sĩ Bảo, `cafe_bao`, ghost), `mai` (Bà Mai, `cafe_mai`, ghost), `rat` (Con Chuột, `zodiac_rat`, zodiac). Plan 4 adds the stats columns.

Scene JSON: `{"format": "umbra.lc-scene", "version": 1, "id": "...", "lines": [{ "id": "a", "speaker": "ti", "face": "happy", "text": "...", "enter": [{"who": "ti", "side": "left", "slot": 0, "face": "calm"}], "leave": [], "choices": [{"text": "...", "fact": "ti.promise", "value": "kept", "jump": "c"}], "jump": "", "sound": "" }]}`. Lines run in order unless `jump` or a choice's jump says otherwise.

Validation (each a diagnostic with file, `lines[i].field`, and the fix): unknown speaker or `who`; a face not in the seven; a side with more than 2 portraits after the line's `enter`/`leave`; a speaker not on stage (narration excepted); 1 or more than 4 choices; a jump to a missing id; duplicate ids; a cycle of jumps with no choice in it.

- [ ] Failing tests: `"The cast sheet loads; a bad form is refused"`, `"A scene loads"` (the prologue from Task 5 once it exists; until then a test scene in `tests/data/scenes/`), `"A broken scene names the file, the line and the fix"` (one case per rule above).
- [ ] Implement; `Campaign.md` "Scenes".
- [ ] `Umbra.Campaign` passes; commit `issue #UMBRA-342 Campaign: the cast sheet and scene files, validated`.

### Task 3: The headless scene player (#343)

**Files:** `include/Campaign.hpp`, `src/ScenePlayer.cpp`, `tests/CampaignTests.cpp`.

**Interfaces:**

```cpp
struct Speaker { std::string name, gender; }; // the player's typed name and "male"/"female" (plan 5's save)
using Facts = std::map<std::string, std::string>;
struct Portrait { std::string who, face; bool lit = false; };
class ScenePlayer
{
public:
    ScenePlayer(const Scene &scene, const Cast &cast, Speaker player, Facts &facts);
    void Update(float seconds, float lettersPerSecond); // the typewriter
    void Advance();                // click/Space: complete the line, else the next one (not past a choice)
    void Skip();                   // to the end, stopping at the first choice met
    bool Choose(int index);        // sets the fact, jumps; false if no choice is shown
    [[nodiscard]] bool Done() const;
    [[nodiscard]] std::string Shown() const;           // the line's text so far (whole letters)
    [[nodiscard]] const std::string &SpeakerName() const; // "" for narration
    [[nodiscard]] Side SpeakerSide() const;
    [[nodiscard]] std::array<Portrait, 2> Left() const, Right() const;
    [[nodiscard]] std::vector<std::string> Choices() const; // shown only when the line is complete
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> Log() const; // (name, text) so far
    [[nodiscard]] std::string Sound() const; // the line's sound, once, as it starts
};
std::string FillTokens(const std::string &text, const Speaker &player); // {player}, {anh/chị}, {Anh/Chị}
```

- [ ] Failing tests: the typewriter counts letters (Review Focus 3); `Advance` completes then moves on; enter/leave/swap give the right `Left`/`Right` with the speaker lit; narration has no name and lights nobody; `Skip` stops at a choice (Review Focus 1); `Choose` sets the fact and jumps; `Log` holds the lines shown; tokens filled for both genders and the default name (Review Focus 5).
- [ ] Implement; `Campaign.md` "Playing a scene".
- [ ] Pass; commit `issue #UMBRA-343 Campaign: the scene player (typewriter, portraits, choices, skip, log)`.

### Task 4: The dialog screen (#344)

**Files:** `apps/LastCurtain/assets/ui/dialog.ui.json`, `dialog-log.ui.json`, `apps/LastCurtain/DialogView.{hpp,cpp}`, `apps/LastCurtain/Main.cpp` (`--scene <id> [--backdrop <name>]` developer run), `apps/LastCurtain/assets/campaign-words.json` (Vietnamese: "Nhật ký", "Bỏ qua", the log's title), `apps/LastCurtain/README.md`, `tests/CampaignViewTests.cpp` (in `UmbraNightTests` with the app's play library).

**Interfaces:** `nlohmann::json DialogData(const ScenePlayer &p, const Cast &cast, const Speaker &player, const std::function<bool(const std::string &)> &exists)` — per slot: `image` (the face file, falling back to `face_calm`, else `""` hidden; Review Focus 4), `flip` (right side), `tint` (lit `#ffffffff`, dimmed `#8a8a8aff`, then the form's colour multiplied: strung `#ff9a9aff`, ghost `#d8e4ffcc`, ally `#a8c8ffff`, host `#ffe8d0ff`), the name tag's side, the text, choices, buttons. Layout as the spec's sketch: 2+2 portraits over the box, name tag on the box's edge on the speaker's side, ▼ when complete, buttons bottom-right. Click or Space advance; 1–4 choose; Esc = Bỏ qua.

- [ ] Failing tests: `DialogData` on the test scene (slots, flips, tints, fallback face, name side); `UmbraTool ui-preview dialog` with a sample data file (no diagnostics).
- [ ] Implement; capture `LastCurtain --scene prologue --backdrop foyer_today --frames 120 --capture` under SwiftShader, with the analog screen on: portraits face the middle, the listener dimmed, Vietnamese wraps.
- [ ] Commit `issue #UMBRA-344 The Last Curtain: the dialog screen with portraits, name tag, choices, skip and log`.

### Task 5: The prologue (#345)

**Files:** `assets/campaign/scenes/prologue.json` (Vietnamese, drafted for the user's review), `Campaign.md`.

The game's first minutes: the hook. A night tour of the restored opera house (today); the guide's torch dies; the player alone in the foyer; a woman in a red bridal áo dài on the stairs, too still, asks the player's name without being told it was needed (she already knows it: `{player}`); she says the house is "full tonight" though the seats are empty; rats scratch in the walls; she is gone; the first rat specter's strings creak (leads into T1). One choice that counts later: answer her kindly or warily (fact `lethanh.first`), which colours one line of hers in the café. ~14 lines, narration and Lệ Thanh, the player's portrait (face afraid/calm).

- [ ] Failing test: the prologue loads and plays to the end both ways through its choice.
- [ ] Write it; capture it; commit `issue #UMBRA-345 Campaign: the prologue scene (Vietnamese, drafted for review)`.

### Task 6: Records (#346)

`docs/testing/s47-dialog-scenes.md` (§4: read the prologue as a native speaker; on Windows, click and Space through it, Skip, the log, a choice). ROADMAP, status, CLAUDE.md. Ship to the PC.

## Left to later plans

Scenes inside fights and their triggers (plan 4, with stage files); facts saved (plan 5); café visits as scenes and the model's wording (plan 6); strings drawn on portraits (plan 7).
