#include <MatchPlay.hpp>

#include "CareerMatch.hpp"
#include <MatchSave.hpp>

#include <algorithm>
#include <tuple>

// The faction match on screen (MatchPlay.md).
using namespace umbra::match;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }

    std::string Portrait(const std::string &sprite) { return "art/sprites/" + sprite + "/face_calm.png"; }

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
    SyncHp();
}

MatchPlay::MatchPlay(const MatchDef &def, MatchState saved, BotLevel level) : def_(def), s_(std::move(saved)), level_(level)
{
    queued_ = shown_ = s_.log.size();
    SyncHp();
}

void MatchPlay::SyncHp()
{
    toldHp_.clear();
    for (const Unit &u : s_.units)
        toldHp_.push_back(u.hp);
}

const char *MatchPlay::LogName(LogKind kind)
{
    static const char *names[] = {"Moved", "Battle", "Blow", "Down", "Essence", "Performed", "Level", "Back",
                                  "SpawnSoon", "Spawned", "Event", "Item", "Treasure", "Over", "Healed", "Bided", "Freed", "Arriving", "Arrived", "Hazard", "Breath"};
    return names[static_cast<int>(kind)];
}

const char *MatchPlay::WhyName(Why why)
{
    static const char *names[] = {"Ok", "NotYourTurn", "NoMovesLeft", "Blocked", "AlreadyActed", "OutOfRange", "OwnSide", "TooCostly",
                                  "NotOnStage", "NothingToPerform", "NothingHere", "BagFull", "MustChoose", "Over", "FullHp", "Off", "Fallen", "NotFallen", "NoAp", "OnlyPlayer", "NotStrung", "NoRite", "NotHere"};
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
    if (U(unit) < names_.size() && !names_[U(unit)].empty())
        return names_[U(unit)]; // a campaign stage: the cast's name
    const Unit &u = s_.units[U(unit)];
    if (u.kind != 0)
        return def_.monsters[U(u.def)].name;
    // The same character on both sides: theirs is tagged.
    const std::string &name = def_.characters[U(u.def)].name;
    const bool twin = std::ranges::any_of(s_.units, [&](const Unit &o) { return o.kind == 0 && o.def == u.def && o.faction != u.faction; });
    return twin && u.faction == 1 ? Fill(Word("misc", "theirs"), {{"name", name}}) : name;
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
    case LogKind::Treasure:
    {
        // What the chosen card held, in words (match-words.json "cards").
        const TreasureDef &t = def_.treasure[U(e.b)];
        static const char *kinds[] = {"item", "essence", "xp", "heal"};
        const std::string card = Fill(Word("cards", kinds[static_cast<int>(t.kind)]), {{"item", t.item >= 0 ? def_.items[U(t.item)].name : ""}, {"n", std::to_string(t.amount)}});
        v = {{"who", Name(e.a)}, {"card", card}};
        break;
    }
    case LogKind::Over: v = {{"side", Word("misc", e.a == 0 ? "side0" : e.a == 1 ? "side1" : "draw")}}; break;
    case LogKind::Arriving:
    case LogKind::Arrived:
    case LogKind::Breath: v = {{"who", Name(e.a)}}; break;
    case LogKind::Hazard: v = {{"who", Name(e.a)}, {"n", std::to_string(e.c)}}; break;
    default: break;
    }
    return Fill(Word("log", LogName(e.kind)), v);
}

bool MatchPlay::Mine(int unit) const { return !watch_ && unit >= 0 && s_.units[U(unit)].kind == 0 && s_.units[U(unit)].faction == 0; }

bool MatchPlay::BannerDue() const { return !s_.over && shown_ == s_.log.size() && shownRound_ != s_.round && fight_[0] < 0; }

bool MatchPlay::PlayerTurn() const
{
    // Not while a finished fight is still on its stage, nor while a round's banner is due.
    return !s_.over && shown_ == s_.log.size() && !BannerDue() && (s_.battle.on || fight_[0] < 0) && Mine(Current(s_));
}

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
    for (const auto &m : Moves(e))
        moves_.push_back(m);
    Speak(e, shown_ - 1);
    if (const std::string toast = ToastText(e); !toast.empty())
        Toast(toast);
    if (const std::string cue = Sound(e); !cue.empty())
        sounds_.push_back(cue);
    // 1b. The cards follow the told lines.
    if (e.kind == LogKind::Blow && U(e.b) < toldHp_.size())
        toldHp_[U(e.b)] = std::max(0, toldHp_[U(e.b)] - e.c);
    else if (e.kind == LogKind::Healed && U(e.b) < toldHp_.size())
        toldHp_[U(e.b)] = std::min<int>(s_.units[U(e.b)].maxHp, toldHp_[U(e.b)] + e.c);
    else if (e.kind == LogKind::Down && U(e.a) < toldHp_.size())
        toldHp_[U(e.a)] = 0;
    if (e.kind == LogKind::Blow)
        lastBlow_ = line;
    else if (e.kind == LogKind::Battle)
    {
        // The fighters of this battle: the rules' list when it is the same battle, else just the two.
        lastBlow_.clear(), fight_[0] = e.a, fight_[1] = e.b;
        fighters_ = {e.a, -1, e.b, -1};
        if (s_.battle.attacker == e.a && s_.battle.defender == e.b)
            for (int p = 0; p < 4; ++p)
                fighters_[U(p)] = s_.battle.fighters[U(p)];
    }
    if (e.kind == LogKind::Battle || e.kind == LogKind::Blow || e.kind == LogKind::Down)
        fightHold_ = options_.beatSeconds;
    wait_ = e.kind == LogKind::Moved ? options_.plainBeatSeconds : e.kind == LogKind::Blow || e.kind == LogKind::Battle ? options_.blowSeconds : options_.beatSeconds;
    // 2. Whom it is about: the camera frames them while the line stays.
    switch (e.kind)
    {
    case LogKind::Battle:
    case LogKind::Blow:
    case LogKind::Healed:
    case LogKind::Down: focus_ = {e.a, e.b}; break;
    case LogKind::Performed:
    case LogKind::Level:
    case LogKind::Event:
    case LogKind::Item:
    case LogKind::Treasure:
    case LogKind::Back: focus_ = {e.a}; break;
    case LogKind::Spawned: focus_ = {e.b}; break;
    default: return true;
    }
    std::erase_if(focus_, [&](int u) { return u < 0 || u >= static_cast<int>(s_.units.size()); });
    focusLeft_ = options_.beatSeconds;
    return true;
}

