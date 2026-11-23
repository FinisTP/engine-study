#pragma once

#include <Diagnostic.hpp>

#include <nlohmann/json.hpp>
#include <Match.hpp>
#include <MatchDef.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <filesystem>
#include <string>
#include <vector>

namespace umbra::campaign
{
    // How the engine tints a character (plan 7).
    enum class Form : std::uint8_t
    {
        Player,
        Ally,
        Strung,
        Ghost,
        Host,
        Zodiac
    };

    // One row of cast.csv.
    struct CastMember
    {
        std::string id, name, sprite, bound; // bound: the sprite while strung ("" = none)
        Form form = Form::Ghost;
        std::string voice;
        std::string role = "allrounder";     // the class line (growth.csv)
        int hpPlus = 0, attackPlus = 0, movePlus = 0; // this character's change to its line
        std::vector<std::string> kit;        // its abilities; empty: the class's up to its level
    };

    struct Cast
    {
        std::vector<CastMember> members;
        const CastMember *Find(const std::string &id) const;
    };

    bool LoadCast(const std::filesystem::path &file, Cast &cast, std::vector<core::Diagnostic> &d);

    // growth.csv: each class line's numbers at each level (Campaign.md "Units").
    struct Growth
    {
        std::map<std::pair<std::string, int>, std::array<int, 5>> lines; // (role, level) -> hp, attack, defence, speed, move
    };

    bool LoadGrowth(const std::filesystem::path &file, Growth &growth, std::vector<core::Diagnostic> &d);

    // A fighting unit for a cast member at a level: its class line, its own changes, and for an
    // enemy the form's scaling (zodiac attack and HP x1.33, ordinary ghost x0.6).
    match::Unit MakeUnit(const match::MatchDef &def, const Cast &cast, const Growth &growth, const std::string &id, int level, int faction, bool enemy);

    // ---- Stages (Campaign.md "Stages") ----

    enum class Behaviour : std::uint8_t
    {
        None,          // the bots play it from the start
        Aggressive,    // holds until round `after`, then plays
        Passive,       // acts only on a party unit in its room within 4 squares, or once attacked
        Reinforcement  // appears at round `round` on its square
    };

    struct StageUnit
    {
        std::string who;
        int x = 0, y = 0, side = 1, level = 1;
        bool boss = false, recruit = false, strung = false;
        float height = 1.0F;
        Behaviour behaviour = Behaviour::None;
        int after = 0, round = 0;
        std::vector<std::pair<std::string, int>> drops; // item id, percent
    };

    struct StageItem
    {
        std::string id, item; // id: take-once, kept in the save; item: an items.csv id
        int x = 0, y = 0;
    };

    struct Twist
    {
        std::string name;
        std::vector<StageUnit> units;
        nlohmann::json gimmicks;
    };

