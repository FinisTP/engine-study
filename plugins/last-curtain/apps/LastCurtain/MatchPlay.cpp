#include <MatchPlay.hpp>

#include <MatchSave.hpp>

#include <algorithm>

// The faction match on screen (MatchPlay.md).
using namespace umbra::match;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }

    // Every "{key}" in text replaced by its value.
    std::string Fill(std::string text, const std::vector<std::pair<std::string, std::string>> &values)
    {
        for (const auto &[key, value] : values)
            for (std::size_t at = 0; (at = text.find("{" + key + "}", at)) != std::string::npos; at += value.size())
                text.replace(at, key.size() + 2, value);
        return text;
    }
}

MatchPlay::MatchPlay(const MatchDef &def, std::uint64_t seed, std::vector<int> picks, BotLevel level)
    : def_(def), s_(NewMatch(def, seed, picks)), level_(level)
{
    queued_ = shown_ = s_.log.size(); // the setup is not told
}

MatchPlay::MatchPlay(const MatchDef &def, MatchState saved, BotLevel level) : def_(def), s_(std::move(saved)), level_(level)
{
    queued_ = shown_ = s_.log.size();
}

const char *MatchPlay::LogName(LogKind kind)
{
    static const char *names[] = {"Moved", "Battle", "Blow", "Down", "Essence", "Performed", "Level", "Back",
                                  "SpawnSoon", "Spawned", "Event", "Item", "Over"};
    return names[static_cast<int>(kind)];
}

const char *MatchPlay::WhyName(Why why)
{
    static const char *names[] = {"Ok", "NotYourTurn", "NoMovesLeft", "Blocked", "AlreadyActed", "OutOfRange", "OwnSide", "TooCostly",
                                  "NotOnStage", "NothingToPerform", "NothingHere", "BagFull", "MustChoose", "Over"};
    return names[static_cast<int>(why)];
}

std::string MatchPlay::Word(const std::string &group, const std::string &key) const
{
    const auto g = words_.find(group);
    return g != words_.end() && g->is_object() ? g->value(key, std::string()) : std::string();
}

std::string MatchPlay::Name(int unit) const
{
    if (unit < 0 || unit >= static_cast<int>(s_.units.size()))
        return Word("misc", "nobody");
    const Unit &u = s_.units[U(unit)];
    return u.kind == 0 ? def_.characters[U(u.def)].name : def_.monsters[U(u.def)].name;
}

std::string MatchPlay::WhyText(Why why) const { return Word("why", WhyName(why)); }

std::string MatchPlay::Caption(const LogEntry &e) const
{
    // 1. Who and what each kind's numbers mean (Match.md, the log), then the words.
    const std::string n = std::to_string(e.c);
    std::vector<std::pair<std::string, std::string>> v{{"who", Name(e.a)}, {"whom", Name(e.b)}, {"n", n}};
    switch (e.kind)
    {
    case LogKind::Performed:
    case LogKind::Level: v = {{"who", Name(e.a)}, {"n", std::to_string(e.b)}}; break;
    case LogKind::SpawnSoon: v = {{"monster", e.b >= 0 ? def_.monsters[U(e.b)].name : ""}, {"n", n}}; break;
    case LogKind::Spawned: v = {{"who", Name(e.b)}}; break;
    case LogKind::Event: v = {{"who", Name(e.a)}, {"result", Word("misc", e.c ? "passed" : "failed")}}; break;
    case LogKind::Item: v = {{"who", Name(e.a)}, {"item", e.b >= 0 ? def_.items[U(e.b)].name : Word("misc", "essence")}, {"n", n}}; break;
    case LogKind::Over: v = {{"side", Word("misc", e.a == 0 ? "side0" : e.a == 1 ? "side1" : "draw")}}; break;
    default: break;
    }
    return Fill(Word("log", LogName(e.kind)), v);
}

bool MatchPlay::Mine(int unit) const { return unit >= 0 && s_.units[U(unit)].kind == 0 && s_.units[U(unit)].faction == 0; }

bool MatchPlay::BannerDue() const { return !s_.over && shown_ == s_.log.size() && shownRound_ != s_.round; }

bool MatchPlay::PlayerTurn() const { return !s_.over && shown_ == s_.log.size() && !BannerDue() && Mine(Current(s_)); }

void MatchPlay::Queue() { queued_ = s_.log.size(); }

bool MatchPlay::Reveal()
{
    if (shown_ >= queued_)
        return false;
    const LogEntry &e = s_.log[shown_++];
    // 1. A unit's steps share one line; anything else starts a new one.
    const std::string line = Caption(e);
    if (e.kind == LogKind::Moved && e.a == lastMover_ && !log_.empty())
        log_.back() = line;
    else
        log_.push_back(line);
    lastMover_ = e.kind == LogKind::Moved ? e.a : -1;
    wait_ = e.kind == LogKind::Moved ? options_.plainBeatSeconds : options_.beatSeconds;
    return true;
}

