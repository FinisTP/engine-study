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

bool MatchPlay::HandleUi(const std::string &screen, const std::string &action, const std::string &arg)
{
    if (action == "log")
        return logOpen_ = !logOpen_, true;
    if (screen.empty() && action == "act" && PlayerTurn() && Chose())
    {
        // 1. Undo the walk (or go back to the range), arm a target choice, or act at once.
        const int me = Current(s_);
        const Unit &u = s_.units[U(me)];
        if (arg == "undo")
        {
            armed_.clear();
            if (u.cell != s_.now.origin)
                Do({ActionKind::Undo});
            chosen_ = -1;
            return true;
        }
        if (arg == "attack" || arg.starts_with("ability:"))
        {
            armed_ = arg, armedKey_ = Key();
            if (ArmedActions().empty())
                armed_.clear();
            return !armed_.empty();
        }
        if (arg.starts_with("item:"))
            return Do({ActionKind::UseItem, static_cast<std::int8_t>(std::stoi(arg.substr(5)))});
        if (arg == "pickup")
            for (int f = 0; f < static_cast<int>(s_.floor.size()); ++f)
                if (s_.floor[U(f)].cell == u.cell)
                    return Do({ActionKind::PickUp, static_cast<std::int8_t>(f)});
        if (arg == "perform")
            return Do({ActionKind::Perform});
        if (arg == "end")
            return Do({ActionKind::EndTurn});
        return false;
    }
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
    // 3. The player's turn: the move range (or an armed ability's range), the actions, the targets.
    if (PlayerTurn() && !s_.battle.on)
    {
        if (!Chose())
        {
            nlohmann::json reach = nlohmann::json::array(), path = nlohmann::json::array();
            const auto r = Reach();
            for (std::size_t i = 1; i < r.size(); ++i)
                reach.push_back(r[i].first);
            for (int c = hover_; c >= 0 && c != r[0].first;)
            {
                const auto it = std::ranges::find(r, c, &std::pair<int, int>::first);
                if (it == r.end())
                    break;
                path.insert(path.begin(), c);
                c = it->second;
            }
            j["reach"] = reach;
            j["path"] = path;
        }
        else
            j["actions"] = Actions();
        if (Armed())
        {
            nlohmann::json targets = nlohmann::json::array(), range = nlohmann::json::array();
            for (const Action &a : ArmedActions())
                for (int c : Squares(s_.board, s_.units[U(a.kind == ActionKind::Attack ? a.a : a.b)]))
                    targets.push_back(c);
            const int me = Current(s_);
            const int reachSteps = armed_ == "attack" ? 1 : def_.abilities[U(s_.units[U(me)].abilities[U(std::stoi(armed_.substr(8)))])].range;
            for (int c = 0; c < static_cast<int>(s_.board.cells.size()); ++c)
                if (const int d = Steps(s_.board, s_.units[U(me)].cell, c); d > 0 && d <= reachSteps)
                    range.push_back(c);
            j["targets"] = targets;
            j["reach"] = range;
            j["armed"] = armed_;
            j["unit"]["attack"] = s_.units[U(me)].attack;
            j["unit"]["defence"] = s_.units[U(me)].defence;
        }
    }
    j["forecast"] = Forecast();
    j["log"] = log_;
    j["logOpen"] = logOpen_;
    j["banner"] = {{"title", Fill(Word("banner", "title"), {{"n", std::to_string(s_.round)}})},
                   {"text", Fill(Word("banner", "text"), {{"n", std::to_string(TurnsLeft(def_, s_))}})}};
    return j;
}

std::vector<std::pair<int, int>> MatchPlay::Reach() const
{
    // 1. Breadth first from the unit, asking Check for each step as if the unit stood there.
    std::vector<std::pair<int, int>> out;
    const int me = Current(s_);
    if (me < 0 || s_.now.acted)
        return out;
    MatchState work = s_;
    work.log.clear();
    std::vector<int> steps(s_.board.cells.size(), -1);
    out.push_back({s_.units[U(me)].cell, -1});
    steps[U(s_.units[U(me)].cell)] = 0;
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        const int c = out[i].first;
        if (steps[U(c)] >= s_.now.movesLeft)
            continue;
        for (int d = 0; d < 4; ++d)
        {
            const int n = Neighbour(s_.board, c, d);
            if (n < 0 || steps[U(n)] >= 0)
                continue;
            work.units[U(me)].cell = static_cast<std::int16_t>(c);
            work.now.movesLeft = 1;
            if (Check(def_, work, {ActionKind::Move, static_cast<std::int8_t>(d)}) != Why::Ok)
                continue;
            steps[U(n)] = steps[U(c)] + 1;
            out.push_back({n, c});
        }
    }
    return out;
}

