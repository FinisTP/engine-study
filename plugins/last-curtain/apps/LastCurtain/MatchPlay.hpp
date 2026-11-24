#pragma once

#include <array>
#include <Match.hpp>
#include <MatchBots.hpp>
#include <Options.hpp>
#include <Sequences.hpp>

#include <glm/vec2.hpp>
#include <nlohmann/json.hpp>

#include <functional>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

// One faction match as the person plays it, without a window (tested; MatchPlay.md): the player's
// side waits for input, bots and monsters play one beat at a time, and each screen gets its JSON.
namespace umbra::cafe { struct Career; }

// The board cursor for keys and the gamepad (P4): one square, stepped by a direction, repeating while held.
struct BoardCursor
{
    int cell = -1;
    // dir: this frame's direction (-1..1 each); start: the acting unit's square when the cursor is off.
    // True when it stepped.
    bool Update(glm::ivec2 dir, float dt, int width, int height, int start);
    void Off() { cell = -1, held_ = 0.0F, dir_ = {0, 0}; }

private:
    float held_ = 0.0F; // how long this direction has been held
    glm::ivec2 dir_{0, 0};
};

class MatchPlay final
{
public:
    MatchPlay(const umbra::match::MatchDef &def, std::uint64_t seed, std::vector<int> picks, umbra::match::BotLevel level);
    MatchPlay(const umbra::match::MatchDef &def, umbra::match::MatchState saved, umbra::match::BotLevel level);

    void SetWords(nlohmann::json words) { words_ = std::move(words); }      // assets/match-words.json
    void SetOptions(const umbra::night::GameOptions &o) { options_ = o; } // the pace (assets/options.csv)
    void StartFrom(const umbra::cafe::Career &career); // the player's side as the career left it (CareerMatch)
    void SetWatch(bool watch) { watch_ = watch; }                          // both sides are bots (--watch)
    // Who plays the units that aren't the player's (a campaign stage's enemy brain); default the faction bots.
    void SetBrain(std::function<umbra::match::Action(const umbra::match::MatchState &)> brain) { brain_ = std::move(brain); }

    // A campaign scene over the fight (campaign plan 4): paused, nothing moves; Quiet is between told beats.
    void Pause(bool paused) { paused_ = paused; }
    [[nodiscard]] bool Quiet() const;
    [[nodiscard]] std::size_t Told() const { return shown_; } // log entries the person has seen

    // A campaign stage (campaign plan 5): each unit's shown name and sprite ("" keeps the match's), the
    // stage's title; HudData is the campaign HUD's goals, this round's strip, the hint and the last log line.
    void SetCast(std::vector<std::string> names, std::vector<std::string> sprites) { names_ = std::move(names), sprites_ = std::move(sprites); }
    void SetStage(std::string title) { stageTitle_ = std::move(title); }
    void SetHeights(std::vector<float> heights) { heights_ = std::move(heights); }
    [[nodiscard]] const std::vector<std::string> &Names() const { return names_; }
    [[nodiscard]] const std::vector<float> &Heights() const { return heights_; }
    [[nodiscard]] const std::vector<std::string> &Sprites() const { return sprites_; } // a stage's figure per unit ("" the class's)
    void SetKeyNames(std::string hurry, std::string skip) { hurryKey_ = std::move(hurry), skipKey_ = std::move(skip); }
    [[nodiscard]] nlohmann::json HudData() const;
    // The command menu (campaign plan 5, P2): {title, rows: [{id, label, note, key, off, why, sub}]} for the
    // player's turn: walking (stay, end), the main menu, a sub-menu (skills, items) or the armed targets.
    [[nodiscard]] nlohmann::json Menu() const;
    bool MenuKey(int n);                // number key n (1-based) picks row n when it is on
    bool MenuPick(const std::string &id);
    bool Back();                        // one level back: targets, sub-menu, the walk; at the top the panel hides and comes back
    void Look(int unit);                // a portrait or a click: that unit's card (the acting one: the panel back)
    // What an action would do (campaign plan 5): {min, max, counterMin, counterMax, hp, warn}; an attack
    // uses the best battle strike each way, a board ability the rules' Preview.
    [[nodiscard]] nlohmann::json PreviewFor(const umbra::match::Action &action) const;
    bool Pin(); // the card stays on the hovered unit (I); again unpins
    // What the board marks a round ahead (campaign plan 5): [{cell, kind}], kind hazard, breath, window, arrival.
    // The breath's row only once its drawing in has been told (told: log entries shown; note 15).
    static nlohmann::json Telegraphs(const umbra::match::MatchState &s, std::size_t told = static_cast<std::size_t>(-1));
    // Toasts (P12): a told find, freeing or level shows top right for 3 s, at most 3 at once.
    [[nodiscard]] std::string ToastText(const umbra::match::LogEntry &e) const; // "" when not a toast
    void Toast(std::string text);