void MatchPlay::RevealAll()
{
    Queue();
    while (Reveal()) {}
    wait_ = 0.0F;
}

bool MatchPlay::Do(const Action &action)
{
    if (!PlayerTurn() || Check(def_, s_, action) != Why::Ok)
        return false;
    Apply(def_, s_, action);
    Queue();
    // 1. While it stays the player's turn, what they did shows at once; else it plays out in beats.
    if (!s_.over && Mine(Current(s_)) && shownRound_ == s_.round)
        RevealAll();
    clock_ = 0.0F;
    return true;
}

void MatchPlay::Update(float dt)
{
    if (dt <= 0.0F)
        return;
    // 1. A new round's banner, once everything before it has been told.
    if (BannerDue())
    {
        if (!bannerOn_)
            bannerOn_ = true, banner_ = options_.turnBannerSeconds;
        if ((banner_ -= dt) > 0.0F)
            return;
        bannerOn_ = false, shownRound_ = s_.round, clock_ = 0.0F, wait_ = 0.0F;
        return;
    }
    // 2. One beat at a time: the next line the rules wrote, else a bot's next action.
    if ((clock_ += dt) < wait_)
        return;
    clock_ = 0.0F;
    if (Reveal() || s_.over || shown_ != s_.log.size())
        return;
    const int now = Current(s_);
    if (now < 0 || Mine(now))
        return;
    Apply(def_, s_, ChooseAction(def_, s_, level_));
    Queue();
    Reveal();
}

bool MatchPlay::HandleUi(const std::string &screen, const std::string &action, const std::string &)
{
    if (action == "log")
        return logOpen_ = !logOpen_, true;
    if (screen.empty() && action == "skip")
    {
        // 1. Everything told at once and every bot action taken, up to the player's next turn.
        RevealAll();
        for (int guard = 0; guard < 100000 && !s_.over && !Mine(Current(s_)); ++guard)
        {
            Apply(def_, s_, ChooseAction(def_, s_, level_));
            RevealAll();
        }
        if (bannerOn_)
            banner_ = 0.0F;
        return true;
    }
    return false;
}

std::string MatchPlay::Screen() const
{
    if (s_.over && shown_ == s_.log.size())
        return "end";
    return BannerDue() ? "banner" : "";
}

nlohmann::json MatchPlay::UiData() const
{
    nlohmann::json j;
    j["score"] = s_.score;
    // 1. The order strip: the acting one to the round's end, the marker, then those who have acted.
    nlohmann::json order = nlohmann::json::array();
    const int n = static_cast<int>(s_.order.size()), turn = s_.monsterTurn >= 0 ? n : s_.turn;
    auto entry = [&](int i, bool acted) {
        const int u = s_.order[U(i)];
        const Unit &unit = s_.units[U(u)];
        order.push_back({{"unit", u}, {"name", Name(u)}, {"side", unit.faction}, {"sprite", def_.characters[U(unit.def)].sprite},
                         {"current", !acted && i == turn}, {"acted", acted}, {"down", unit.cell < 0}});
    };
    for (int i = turn; i < n; ++i)
        entry(i, false);
    order.push_back({{"marker", true}, {"text", Word("misc", "nextTurn")}});
    for (int i = 0; i < turn; ++i)
        entry(i, true);
    j["order"] = order;
    // 2. The panel: the acting character, else the player's first one standing.
    int shown = Current(s_);
    if (shown < 0 || s_.units[U(shown)].kind != 0)
        shown = s_.order[U(std::min(turn, n - 1))];
    const Unit &u = s_.units[U(shown)];
    const CharacterDef &c = def_.characters[U(u.def)];
    static const char *roles[] = {"attacker", "defender", "supporter", "speedster", "allrounder"};
    j["unit"] = {{"unit", shown}, {"name", c.name}, {"sprite", c.sprite}, {"side", u.faction}, {"role", Word("roles", roles[static_cast<int>(c.role)])},
                 {"level", u.level}, {"hp", u.hp}, {"maxHp", u.maxHp}, {"essence", u.essence}};
    j["log"] = log_;
    j["logOpen"] = logOpen_;
    j["banner"] = {{"title", Fill(Word("banner", "title"), {{"n", std::to_string(s_.round)}})},
                   {"text", Fill(Word("banner", "text"), {{"n", std::to_string(TurnsLeft(def_, s_))}})}};
    return j;
}

nlohmann::json MatchPlay::Save() const { return SaveMatch(s_); }