std::vector<Action> MatchPlay::ArmedActions() const
{
    std::vector<Action> out;
    if (!Armed())
        return out;
    const bool attack = armed_ == "attack";
    const auto slot = static_cast<std::int8_t>(attack ? -1 : std::stoi(armed_.substr(8)));
    for (int t = 0; t < static_cast<int>(s_.units.size()); ++t)
    {
        const Action a = attack ? Action{ActionKind::Attack, static_cast<std::int8_t>(t)} : Action{ActionKind::BoardAbility, slot, static_cast<std::int8_t>(t)};
        if (Check(def_, s_, a) == Why::Ok)
            out.push_back(a);
    }
    return out;
}

void MatchPlay::Click(int cell)
{
    if (!PlayerTurn() || s_.battle.on || cell < 0)
        return;
    const int me = Current(s_);
    // 1. Armed: a valid target there is attacked; anything else disarms.
    if (Armed())
    {
        for (const Action &a : ArmedActions())
        {
            const int t = a.kind == ActionKind::Attack ? a.a : a.b;
            if (Covers(s_.board, s_.units[U(t)], cell))
            {
                armed_.clear();
                Do(a);
                return;
            }
        }
        armed_.clear();
        return;
    }
    if (Chose())
        return;
    // 2. Oneself stays; a lit square walks there along the path, one legal step at a time.
    const int from = s_.units[U(me)].cell, key = Key();
    if (cell == from)
    {
        chosen_ = key;
        return;
    }
    const auto reach = Reach();
    std::vector<int> path;
    for (int c = cell; c != from;)
    {
        const auto it = std::ranges::find(reach, c, &std::pair<int, int>::first);
        if (it == reach.end())
            return;
        path.push_back(c);
        c = it->second;
    }
    for (auto it = path.rbegin(); it != path.rend(); ++it)
        for (int d = 0; d < 4; ++d)
            if (Neighbour(s_.board, s_.units[U(me)].cell, d) == *it)
            {
                Do({ActionKind::Move, static_cast<std::int8_t>(d)});
                break;
            }
    chosen_ = key;
}

nlohmann::json MatchPlay::Actions() const
{
    // 1. Each entry: {id, label, note, enabled, why}; enabled only when Check says Ok.
    const int me = Current(s_);
    const Unit &u = s_.units[U(me)];
    nlohmann::json list = nlohmann::json::array();
    auto add = [&](const std::string &id, const std::string &label, const std::string &note, Why why) {
        list.push_back({{"id", id}, {"label", label}, {"note", note}, {"enabled", why == Why::Ok}, {"why", why == Why::Ok ? "" : WhyText(why)}});
    };
    auto any = [&](auto make) {
        Why first = Why::NothingHere;
        for (int t = 0; t < static_cast<int>(s_.units.size()); ++t)
            if (const Why w = Check(def_, s_, make(t)); w == Why::Ok)
                return Why::Ok;
            else if (w == Why::AlreadyActed || w == Why::TooCostly)
                first = w;
        return first;
    };
    auto hp = [&](int n) { return Fill(Word("actions", "hpNote"), {{"n", std::to_string(n)}}); };
    const bool moved = u.cell != s_.now.origin;
    list.push_back({{"id", "undo"}, {"label", Word("actions", moved ? "undo" : "back")}, {"note", ""}, {"enabled", !s_.now.acted}, {"why", s_.now.acted ? WhyText(Why::AlreadyActed) : ""}});
    const Why attack = any([](int t) { return Action{ActionKind::Attack, static_cast<std::int8_t>(t)}; });
    list.push_back({{"id", "attack"}, {"label", Word("actions", "attack")}, {"note", ""}, {"enabled", attack == Why::Ok},
                    {"why", attack == Why::Ok ? "" : attack == Why::AlreadyActed ? WhyText(attack) : Word("why", "NoTarget")}});
    for (int slot = 0; slot < maxAbilities; ++slot)
        if (const int id = u.abilities[U(slot)]; id >= 0 && def_.abilities[U(id)].use == Use::Board)
        {
            const AbilityDef &a = def_.abilities[U(id)];
            Why w = any([&](int t) { return Action{ActionKind::BoardAbility, static_cast<std::int8_t>(slot), static_cast<std::int8_t>(t)}; });
            if (w != Why::Ok && !s_.now.acted && u.hp <= a.hpCost)
                w = Why::TooCostly;
            list.push_back({{"id", "ability:" + std::to_string(slot)}, {"label", a.name}, {"note", hp(a.hpCost)}, {"target", static_cast<int>(a.target)},
                            {"enabled", w == Why::Ok}, {"why", w == Why::Ok ? "" : w == Why::NothingHere ? Word("why", "NoTarget") : WhyText(w)}});
        }
    for (int slot = 0; slot < maxItems; ++slot)
        if (u.items[U(slot)] >= 0)
            add("item:" + std::to_string(slot), def_.items[U(u.items[U(slot)])].name, "", Check(def_, s_, {ActionKind::UseItem, static_cast<std::int8_t>(slot)}));
    for (int f = 0; f < static_cast<int>(s_.floor.size()); ++f)
        if (s_.floor[U(f)].cell == u.cell)
            add("pickup", Word("actions", "pickup"), "", Check(def_, s_, {ActionKind::PickUp, static_cast<std::int8_t>(f)}));
    if (const Cell &c = s_.board.cells[U(u.cell)]; c.event >= 0 && !c.resolved)
        add("answer", Word("actions", "answer"), "", Check(def_, s_, {ActionKind::Answer, 0}));
    if (u.cell == s_.stage)
        add("perform", Word("actions", "perform"), Fill(Word("actions", "essenceNote"), {{"n", std::to_string(u.essence)}}), Check(def_, s_, {ActionKind::Perform}));
    add("end", Word("actions", "end"), "", Why::Ok);
    return list;
}