    struct StageDef
    {
        std::string id, kind = "story", title, titleEn, backdrop, music;
        int level = 1, partyCap = 1, xp = 0, width = 0, height = 0;
        std::vector<std::string> map; // '#' wall, '.' floor, '~' hazard, 'W' window, 'R' rope post, 'S' party start, 'D' doorway
        std::vector<StageUnit> units;
        std::vector<StageItem> items;
        bool mustFree = false;
        std::vector<int> noRevive; // sides that never revive
        nlohmann::json gimmicks = nlohmann::json::object();
        std::string aimStart, aimRecap;
        std::map<std::string, std::string> scenes; // moment -> scene id
        std::vector<Twist> twists;                 // skirmishes: one per refill
        char At(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height ? map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] : '#'; }
    };

    // Reads and checks a stage; scene ids must be files in `scenes`; item ids in the match's items.csv.
    bool LoadStage(const std::filesystem::path &file, const Cast &cast, const match::MatchDef &def, const std::filesystem::path &scenes,
                   StageDef &stage, std::vector<core::Diagnostic> &d);

    // A party member as the save keeps it (plan 7): who, level, the 4 item slots (items.csv ids, "" empty).
    struct PartyMember
    {
        std::string id;
        int level = 1;
        std::array<std::string, 4> items{};
    };

    // A fight ready to play, and for each unit its cast id and drawn height.
    struct BuiltStage
    {
        match::MatchState state;
        std::vector<std::string> who;
        std::vector<float> height;
    };

    // The stage's fight: its map as the board, the party (up to its cap) on the starts, the player
    // first, enemies (with the refill's twist), items not yet taken, reinforcements waiting.
    BuiltStage BuildStage(const match::MatchDef &def, const Cast &cast, const Growth &growth, const StageDef &stage,
                          const std::vector<PartyMember> &party, int refill, const std::set<std::string> &taken, std::uint64_t seed);

    enum class Side : std::uint8_t
    {
        Left,
        Right
    };

    // A portrait brought in (or moved) before a line.
    struct Stage
    {
        std::string who;
        Side side = Side::Left;
        int slot = 0; // 0 nearest the middle, 1 behind
        std::string face;
    };

    struct Choice
    {
        std::string text, fact, value, jump;
    };

    struct Line
    {
        std::string id, speaker, face, text, sound, jump; // speaker "" = narration; jump: the next line's id
        std::vector<Stage> enter;
        std::vector<std::string> leave;
        std::vector<Choice> choices; // 0 or 2-4
        bool model = false;          // plan 6: the model may reword it
    };

    struct Scene
    {
        std::string id;
        std::vector<Line> lines;
    };

    // The faces every portrait may have (face_<name>.png).
    bool KnownFace(const std::string &face);

    // Reads and checks a scene; each problem names the file, the line's field and the fix.
    bool LoadScene(const std::filesystem::path &file, const Cast &cast, Scene &scene, std::vector<core::Diagnostic> &d);

    // The player as the save knows them: the typed name and "male" or "female".
    struct Speaker
    {
        std::string name, gender;
    };

    using Facts = std::map<std::string, std::string>;

    struct Portrait
    {
        std::string who, face;
        bool lit = false;
    };

    // {player}, {anh/chi}, {Anh/Chi}; an empty name becomes the cast's "nameless" name.
    std::string FillTokens(const std::string &text, const Speaker &player, const Cast &cast);

    // Plays a scene with no drawing: the line so far, who stands where, the choices (Campaign.md).
    class ScenePlayer final
    {
    public:
        ScenePlayer(const Scene &scene, const Cast &cast, Speaker player, Facts &facts);
        void Update(float seconds, float lettersPerSecond); // the typewriter
        void Advance();                                     // complete the line, else the next one (not past a choice)
        void Skip();                                        // to the end, stopping at the first choice
        bool Choose(int index);                             // sets the fact and jumps; false with no choice shown
        [[nodiscard]] bool Done() const { return at_ >= scene_.lines.size(); }
        [[nodiscard]] bool Complete() const { return Done() || shown_ >= letters_; } // the whole line is shown
        [[nodiscard]] std::string Shown() const;
        [[nodiscard]] const std::string &SpeakerName() const { return name_; }
        [[nodiscard]] Side SpeakerSide() const;
        [[nodiscard]] std::array<Portrait, 2> Left() const { return Row(Side::Left); }
        [[nodiscard]] std::array<Portrait, 2> Right() const { return Row(Side::Right); }
        [[nodiscard]] std::vector<std::string> Choices() const;
        [[nodiscard]] std::vector<std::pair<std::string, std::string>> Log() const { return log_; }
        std::string TakeSound(); // the line's sound, once, as it starts

    private:
        void Enter(std::size_t index);
        void Next(const std::string &jump);
        std::array<Portrait, 2> Row(Side side) const;

        const Scene &scene_;
        const Cast &cast_;
        Speaker player_;
        Facts &facts_;
        std::map<std::string, Stage> stage_;
        std::size_t at_ = 0, letters_ = 0, shown_ = 0;
        float progress_ = 0.0F;
        std::string text_, name_, sound_;
        std::vector<std::pair<std::string, std::string>> log_;
    };
}