void MatchPlay::Speak(const LogEntry &e, std::size_t index)
{
    // 1. Who speaks: the striker, the fallen, the performer, whoever levels; characters only.
    const int who = e.kind == LogKind::Blow || e.kind == LogKind::Down || e.kind == LogKind::Performed || e.kind == LogKind::Level ? e.a : -1;
    if (who < 0 || s_.units[U(who)].kind != 0)
        return;
    const auto lines = words_.find("lines");
    if (lines == words_.end() || !lines->contains(LogName(e.kind)) || (*lines)[LogName(e.kind)].empty())
        return;
    // 2. A line picked from the round, the speaker and the moment, so a replay says the same; one bubble per speaker.
    const nlohmann::json &list = (*lines)[LogName(e.kind)];
    const std::string text = list[(static_cast<std::size_t>(s_.round) * 7 + static_cast<std::size_t>(who) * 3 + index) % list.size()];
    std::erase_if(bubbles_, [&](const Bubble &b) { return b.unit == who; });
    bubbles_.push_back({who, text, options_.beatSeconds * 2.0F});
}

void MatchPlay::RevealAll()
{
    Queue();
    while (Reveal()) {}
    if (focus_.empty() || focusLeft_ <= 0.0F)
        wait_ = 0.0F; // nothing to look at: go on at once
}

bool MatchPlay::Do(const Action &action)
{
    if (!PlayerTurn() || Check(def_, s_, action) != Why::Ok)
        return false;
    SyncHp();
    Apply(def_, s_, action);
    Queue();
    // 1. A blow, a heal or a fight is told beat by beat, the player's too; anything else shows at once while it stays their turn.
    const bool told = action.kind == ActionKind::Attack || action.kind == ActionKind::BattleAbility || action.kind == ActionKind::BoardAbility;
    if (told)
        wait_ = options_.fastBeatSeconds;
    else if (!s_.over && Mine(Current(s_)) && shownRound_ == s_.round)
        RevealAll();
    clock_ = 0.0F;
    return true;
}

bool MatchPlay::Quiet() const
{
    return shown_ == s_.log.size() && !s_.battle.on && fight_[0] < 0 && !bannerOn_;
}

