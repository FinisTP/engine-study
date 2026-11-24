#pragma once

#include <array>
#include <Match.hpp>
#include <MatchBots.hpp>
#include <Options.hpp>

#include <nlohmann/json.hpp>

#include <functional>

#include <string>
#include <utility>
#include <vector>

// One faction match as the person plays it, without a window (tested; MatchPlay.md): the player's
// side waits for input, bots and monsters play one beat at a time, and each screen gets its JSON.
namespace umbra::cafe { struct Career; }

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
    bool Back();                        // one level back: targets, sub-menu, the walk; false at the top
    // What an action would do (campaign plan 5): {min, max, counterMin, counterMax, hp, warn}; an attack
    // uses the best battle strike each way, a board ability the rules' Preview.
    [[nodiscard]] nlohmann::json PreviewFor(const umbra::match::Action &action) const;
    bool Pin(); // the card stays on the hovered unit (I); again unpins
    // What the board marks a round ahead (campaign plan 5): [{cell, kind}], kind hazard, breath, window, arrival.
    static nlohmann::json Telegraphs(const umbra::match::MatchState &s);
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
    bool bannerOn_ = false, logOpen_ = true;
    int hover_ = -1, chosen_ = -1, armedKey_ = -1;
    bool watch_ = false, paused_ = false;
    std::vector<std::string> names_, sprites_;
    std::vector<float> heights_;
    std::string submenu_; // "", "skills" or "items"
    int pinned_ = -1;     // the unit whose card is pinned
    struct Toasted
    {
        std::string text;
        float left = 3.0F;
    };
    std::vector<Toasted> toasts_;
    std::string stageTitle_, hurryKey_ = "Space", skipKey_ = "F";
    std::string armed_; // "attack" or "ability:<slot>"
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
    struct Bubble
    {
        int unit = -1;
        std::string text;
        float left = 0.0F; // seconds still shown
    };
    std::vector<Bubble> bubbles_;
    void Speak(const umbra::match::LogEntry &e, std::size_t index); // a written line beside its speaker
    std::vector<std::pair<int, std::string>> moves_;
    std::vector<std::string> sounds_;                 // the room whose event card is open, -1 none
    std::function<umbra::match::Action(const umbra::match::MatchState &)> brain_;
};