    bool Do(const umbra::match::Action &action); // the player's action; false unless it is theirs and legal
    void Update(float dt);
    [[nodiscard]] bool PlayerTurn() const;
    [[nodiscard]] std::string Screen() const; // "", "banner", "battle", "level", "event" or "end"
    [[nodiscard]] nlohmann::json UiData() const;
    bool HandleUi(const std::string &screen, const std::string &action, const std::string &arg);
    void Hover(int cell) { hover_ = cell; }
    void Click(int cell); // a lit square walks there, oneself stays, an armed target is attacked
    [[nodiscard]] const umbra::match::MatchState &State() const { return s_; }
    [[nodiscard]] nlohmann::json Save() const;

    // The pick screen (match-pick.ui.json): every character as a card, Start only with a full side.
    static nlohmann::json PickData(const umbra::match::MatchDef &def, const std::vector<int> &picks, bool canContinue, umbra::match::BotLevel level);
    static bool TogglePick(const umbra::match::MatchDef &def, std::vector<int> &picks, int character); // false: unknown or the side is full
    // What a told line looks and sounds like (MatchPlay.md "Motions and sounds"): units and their
    // motions (motions.json), and a sounds.csv cue ("" none). Taken by the window once each.
    static std::vector<std::pair<int, std::string>> Moves(const umbra::match::LogEntry &e);
    static std::string Sound(const umbra::match::LogEntry &e);
    std::vector<std::pair<int, std::string>> TakeMoves() { return std::exchange(moves_, {}); }
    std::vector<std::string> TakeSounds() { return std::exchange(sounds_, {}); }
    // Sequences (Sequences.md): how told lines play out over time; none: the plain way.
    void SetSequences(const Sequences *sequences) { sequences_ = sequences; }
    struct Fx // an effect a sequence asks for: at a unit, flying from it to another, or a shake
    {
        std::string effect;
        int unit = -1, to = -1;
        float seconds = 0.0F, shake = 0.0F;
    };
    std::vector<Fx> TakeEffects() { return std::exchange(fx_, {}); }
    // A tutorial's guided steps (MatchPlay.md "Guided steps"): on its unit's turn a step allows one
    // square, one action, one target, one battle pick, with its tip; done, the next waits its turn.
    struct GuideStep
    {
        int unit = -1, round = 0;   // whose turn; 0: its first turn from now
        std::string when;           // "" or "breath": only while a breath row is marked
        int cell = -1, with = -1;   // the square to walk to, or the square of this unit
        bool toward = false, safe = false; // beside the target; or off every marked square
        std::string act;            // "", attack, ability:<id>, end
        int target = -1;            // a unit, or -1 any (toward: the nearest foe)
        std::string pick;           // in the battle: this ability only
        std::string tip;            // match words "guide.<tip>"
        bool done = false;
    };
    void SetGuide(std::vector<GuideStep> steps) { guide_ = std::move(steps); }
    std::vector<int> TakeSnaps() { return std::exchange(snaps_, {}); } // units put straight back (an undone walk)
    // The 2d6 total to reach whose odds are nearest an event's percent chance (13: never, 2: always).
    static int DiceTarget(int chance);
    static const char *LogName(umbra::match::LogKind kind);
    static const char *WhyName(umbra::match::Why why);
    [[nodiscard]] std::string Caption(const umbra::match::LogEntry &e) const;
    [[nodiscard]] std::string WhyText(umbra::match::Why why) const;

private:
    [[nodiscard]] bool Mine(int unit) const;   // a character of the player's side
    [[nodiscard]] bool BannerDue() const;      // a new round not yet announced
    [[nodiscard]] std::string Name(int unit) const;
    [[nodiscard]] std::string Sprite(int unit) const;
    [[nodiscard]] std::string Word(const std::string &group, const std::string &key) const;
    [[nodiscard]] int Key() const { return s_.round * 64 + s_.turn; } // one character's turn
    [[nodiscard]] bool Chose() const { return chosen_ == Key(); }      // walked or stayed: the actions show
    [[nodiscard]] bool Armed() const { return !armed_.empty() && armedKey_ == Key(); }
    // Squares the acting unit can walk to, each with the square it is reached from (rules' Check on each step).
    [[nodiscard]] std::vector<std::pair<int, int>> Reach() const;
    [[nodiscard]] nlohmann::json Actions() const;
    [[nodiscard]] std::vector<umbra::match::Action> ArmedActions() const; // the armed action on each valid target
    [[nodiscard]] nlohmann::json Forecast() const;
    [[nodiscard]] nlohmann::json UnitNumbers(int unit) const;
    [[nodiscard]] nlohmann::json Fighter(int unit, int place) const;
    [[nodiscard]] std::string AbilityText(int ability) const; // what an ability does, in words from its numbers
    [[nodiscard]] nlohmann::json BattleData() const;
    [[nodiscard]] nlohmann::json EndData() const;
    void Queue();       // the log entries the rules added since the last call
    bool Reveal();      // shows the next queued entry; false if none
    void RevealAll();

