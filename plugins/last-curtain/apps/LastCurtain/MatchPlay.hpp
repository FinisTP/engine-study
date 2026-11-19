#pragma once

#include <Match.hpp>
#include <MatchBots.hpp>
#include <Options.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

// One faction match as the person plays it, without a window (tested; MatchPlay.md): the player's
// side waits for input, bots and monsters play one beat at a time, and each screen gets its JSON.
class MatchPlay final
{
public:
    MatchPlay(const umbra::match::MatchDef &def, std::uint64_t seed, std::vector<int> picks, umbra::match::BotLevel level);
    MatchPlay(const umbra::match::MatchDef &def, umbra::match::MatchState saved, umbra::match::BotLevel level);

    void SetWords(nlohmann::json words) { words_ = std::move(words); }      // assets/match-words.json
    void SetOptions(const umbra::night::GameOptions &o) { options_ = o; } // the pace (assets/options.csv)

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
    static const char *LogName(umbra::match::LogKind kind);
    static const char *WhyName(umbra::match::Why why);
    [[nodiscard]] std::string Caption(const umbra::match::LogEntry &e) const;
    [[nodiscard]] std::string WhyText(umbra::match::Why why) const;

private:
    [[nodiscard]] bool Mine(int unit) const;   // a character of the player's side
    [[nodiscard]] bool BannerDue() const;      // a new round not yet announced
    [[nodiscard]] std::string Name(int unit) const;
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
    [[nodiscard]] nlohmann::json Fighter(int unit, int side) const;
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
    std::string armed_; // "attack" or "ability:<slot>"
    std::string lastBlow_, eventResult_; // the battle's last blow; the event's outcome
    int eventCell_ = -1;                 // the room whose event card is open, -1 none
};
