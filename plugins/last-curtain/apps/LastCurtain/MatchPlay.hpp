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
    [[nodiscard]] const umbra::match::MatchState &State() const { return s_; }
    [[nodiscard]] nlohmann::json Save() const;

    static const char *LogName(umbra::match::LogKind kind);
    static const char *WhyName(umbra::match::Why why);
    [[nodiscard]] std::string Caption(const umbra::match::LogEntry &e) const;
    [[nodiscard]] std::string WhyText(umbra::match::Why why) const;

private:
    [[nodiscard]] bool Mine(int unit) const;   // a character of the player's side
    [[nodiscard]] bool BannerDue() const;      // a new round not yet announced
    [[nodiscard]] std::string Name(int unit) const;
    [[nodiscard]] std::string Word(const std::string &group, const std::string &key) const;
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
};