    const umbra::match::MatchDef &def_;
    umbra::match::MatchState s_;
    umbra::match::BotLevel level_;
    nlohmann::json words_;
    umbra::night::GameOptions options_;
    std::vector<std::string> log_;  // captions shown, newest last
    std::size_t queued_ = 0, shown_ = 0; // state log entries queued / revealed
    int lastMover_ = -1;             // a Moved caption is replaced by the same unit's next step
    int shownRound_ = 0;             // the round whose banner has played
    float banner_ = 0.0F, clock_ = 0.0F, wait_ = 0.0F;
    int turnKey_ = -1;      // the turn (or battle pick) last seen, so a new one waits turnPause
    float turnPause_ = 0.0F;
    bool bannerOn_ = false, logOpen_ = true;
    int hover_ = -1, chosen_ = -1, armedKey_ = -1;
    bool watch_ = false, paused_ = false;
    std::vector<std::string> names_, sprites_;
    std::vector<float> heights_;
    std::string submenu_; // "", "skills" or "items"
    int pinned_ = -1;     // the unit whose card is pinned
    int looking_ = -1;    // another unit clicked on the board: its card and reach, the panel aside (notes 7-8)
    bool menuHidden_ = false; // Back with nothing to cancel puts the action panel away; again brings it
    std::vector<umbra::match::Action> choices_; // armed at a square holding several: which one (note 9)
    struct Toasted
    {
        std::string text;
        float left = 3.0F;
    };
    std::vector<Toasted> passives_;   // passives that just worked: their cards (note 20)
    std::set<std::size_t> boardActs_; // log lines that begin a board ability (staged like a battle, note 16)
    std::vector<Toasted> toasts_;
    std::string stageTitle_, hurryKey_ = "Space", skipKey_ = "F";
    std::string armed_; // "attack" or "ability:<slot>"
    std::map<int, std::string> used_; // the ability each unit used last (its bubble in a campaign stage)
    void Act(const umbra::match::Action &action); // Apply, noting the ability used
    bool moving_ = false; // Di chuyen picked: the menu steps aside while a square is chosen
    std::string lastBlow_, eventResult_, treasureResult_; // the battle's last blow; the event's outcome; the card turned
    int eventCell_ = -1;
    nlohmann::json eventDice_;  // the event's two dice, as shown (display only: the rules' roll decides)
    float diceLeft_ = 0.0F;     // seconds the dice still tumble
    int fight_[2] = {-1, -1};   // the battle on the table: attacker, defender (MatchPlay.md "The battle view")
    std::array<int, 4> fighters_{-1, -1, -1, -1}; // ... and everyone in it by place (two a group)
    float fightHold_ = 0.0F;    // seconds it stays after its last line
    std::vector<int> focus_;    // whom the last told line is about, for the camera (MatchPlay.md "Focus")
    std::vector<int> toldHp_;   // HP as the told lines leave it (cards never run ahead of the blows)
    void SyncHp();
    float focusLeft_ = 0.0F;
    int focusNext_ = -1;
    struct Pop // a number rising over a unit when a blow or a heal is told (note 21)
    {
        int unit = -1;
        std::string text;
        bool hurt = true;
        float left = 0.0F;
    };
    std::vector<Pop> pops_;        // who the camera goes to next (a blow's or a heal's receiver)
    struct Bubble
    {
        int unit = -1;
        std::string text;
        float left = 0.0F; // seconds still shown
    };
    std::vector<Bubble> bubbles_;
    void Speak(const umbra::match::LogEntry &e, std::size_t index); // a written line beside its speaker
    std::vector<std::pair<int, std::string>> moves_;
    std::vector<std::string> sounds_;
    std::vector<int> snaps_;
    // The sequence playing for the last told line (Sequences.md "Playing").
    const Sequences *sequences_ = nullptr;
    std::vector<GuideStep> guide_;
    [[nodiscard]] const GuideStep *Guided() const;              // the step for this turn, if any
    [[nodiscard]] std::vector<int> GuideCells() const;          // the squares it lets you walk to (empty: any)
    [[nodiscard]] bool GuideAllows(const std::string &row) const; // a menu row it leaves on
    [[nodiscard]] bool GuideAllows(const umbra::match::Action &a) const;
    [[nodiscard]] bool GuidePossible() const;                   // its action can be taken now
    void GuideDone(const umbra::match::Action &a, int me);      // marks the step done when its action is taken
    std::map<std::size_t, std::string> abilityOf_; // log entry -> the ability id that wrote it
    struct Playing
    {
        const Sequence *sequence = nullptr;
        std::size_t entry = 0, next = 0; // the told line; its next cue
        float time = 0.0F;
    } playing_;
    std::vector<Fx> fx_;
    bool skipping_ = false;
    void Tell(const umbra::match::LogEntry &e);        // the hit: the number rises, the cards follow
    void Play(float dt);                               // fires the cues due by now
    void Finish();                                     // the rest of the cues' hits at once, no effects
    // A campaign stage's outcome banner (Chien thang / That bai): shown once the end is told, before its scenes.
    [[nodiscard]] bool Campaign() const { return !names_.empty(); }
    [[nodiscard]] bool OutcomeDue() const { return Campaign() && s_.over && shown_ == s_.log.size() && !outcomeDone_; }
    float outcomeLeft_ = -1.0F;
    bool outcomeDone_ = false;                 // the room whose event card is open, -1 none
    std::function<umbra::match::Action(const umbra::match::MatchState &)> brain_;
};
