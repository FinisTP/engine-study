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

    // A tutorial's guided step (Campaign.md "Guided steps"): whose turn, where to walk (a square [x, y],
    // "with:<cast id>", "toward" the target, "safe" off the marked squares), what to do, at whom, its tip.
    struct GuideDef
    {
        std::string unit, move, act, target, pick, when, tip;
        int round = 0, x = -1, y = -1;
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
        std::map<std::string, int> startHp; // the party starts hurt: HP by cast id (a tutorial)
        std::vector<GuideDef> guide;        // a tutorial's guided steps, in order
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

    struct UnitBehaviour
    {
        Behaviour kind = Behaviour::None;
        int after = 0;
    };

    // A fight ready to play: per unit its cast id, drawn height and behaviour; per square its room
    // (open squares joined without passing a doorway 'D'; -1 for walls and doorways).
    struct BuiltStage
    {
        match::MatchState state;
        std::vector<std::string> who;
        std::vector<float> height;
        std::vector<UnitBehaviour> behaviour;
        std::vector<int> rooms;
        int sight = 4;              // how far a passive unit sees in its room ("dark" lowers it)
        std::string spawnWho;       // what a window lets out (its cast id)
    };

    // Which of a stage's scenes have played this attempt, and how much of the log was read.
    struct Moments
    {
        std::set<std::string> done;
        std::size_t read = 0;
    };

    // The scenes whose moments the fight has reached in its first `told` log lines, each once per attempt:
    // start, round:<n>, then firstAttacked/falls/freed:<cast id> in log order, then end (won) or lost. Reads only.
    std::vector<std::string> StageMoments(const StageDef &def, const BuiltStage &stage, std::size_t told, Moments &m);

    // After play adds units (a window's spawn), gives them their name, height and a plain behaviour.
    void Sync(BuiltStage &stage);

    // The stage's fight: its map as the board, the party (up to its cap) on the starts, the player
    // first, enemies (with the refill's twist), items not yet taken, reinforcements waiting.
    BuiltStage BuildStage(const match::MatchDef &def, const Cast &cast, const Growth &growth, const StageDef &stage,
                          const std::vector<PartyMember> &party, int refill, const std::set<std::string> &taken, std::uint64_t seed);

    // The enemy's next action: its behaviour first (hold, or wake), then the faction bots (Campaign.md "The enemy brain").
    match::Action EnemyAction(const match::MatchDef &def, const BuiltStage &stage, const match::MatchState &s, match::BotLevel level);
    // The party played by a bot (tests, the campaign soak): while a recruit is strung it is the target, never
    // the boss; the player walks to it once fallen and cuts. Otherwise the faction bots.
    match::Action PartyAction(const match::MatchDef &def, const BuiltStage &stage, const match::MatchState &s, match::BotLevel level);

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
        std::string en; // the English (Localization.md); text is the Vietnamese
    };

    struct Line
    {
        std::string id, speaker, face, text, sound, jump; // speaker "" = narration; jump: the next line's id
        std::vector<Stage> enter;
        std::vector<std::string> leave;
        std::vector<Choice> choices; // 0 or 2-4
        bool model = false;          // plan 6: the model may reword it
        std::string en;              // the English of text (Localization.md)
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
    void UseEnglish(Scene &scene); // every line and choice shows its English (where it has one)

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