void MatchPlay::Update(float dt)
{
    if (dt <= 0.0F || paused_)
        return;
    for (Toasted &t : toasts_)
        t.left -= dt;
    std::erase_if(toasts_, [](const Toasted &t) { return t.left <= 0.0F; });
    diceLeft_ = std::max(0.0F, diceLeft_ - dt);
    if ((focusLeft_ -= dt) <= 0.0F)
        focus_.clear();
    if ((fightHold_ = std::max(0.0F, fightHold_ - dt)) <= 0.0F && !s_.battle.on && shown_ == s_.log.size())
        fight_[0] = fight_[1] = -1, fighters_.fill(-1); // the battle is told: the board comes back
    for (Bubble &b : bubbles_)
        b.left -= dt;
    std::erase_if(bubbles_, [](const Bubble &b) { return b.left <= 0.0F; });
    // 1. A new round's banner, once everything before it has been told.
    if (BannerDue())
    {
        if (!bannerOn_)
            bannerOn_ = true, banner_ = options_.turnBannerSeconds, sounds_.push_back("turn");
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
    if (now < 0 || Mine(now) || (fight_[0] >= 0 && !s_.battle.on)) // a finished fight keeps its stage a moment
        return;
    SyncHp();
    Apply(def_, s_, brain_ ? brain_(s_) : ChooseAction(def_, s_, level_));
    Queue();
    Reveal();
}

bool MatchPlay::HandleUi(const std::string &screen, const std::string &action, const std::string &arg)
{
    if (action == "log")
        return logOpen_ = !logOpen_, true;
    // 1. The battle's pick, the level choice, the event's choice and its close.
    if ((screen == "battle" || screen == "level") && action == "pick" && !arg.empty())
    {
        const auto colon = arg.find(':'); // "slot:target" picks a foe in a group battle
        const int target = colon == std::string::npos ? -1 : std::stoi(arg.substr(colon + 1));
        return Do({screen == "battle" ? ActionKind::BattleAbility : ActionKind::LevelUp, static_cast<std::int8_t>(std::stoi(arg)), static_cast<std::int8_t>(target)});
    }
    // 1b. Treasure: pick a face-down card, see what it was, close.
    if (screen == "treasure")
    {
        if (action == "close" && !treasureResult_.empty())
            return treasureResult_.clear(), true;
        if (action != "pick" || arg.empty())
            return false;
        const std::size_t before = s_.log.size();
        if (!Do({ActionKind::Card, static_cast<std::int8_t>(std::stoi(arg))}))
            return false;
        for (std::size_t i = before; i < s_.log.size(); ++i)
            if (s_.log[i].kind == LogKind::Treasure)
                treasureResult_ = Caption(s_.log[i]);
        return true;
    }
    if (screen == "event" && eventCell_ >= 0)
    {
        if (action == "close" && !eventResult_.empty() && diceLeft_ <= 0.0F)
            return eventCell_ = -1, eventDice_ = nullptr, true;
        if (action != "choose" || !eventResult_.empty() || arg.empty())
            return false;
        const std::size_t before = s_.log.size();
        if (!Do({ActionKind::Answer, static_cast<std::int8_t>(std::stoi(arg))}))
            return false;
        for (std::size_t i = before; i < s_.log.size(); ++i)
            if (s_.log[i].kind == LogKind::Event)
            {
                eventResult_ = Caption(s_.log[i]);
                // 1. Two dice landing on a total that agrees with what the rules decided, against the chance's target.
                const bool passed = s_.log[i].c != 0;
                const int chance = def_.events[U(s_.board.cells[U(eventCell_)].event)].choices[U(std::stoi(arg))].chance;
                const int target = std::clamp(DiceTarget(chance), passed ? 2 : 3, passed ? 12 : 13);
                const int total = passed ? (target + 13) / 2 : (target + 1) / 2;
                const int a = std::clamp((total + 1) / 2, std::max(1, total - 6), 6);
                eventDice_ = {{"a", a}, {"b", total - a}, {"total", total}, {"target", target}, {"passed", passed}};
                diceLeft_ = options_.diceSeconds;
            }
        return true;
    }
    if (screen.empty() && action == "menu")
        return MenuPick(arg); // the campaign's command menu (campaign plan 5)
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
        if (arg == "answer" && Check(def_, s_, {ActionKind::Answer, 0}) == Why::Ok)
            return eventCell_ = u.cell, eventResult_.clear(), true;
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
            Apply(def_, s_, brain_ ? brain_(s_) : ChooseAction(def_, s_, level_));
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
    if (BannerDue())
        return "banner";
    if (eventCell_ >= 0)
        return "event";
    if (!treasureResult_.empty() || (shown_ == s_.log.size() && !s_.cards.empty() && Mine(Current(s_))))
        return "treasure";
    const int now = Current(s_);
    if (shown_ == s_.log.size() && now >= 0 && s_.units[U(now)].pending >= 0 && Mine(now))
        return "level";
    return (s_.battle.on && shown_ == s_.log.size()) || fight_[0] >= 0 ? "battle" : ""; // the fight's own screen stays while it is staged
}

nlohmann::json MatchPlay::UiData() const
{
    nlohmann::json j;
    j["score"] = s_.score;
    j["scores"] = {{"mine", s_.score[0]}, {"theirs", s_.score[1]}};
    j["canSkip"] = !s_.over && !PlayerTurn() && !BannerDue();
    // 1. The order strip: the acting one to the round's end, the marker, then those who have acted.
    nlohmann::json order = nlohmann::json::array();
    // Every unit in the order, monsters too (one Speed order, campaign plan 2); monsters only while on the board.
    const int n = static_cast<int>(s_.order.size()), turn = std::max(0, s_.turn);
    auto entry = [&](int i, bool later) {
        const int u = s_.order[U(i)];
        const Unit &unit = s_.units[U(u)];
        if (unit.kind == 1 && !Up(unit))
            return;
        const std::string sprite = Sprite(u);
        const std::string full = Name(u);
        const std::string tip = Fill(Word("misc", "orderTip"), {{"name", Name(u)}, {"level", std::to_string(unit.level)}, {"hp", std::to_string(U(u) < toldHp_.size() ? toldHp_[U(u)] : unit.hp)},
                                                                  {"maxHp", std::to_string(unit.maxHp)}, {"attack", std::to_string(unit.attack)}, {"defence", std::to_string(unit.defence)},
                                                                  {"essence", std::to_string(unit.essence)}});
        order.push_back({{"unit", u}, {"name", Name(u)}, {"side", unit.faction}, {"sprite", sprite},
                         {"portrait", unit.kind == 0 ? Portrait(sprite) : ""}, {"mine", unit.faction == 0}, {"theirs", unit.faction == 1},
                         {"short", full.substr(full.rfind(' ') + 1)},
                         {"current", !later && i == turn}, {"acted", later}, {"down", !Up(unit)}, {"tip", tip}});
    };
    // Twelve portraits: the rest of this round, the marker, then the rounds after in the same order.
    for (int i = turn; i < n && static_cast<float>(order.size()) < options_.orderLength; ++i)
        entry(i, false);
    order.push_back({{"marker", true}, {"text", Word("misc", "nextTurn")}});
    for (int k = 0; static_cast<float>(order.size()) < options_.orderLength + 1.0F && n > 0 && k < n * 4; ++k)
        entry(k % n, true);
    j["order"] = order;
    j["telegraphs"] = Telegraphs(s_); // marked a round ahead (campaign plan 5)
    // 2. The panel: the acting character, else the player's first one standing.
    int shown = Current(s_);
    for (int k = 0; (shown < 0 || s_.units[U(shown)].kind != 0) && k < n; ++k) // a monster's turn: the next character
        shown = s_.order[U((std::min(turn, n - 1) + k) % n)];
    const Unit &u = s_.units[U(shown)];
    const CharacterDef &c = def_.characters[U(u.def)];
    static const char *roles[] = {"attacker", "defender", "supporter", "speedster", "allrounder"};
    j["unit"] = {{"unit", shown}, {"name", Name(shown)}, {"sprite", Sprite(shown)}, {"portrait", Portrait(Sprite(shown))}, {"side", u.faction}, {"role", Word("roles", roles[static_cast<int>(c.role)])},
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
            j["hint"] = Word("hints", "move");
        }
        else
            j["actions"] = Actions(), j["hint"] = Word("hints", "act");
        if (Armed())
        {
            nlohmann::json targets = nlohmann::json::array(), range = nlohmann::json::array();
            for (const Action &a : ArmedActions())
                for (int c : Squares(s_.board, s_.units[U(a.kind == ActionKind::Attack ? a.a : a.b)]))
                    targets.push_back(c);
            const int me = Current(s_);
            const AbilityDef *ability = armed_ == "attack" ? nullptr : &def_.abilities[U(s_.units[U(me)].abilities[U(std::stoi(armed_.substr(8)))])];
            const int reachSteps = ability ? ability->range : 1, from = s_.units[U(me)].cell, w = s_.board.width;
            for (int c = 0; c < static_cast<int>(s_.board.cells.size()); ++c)
            {
                // The area the ability reaches: steps through doors, or squares counted straight (its reach column).
                const bool straight = ability && ability->reach == RangeBy::Straight;
                const int d = straight ? std::abs(c % w - from % w) + std::abs(c / w - from / w) : Steps(s_.board, from, c);
                if (d > 0 && d <= reachSteps && s_.board.cells[U(c)].kind != CellKind::Wall)
                    range.push_back(c);
            }
            j["targets"] = targets;
            j["reach"] = range;
            j["armed"] = armed_;
            j["hint"] = Word("hints", "target");
            j["unit"]["attack"] = s_.units[U(me)].attack;
            j["unit"]["defence"] = s_.units[U(me)].defence;
        }
    }
    j["forecast"] = Forecast();
    if (s_.battle.on || fight_[0] >= 0)
        j["battle"] = BattleData();
    // The hint line always says what to do next (the user's note 19).
    if (!j.contains("hint"))
    {
        const std::string screen = Screen();
        j["hint"] = screen == "battle" ? Word("hints", PlayerTurn() ? "battle" : "fight")
                  : screen == "level" || screen == "treasure" || screen == "event" ? Word("hints", screen)
                  : s_.over ? std::string{} : Word("hints", "watch");
    }
    if (const int now = Current(s_); now >= 0 && s_.units[U(now)].pending >= 0)
    {
        // 4. The level's cards (Vampire Survivors style): learn, a stat, an upgrade; each with its words.
        const Unit &u = s_.units[U(now)];
        static const char *stats[] = {"hp", "attack", "defence", "speed", "move"};
        nlohmann::json choices = nlohmann::json::array();
        for (const LevelCard &c : s_.offer)
        {
            const int ability = c.kind == 0 ? c.a : c.kind == 2 ? u.abilities[U(c.a)] : -1;
            const std::string name = ability >= 0 ? def_.abilities[U(ability)].name : "";
            const std::string key = c.kind == 0 ? "learn" : c.kind == 1 ? stats[U(c.a)] : c.b == 0 ? "power" : "cost";
            const int amount = c.kind == 1 ? c.b : c.kind == 2 ? (c.b == 0 ? def_.rules.levels.upgradePower : def_.rules.levels.upgradeCost) : 0;
            choices.push_back({{"index", choices.size()}, {"kind", c.kind == 0 ? "learn" : c.kind == 1 ? "stat" : "upgrade"},
                               {"name", Fill(Word("levelcards", key + "Title"), {{"ability", name}, {"n", std::to_string(amount)}})},
                               {"text", Fill(Word("levelcards", key), {{"ability", name}, {"n", std::to_string(amount)}}) +
                                            (ability >= 0 ? "\n" + AbilityText(ability) : std::string{})},
                               {"icon", Word("levelcards", key + "Icon")}});
        }
        j["level"] = {{"unit", now}, {"name", Name(now)}, {"level", s_.units[U(now)].level}, {"choices", choices}};
    }
    if (eventCell_ >= 0)
    {
        const EventDef &e = def_.events[U(s_.board.cells[U(eventCell_)].event)];
        nlohmann::json choices = nlohmann::json::array();
        for (const Choice &c : e.choices)
            choices.push_back({{"label", c.label}, {"chance", c.chance}});
        j["event"] = {{"name", e.name}, {"text", e.text}, {"choices", choices}, {"result", diceLeft_ > 0.0F ? "" : eventResult_}, {"choosing", eventResult_.empty()}};
        if (!eventDice_.is_null() && !eventResult_.empty())
        {
            // While they tumble the faces flicker; then they land.
            nlohmann::json d = eventDice_;
            if (diceLeft_ > 0.0F)
                d["a"] = 1 + static_cast<int>(diceLeft_ * 37.0F) % 6, d["b"] = 1 + static_cast<int>(diceLeft_ * 23.0F) % 6, d["rolling"] = true;
            j["event"]["dice"] = d;
        }
    }
    if (!s_.cards.empty() || !treasureResult_.empty())
    {
        nlohmann::json cards = nlohmann::json::array();
        for (std::size_t c = 0; c < s_.cards.size(); ++c)
            cards.push_back({{"index", c}});
        j["treasure"] = {{"cards", cards}, {"result", treasureResult_}, {"choosing", treasureResult_.empty()}};
    }
    if (s_.over)
        j["end"] = EndData();
    nlohmann::json bubbles = nlohmann::json::array();
    for (const Bubble &b : bubbles_)
        bubbles.push_back({{"unit", b.unit}, {"text", b.text}});
    j["bubbles"] = bubbles;
    if (fight_[0] >= 0)
        j["fight"] = fighters_;
    else if (s_.battle.on) // a battle loaded from a save
        j["fight"] = s_.battle.fighters;
    if (j.contains("fight") && s_.battle.on && shown_ == s_.log.size())
        j["picking"] = Current(s_); // whose pick it is: the stage's arrow
    if (!focus_.empty())
        j["focus"] = focus_;
    // Downed by the rules but not yet told: they stand until their fall is told (MatchPlay.md "The battle view").
    nlohmann::json standing = nlohmann::json::array();
    for (std::size_t i = shown_; i < s_.log.size(); ++i)
        if (s_.log[i].kind == LogKind::Down && s_.units[U(s_.log[i].a)].cell < 0)
            standing.push_back(s_.log[i].a);
    if (!standing.empty())
        j["standing"] = standing;
    j["hp"] = toldHp_; // HP as the told lines leave it, by unit
    j["log"] = log_;
    j["logTail"] = std::vector<std::string>(log_.end() - std::min<std::ptrdiff_t>(6, static_cast<std::ptrdiff_t>(log_.size())), log_.end());
    j["logOpen"] = logOpen_;
    j["logClosed"] = !logOpen_;
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
        list.push_back({{"id", id}, {"label", label}, {"note", note}, {"enabled", why == Why::Ok}, {"off", why != Why::Ok}, {"why", why == Why::Ok ? "" : WhyText(why)}});
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
    list.push_back({{"id", "undo"}, {"label", Word("actions", moved ? "undo" : "back")}, {"note", ""}, {"enabled", !s_.now.acted}, {"off", s_.now.acted},
                    {"why", s_.now.acted ? WhyText(Why::AlreadyActed) : ""}});
    const Why attack = any([](int t) { return Action{ActionKind::Attack, static_cast<std::int8_t>(t)}; });
    list.push_back({{"id", "attack"}, {"label", Word("actions", "attack")}, {"note", ""}, {"enabled", attack == Why::Ok}, {"off", attack != Why::Ok},
                    {"why", attack == Why::Ok ? "" : attack == Why::AlreadyActed ? WhyText(attack) : Word("why", "NoTarget")}});
    for (int slot = 0; slot < maxAbilities; ++slot)
        if (const int id = u.abilities[U(slot)]; id >= 0 && def_.abilities[U(id)].use == Use::Board)
        {
            const AbilityDef &a = def_.abilities[U(id)];
            Why w = any([&](int t) { return Action{ActionKind::BoardAbility, static_cast<std::int8_t>(slot), static_cast<std::int8_t>(t)}; });
            if (w != Why::Ok && !s_.now.acted && u.hp <= a.hpCost)
                w = Why::TooCostly;
            list.push_back({{"id", "ability:" + std::to_string(slot)}, {"label", a.name}, {"note", hp(a.hpCost)}, {"target", static_cast<int>(a.target)},
                            {"enabled", w == Why::Ok}, {"off", w != Why::Ok}, {"why", w == Why::Ok ? "" : w == Why::NothingHere ? Word("why", "NoTarget") : WhyText(w)}});
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
    const std::string sprite = Sprite(unit);
    return {{"unit", unit}, {"name", Name(unit)}, {"side", u.faction}, {"level", level}, {"portrait", Portrait(sprite)}, {"hp", U(unit) < toldHp_.size() ? toldHp_[U(unit)] : u.hp}, {"maxHp", u.maxHp},
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

nlohmann::json MatchPlay::Fighter(int unit, int place) const
{
    nlohmann::json f = UnitNumbers(unit);
    f["shield"] = s_.battle.on ? s_.battle.shield[U(place)] : 0;
    f["group"] = place / 2;
    f["picking"] = s_.battle.on && shown_ == s_.log.size() && Current(s_) == unit;
    f["sprite"] = Sprite(unit);
    return f;
}

nlohmann::json MatchPlay::BattleData() const
{
    const Battle &b = s_.battle;
    // 1. Everyone in it by place: the attacker's group first.
    const std::array<int, 4> places = fight_[0] >= 0 ? fighters_ : std::array<int, 4>{b.fighters[0], b.fighters[1], b.fighters[2], b.fighters[3]};
    nlohmann::json fighters = nlohmann::json::array(), groups = {nlohmann::json::array(), nlohmann::json::array()};
    for (int p = 0; p < 4; ++p)
        if (places[U(p)] >= 0)
            fighters.push_back(Fighter(places[U(p)], p)), groups[U(p / 2)].push_back(Fighter(places[U(p)], p));
    nlohmann::json j{{"fighters", fighters}, {"mine", groups[0]}, {"theirs", groups[1]}, {"round", b.round}, {"rounds", def_.rules.battle.rounds},
                     {"title", Fill(Word("battle", "title"), {{"r", std::to_string(b.round)}, {"n", std::to_string(def_.rules.battle.rounds)}})},
                     {"line", lastBlow_}};
    // 1. The player's pick: each battle ability with what it would do, or why not.
    nlohmann::json picks = nlohmann::json::array();
    if (PlayerTurn() && s_.battle.on)
    {
        // With two foes standing, a strike is offered at each ("Jab: Mai"); the arg is "slot:target".
        const int me = Current(s_);
        const Unit &u = s_.units[U(me)];
        const int mine = static_cast<int>(std::ranges::find(b.fighters, me) - b.fighters.begin());
        std::vector<int> foes;
        for (int p = mine < 2 ? 2 : 0, end = p + 2; p < end; ++p)
            if (const int f = b.fighters[U(p)]; f >= 0 && s_.units[U(f)].hp > 0 && s_.units[U(f)].cell >= 0)
                foes.push_back(f);
        for (int slot = 0; slot < maxAbilities; ++slot)
            if (const int id = u.abilities[U(slot)]; id >= 0 && def_.abilities[U(id)].use == Use::Battle)
            {
                const bool strike = def_.abilities[U(id)].kind == AbilityClass::Strike;
                for (int foe : strike && foes.size() > 1 ? foes : std::vector<int>{-1})
                {
                    const Action a{ActionKind::BattleAbility, static_cast<std::int8_t>(slot), static_cast<std::int8_t>(foe)};
                    const Why w = Check(def_, s_, a);
                    const umbra::match::Forecast f = Preview(def_, s_, a);
                    const std::string label = def_.abilities[U(id)].name + (foe >= 0 ? ": " + Name(foe) : "");
                    picks.push_back({{"slot", foe >= 0 ? std::to_string(slot) + ":" + std::to_string(foe) : std::to_string(slot)}, {"label", label},
                                     {"dealtMin", f.dealtMin}, {"dealtMax", f.dealtMax}, {"healed", f.healed}, {"cost", f.cost},
                                     {"enabled", w == Why::Ok}, {"off", w != Why::Ok}, {"why", w == Why::Ok ? "" : WhyText(w)}});
                }
            }
    }
    j["picks"] = picks;
    return j;
}

std::string MatchPlay::AbilityText(int ability) const
{
    // What it does, from its row: the class's words with its numbers (match-words.json "abilityText").
    const AbilityDef &a = def_.abilities[U(ability)];
    static const char *classes[] = {"strike", "shield", "heal", "hit", "firststrike"};
    const std::string dice = a.dice.count > 0 ? std::to_string(a.dice.count) + "d" + std::to_string(a.dice.sides) : std::to_string(a.power);
    return Fill(Word("abilityText", classes[static_cast<int>(a.kind)]),
                {{"power", dice}, {"cost", std::to_string(a.hpCost)}, {"range", std::to_string(a.range)},
                 {"reach", Word("abilityText", a.reach == RangeBy::Straight ? "straight" : "walk")}, {"where", Word("abilityText", a.use == Use::Board ? "board" : "battle")}});
}

nlohmann::json MatchPlay::EndData() const
{
    // 1. Everything from the state: kills on the units, essence performed and monsters downed from the log.
    int kills[2] = {}, performed[2] = {}, monsters[2] = {};
    for (const Unit &u : s_.units)
        if (u.kind == 0)
            kills[U(u.faction)] += u.kills;
    for (const LogEntry &e : s_.log)
        if (e.kind == LogKind::Performed)
            performed[U(s_.units[U(e.a)].faction)] += e.b;
        else if (e.kind == LogKind::Down && e.b >= 0 && s_.units[U(e.b)].kind == 0 && s_.units[U(e.a)].kind == 1)
            ++monsters[U(s_.units[U(e.b)].faction)];
    nlohmann::json rows = nlohmann::json::array();
    for (auto [key, a, b] : {std::tuple{"score", s_.score[0], s_.score[1]}, {"kills", kills[0], kills[1]}, {"performed", performed[0], performed[1]},
                             {"monsters", monsters[0], monsters[1]}})
        rows.push_back({{"label", Word("end", key)}, {"mine", a}, {"theirs", b}});
    return {{"winner", s_.winner}, {"title", Word("misc", s_.winner == 0 ? "side0" : s_.winner == 1 ? "side1" : "draw")}, {"score", s_.score},
            {"kills", {kills[0], kills[1]}}, {"performed", {performed[0], performed[1]}}, {"monsters", {monsters[0], monsters[1]}}, {"rows", rows}};
}

nlohmann::json MatchPlay::PickData(const MatchDef &def, const std::vector<int> &picks, bool canContinue, BotLevel level)
{
    static const char *roles[] = {"attacker", "defender", "supporter", "speedster", "allrounder"};
    static const char *levels[] = {"easy", "medium", "hard"};
    nlohmann::json cards = nlohmann::json::array();
    for (int c = 0; c < static_cast<int>(def.characters.size()); ++c)
    {
        const CharacterDef &d = def.characters[U(c)];
        cards.push_back({{"id", c}, {"name", d.name}, {"role", roles[static_cast<int>(d.role)]}, {"portrait", Portrait(d.sprite)},
                         {"hp", d.hp}, {"attack", d.attack}, {"defence", d.defence}, {"speed", d.speed}, {"move", d.move},
                         {"picked", std::ranges::find(picks, c) != picks.end()}});
    }
    return {{"characters", cards}, {"count", picks.size()}, {"side", def.options.side},
            {"canStart", static_cast<int>(picks.size()) == def.options.side}, {"cannotStart", static_cast<int>(picks.size()) != def.options.side}, {"canContinue", canContinue}, {"level", levels[static_cast<int>(level)]},
            {"easy", level == BotLevel::Easy}, {"medium", level == BotLevel::Medium}, {"hard", level == BotLevel::Hard}};
}

bool MatchPlay::TogglePick(const MatchDef &def, std::vector<int> &picks, int character)
{
    if (character < 0 || character >= static_cast<int>(def.characters.size()))
        return false;
    if (const auto it = std::ranges::find(picks, character); it != picks.end())
        return picks.erase(it), true;
    if (static_cast<int>(picks.size()) >= def.options.side)
        return false;
    picks.push_back(character);
    return true;
}

std::vector<std::pair<int, std::string>> MatchPlay::Moves(const LogEntry &e)
{
    switch (e.kind)
    {
    case LogKind::Moved: return {{e.a, "walk"}};
    case LogKind::Blow: return {{e.a, "attack"}, {e.b, "hit"}};
    case LogKind::Healed: return {{e.a, "power"}, {e.b, "levelup"}};
    case LogKind::Down: return {{e.a, "down"}};
    case LogKind::Back: return {{e.a, "revive"}};
    case LogKind::Performed: return {{e.a, "bow"}};
    case LogKind::Level: return {{e.a, "levelup"}};
    case LogKind::Spawned: return {{e.b, "loom"}};
    case LogKind::Event: return {{e.a, e.c ? "pass" : "fail"}};
    case LogKind::Item:
    case LogKind::Treasure: return {{e.a, "pickup"}};
    default: return {};
    }
}

std::string MatchPlay::Sound(const LogEntry &e)
{
    switch (e.kind)
    {
    case LogKind::Moved: return "step";
    case LogKind::Blow: return "hit";
    case LogKind::Down: return "death";
    case LogKind::Performed:
    case LogKind::Healed:
    case LogKind::Level: return "pass";
    case LogKind::SpawnSoon:
    case LogKind::Spawned: return "door";
    case LogKind::Event: return e.c ? "pass" : "fail";
    case LogKind::Item:
    case LogKind::Treasure: return "item";
    case LogKind::Over: return "bell";
    default: return "";
    }
}

int MatchPlay::DiceTarget(int chance)
{
    // 1. Ways two dice reach each total or more, of 36; the target whose odds are nearest the chance (ties: the lower).
    int best = 13, bestGap = 1 << 20;
    for (int t = 2; t <= 13; ++t)
    {
        int ways = 0;
        for (int a = 1; a <= 6; ++a)
            for (int b = 1; b <= 6; ++b)
                ways += a + b >= t;
        const int gap = std::abs(ways * 100 - chance * 36);
        if (gap < bestGap)
            best = t, bestGap = gap;
    }
    return best;
}

nlohmann::json MatchPlay::Save() const { return SaveMatch(s_); }

void MatchPlay::StartFrom(const umbra::cafe::Career &career)
{
    umbra::night::ApplyCareer(def_, s_, career);
    SyncHp();
}

std::string MatchPlay::Sprite(int unit) const
{
    if (U(unit) < sprites_.size() && !sprites_[U(unit)].empty())
        return sprites_[U(unit)];
    const Unit &u = s_.units[U(unit)];
    return u.kind == 0 ? def_.characters[U(u.def)].sprite : def_.monsters[U(u.def)].sprite;
}

nlohmann::json MatchPlay::HudData() const
{
    nlohmann::json j;
    // 1. Goals: each boss (one line when there are more than two), each recruit to free.
    nlohmann::json lines = nlohmann::json::array();
    std::vector<int> bosses, recruits;
    for (int i = 0; i < static_cast<int>(s_.units.size()); ++i)
    {
        const Unit &u = s_.units[U(i)];
        if (u.flags & flag::Boss)
            bosses.push_back(i);
        if ((u.flags & flag::Recruit) && s_.mode.mustFree)
            recruits.push_back(i);
    }
    const auto down = [&](int i) { return !Up(s_.units[U(i)]) && std::ranges::none_of(s_.arrivals, [&](const Arrival &a) { return a.unit == i; }); };
    if (bosses.size() > 2)
        lines.push_back({{"text", Word("goals", "all")}, {"done", std::ranges::all_of(bosses, down)}, {"open", !std::ranges::all_of(bosses, down)}});
    else
        for (const int b : bosses)
            lines.push_back({{"text", Fill(Word("goals", "down"), {{"name", Name(b)}})}, {"done", down(b)}, {"open", !down(b)}});
    for (const int r : recruits)
        lines.push_back({{"text", Fill(Word("goals", "free"), {{"name", Name(r)}})}, {"done", (s_.units[U(r)].flags & flag::Freed) != 0}, {"open", (s_.units[U(r)].flags & flag::Freed) == 0}});
    j["goals"] = {{"title", stageTitle_}, {"lines", lines}, {"failed", s_.over && s_.loss == Loss::Trapped}};
    // 2. This round's strip: from the one acting to the round's end, standing units only.
    nlohmann::json strip = nlohmann::json::array();
    for (int i = std::max(0, s_.turn); i < static_cast<int>(s_.order.size()); ++i)
    {
        const int u = s_.order[U(i)];
        if (!Up(s_.units[U(u)]))
            continue;
        strip.push_back({{"unit", u}, {"name", Name(u)}, {"portrait", Portrait(Sprite(u))}, {"mine", s_.units[U(u)].faction == 0},
                         {"theirs", s_.units[U(u)].faction != 0}, {"acting", u == Current(s_)}});
    }
    j["strip"] = strip;
    // 3. The hint (whose turn, the keys) and the last told line.
    const int now = Current(s_);
    j["hint"] = now >= 0 && !Mine(now) && !s_.over ? Fill(Word("misc", "enemyTurn"), {{"name", Name(now)}, {"hurry", hurryKey_}, {"skip", skipKey_}})
                                                   : UiData().value("hint", std::string());
    j["logLine"] = log_.empty() ? std::string() : log_.back();
    j["toasts"] = nlohmann::json::array();
    for (const Toasted &t : toasts_)
        j["toasts"].push_back({{"text", t.text}});
    // 4. The card: the pinned unit, else the one under the mouse.
    int card = pinned_;
    for (int i = 0; card < 0 && i < static_cast<int>(s_.units.size()) && hover_ >= 0; ++i)
        if (Up(s_.units[U(i)]) && Covers(s_.board, s_.units[U(i)], hover_))
            card = i;
    j["card"] = nullptr;
    if (card >= 0)
    {
        static const char *roles[] = {"attacker", "defender", "supporter", "speedster", "allrounder"};
        const Unit &u = s_.units[U(card)];
        nlohmann::json c = UnitNumbers(card);
        c["speed"] = u.speed;
        c["role"] = u.kind == 0 ? Word("roles", roles[static_cast<int>(def_.characters[U(u.def)].role)]) : std::string();
        c["pinned"] = card == pinned_;
        j["card"] = c;
    }
    return j;
}

nlohmann::json MatchPlay::Menu() const
{
    nlohmann::json rows = nlohmann::json::array();
    std::string title;
    if (!PlayerTurn() || s_.battle.on || s_.over)
        return {{"title", ""}, {"rows", rows}};
    const auto icon = [](const std::string &id) -> std::string {
        // The row's picture (art/ui/campaign): a command, or an item by its id; none for targets.
        const std::string dir = "art/ui/campaign/";
        if (id.starts_with("item:"))
            return dir + "item/" + id.substr(5) + ".png";
        const std::string cmd = id == "undo" || id == "stay" ? "move" : id.starts_with("cut:") ? "cut" : id.starts_with("ability:") ? "skills" : id;
        return cmd == "move" || cmd == "attack" || cmd == "skills" || cmd == "items" || cmd == "end" || cmd == "cut" ? dir + "icon/cmd_" + cmd + ".png" : "";
    };
    const auto row = [&](const std::string &id, const std::string &label, const std::string &note, bool off, const std::string &why, bool sub) {
        rows.push_back({{"id", id}, {"label", label + (sub ? "  \u00bb" : "")}, {"note", note}, {"off", off}, {"on", !off}, {"why", why},
                        {"key", std::to_string(rows.size() + 1)}, {"icon", icon(id)}});
    };
    const nlohmann::json actions = Chose() ? Actions() : nlohmann::json::array();
    const auto starts = [](const nlohmann::json &a, const char *prefix) { return a["id"].get<std::string>().starts_with(prefix); };
    if (Armed())
    {
        // 1. Armed: each target, by name.
        title = Word("actions", "target");
        int k = 0;
        for (const Action &a : ArmedActions())
        {
            const nlohmann::json f = PreviewFor(a);
            const std::string note = f.is_null() ? std::string()
                                                 : Fill(Word("actions", f["counterMax"].get<int>() > 0 ? "previewCounter" : "preview"),
                                                        {{"min", std::to_string(f["min"].get<int>())}, {"max", std::to_string(f["max"].get<int>())},
                                                         {"cmin", std::to_string(f["counterMin"].get<int>())}, {"cmax", std::to_string(f["counterMax"].get<int>())}}) +
                                                       (f["warn"].get<std::string>().empty() ? "" : "  " + f["warn"].get<std::string>());
            row("target:" + std::to_string(k++), Name(a.kind == ActionKind::Attack ? a.a : a.b), note, false, "", false);
        }
    }
    else if (!Chose())
    {
        // 2. Walking: click a lit square, or stay; or end at once.
        row("stay", Word("actions", "stay"), "", false, "", false);
        row("end", Word("actions", "end"), "", false, "", false);
    }
    else if (!submenu_.empty())
    {
        // 3. A sub-menu: abilities or items, each with its note and why.
        title = Word("actions", submenu_);
        for (const auto &a : actions)
            if (starts(a, submenu_ == "skills" ? "ability:" : "item:"))
                row(a["id"], a["label"], a["note"], a["off"], a["why"], false);
    }
    else
    {
        // 4. The main menu.
        const int me = Current(s_);
        bool anySkill = false, anyItem = false;
        for (const auto &a : actions)
            anySkill |= starts(a, "ability:") && !a["off"].get<bool>(), anyItem |= starts(a, "item:") && !a["off"].get<bool>();
        for (const auto &a : actions)
            if (a["id"] == "undo")
                row("undo", Word("actions", "move"), "", a["off"], a["why"], false);
            else if (a["id"] == "attack")
                row("attack", a["label"], "", a["off"], a["why"], false);
        row("skills", Word("actions", "skills"), "", !anySkill, anySkill ? "" : Word("why", "NoTarget"), true);
        row("items", Word("actions", "items"), "", !anyItem, anyItem ? "" : Word("why", "NothingHere"), true);
        for (int t = 0; t < static_cast<int>(s_.units.size()); ++t)
            if (Check(def_, s_, {ActionKind::Cut, static_cast<std::int8_t>(t)}) == Why::Ok)
            {
                row("cut:" + std::to_string(t), Word("actions", "cut"), Name(t), false, "", false);
                break;
            }
        for (const auto &a : actions)
            if (a["id"] == "pickup" || a["id"] == "answer" || a["id"] == "perform")
                row(a["id"], a["label"], a["note"], a["off"], a["why"], false);
        row("end", Word("actions", "end"), "", false, "", false);
        (void)me;
    }
    return {{"title", title}, {"rows", rows}};
}

bool MatchPlay::MenuKey(int n)
{
    const nlohmann::json rows = Menu()["rows"];
    if (n < 1 || n > static_cast<int>(rows.size()) || rows[U(n - 1)]["off"].get<bool>())
        return false;
    return MenuPick(rows[U(n - 1)]["id"]);
}

bool MatchPlay::MenuPick(const std::string &id)
{
    if (!PlayerTurn() || s_.battle.on)
        return false;
    if (id == "skills" || id == "items")
        return submenu_ = id, true;
    submenu_.clear();
    if (id == "stay")
        return chosen_ = Key(), true;
    if (id == "end" && !Chose())
        return Do({ActionKind::EndTurn});
    if (id.starts_with("target:"))
    {
        const std::vector<Action> targets = ArmedActions();
        const std::size_t k = static_cast<std::size_t>(std::stoi(id.substr(7)));
        armed_.clear();
        return k < targets.size() && Do(targets[k]);
    }
    if (id.starts_with("cut:"))
        return Do({ActionKind::Cut, static_cast<std::int8_t>(std::stoi(id.substr(4)))});
    return HandleUi("", "act", id);
}

bool MatchPlay::Back()
{
    if (!PlayerTurn() || s_.battle.on)
        return false;
    if (Armed())
        return armed_.clear(), true;
    if (!submenu_.empty())
        return submenu_.clear(), true;
    if (Chose() && !s_.now.acted)
        return HandleUi("", "act", "undo");
    return false;
}

nlohmann::json MatchPlay::PreviewFor(const Action &action) const
{
    const int me = Current(s_);
    const int target = action.kind == ActionKind::Attack ? action.a : action.b;
    if (me < 0 || target < 0 || target >= static_cast<int>(s_.units.size()))
        return nullptr;
    // 1. The best battle strike from one unit to another, low and high roll.
    const auto strike = [&](int from, int to) {
        std::pair<int, int> best{0, 0};
        for (const int a : s_.units[U(from)].abilities)
            if (a >= 0 && def_.abilities[U(a)].use == Use::Battle && def_.abilities[U(a)].kind == AbilityClass::Strike)
            {
                const Dice &d = def_.abilities[U(a)].dice;
                const int high = Damage(def_, s_, a, from, to, d.count * d.sides);
                if (high > best.second)
                    best = {Damage(def_, s_, a, from, to, d.count), high};
            }
        return best;
    };
    std::pair<int, int> dealt{0, 0};
    if (action.kind == ActionKind::Attack)
        dealt = strike(me, target);
    else
    {
        const umbra::match::Forecast f = Preview(def_, s_, action);
        dealt = {f.dealtMin, f.dealtMax};
    }
    const auto counter = action.kind == ActionKind::Attack ? strike(target, me) : std::pair<int, int>{0, 0};
    // 2. One warning word: the target is faster and doubles its blows on the slower (speedkill).
    const Unit &u = s_.units[U(me)], &t = s_.units[U(target)];
    bool speedkill = false;
    for (const int a : t.abilities)
        speedkill |= a >= 0 && def_.abilities[U(a)].effect == AbilityEffect::Speedkill;
    return {{"min", dealt.first}, {"max", dealt.second}, {"counterMin", counter.first}, {"counterMax", counter.second}, {"hp", t.hp},
            {"warn", speedkill && t.speed > u.speed ? Word("actions", "warnFaster") : std::string()}};
}

bool MatchPlay::Pin()
{
    int unit = -1;
    for (int i = 0; unit < 0 && i < static_cast<int>(s_.units.size()) && hover_ >= 0; ++i)
        if (Up(s_.units[U(i)]) && Covers(s_.board, s_.units[U(i)], hover_))
            unit = i;
    if (pinned_ >= 0)
        return pinned_ = -1, true;
    pinned_ = unit;
    return unit >= 0;
}

nlohmann::json MatchPlay::Telegraphs(const MatchState &s)
{
    nlohmann::json out = nlohmann::json::array();
    static const char *kinds[] = {"hazard", "breath", "window"};
    for (const Mark &m : s.gimmicks.marks)
        out.push_back({{"cell", m.cell}, {"kind", kinds[std::clamp<int>(m.kind, 0, 2)]}});
    for (const Arrival &a : s.arrivals)
        if (a.round <= s.round + 1)
            out.push_back({{"cell", a.cell}, {"kind", "arrival"}});
    return out;
}

std::string MatchPlay::ToastText(const LogEntry &e) const
{
    return e.kind == LogKind::Item || e.kind == LogKind::Freed || e.kind == LogKind::Level ? Caption(e) : std::string();
}

void MatchPlay::Toast(std::string text)
{
    toasts_.push_back({std::move(text), 3.0F});
    if (toasts_.size() > 3)
        toasts_.erase(toasts_.begin());
}