nlohmann::json MatchPlay::UnitNumbers(int unit) const
{
    const Unit &u = s_.units[U(unit)];
    const int level = u.kind == 0 ? u.level : def_.monsters[U(u.def)].level;
    return {{"unit", unit}, {"name", Name(unit)}, {"side", u.faction}, {"level", level}, {"hp", u.hp}, {"maxHp", u.maxHp},
            {"attack", u.attack}, {"defence", u.defence}, {"essence", u.essence}};
}

nlohmann::json MatchPlay::Forecast() const
{
    if (hover_ < 0 || hover_ >= static_cast<int>(s_.board.cells.size()))
        return nullptr;
    const int me = Current(s_);
    // 1. An armed target: its numbers and what the action would do (an attack: the first blow).
    for (const Action &a : ArmedActions())
    {
        const int t = a.kind == ActionKind::Attack ? a.a : a.b;
        if (!Covers(s_.board, s_.units[U(t)], hover_))
            continue;
        nlohmann::json f = UnitNumbers(t);
        f["kind"] = "target";
        if (a.kind == ActionKind::Attack)
        {
            const int strike = s_.units[U(me)].abilities[0];
            const Dice &d = def_.abilities[U(strike)].dice;
            f["dealtMin"] = Damage(def_, s_, strike, me, t, d.count);
            f["dealtMax"] = Damage(def_, s_, strike, me, t, d.count * d.sides);
            f["healed"] = 0, f["cost"] = 0;
        }
        else
        {
            const umbra::match::Forecast p = Preview(def_, s_, a);
            f["dealtMin"] = p.dealtMin, f["dealtMax"] = p.dealtMax, f["healed"] = p.healed, f["cost"] = p.cost;
        }
        return f;
    }
    // 2. The stage, an announced spawn, a unit, else the square's kind.
    if (hover_ == s_.stage)
        return {{"kind", "stage"}, {"text", Word("tiles", "stage")}};
    for (const Spawn &w : s_.spawns)
        if (w.cell == hover_ && w.round - def_.rules.spawn.warn <= s_.round && w.monster >= 0)
            return {{"kind", "spawn"}, {"name", def_.monsters[U(w.monster)].name}, {"level", def_.monsters[U(w.monster)].level}, {"round", w.round}};
    for (int t = 0; t < static_cast<int>(s_.units.size()); ++t)
        if (Covers(s_.board, s_.units[U(t)], hover_))
        {
            nlohmann::json f = UnitNumbers(t);
            f["kind"] = "unit";
            return f;
        }
    static const char *kinds[] = {"wall", "hall", "room", "centre", "hub"};
    return {{"kind", "tile"}, {"text", Word("tiles", kinds[static_cast<int>(s_.board.cells[U(hover_)].kind)])}};
}

nlohmann::json MatchPlay::Save() const { return SaveMatch(s_); }
