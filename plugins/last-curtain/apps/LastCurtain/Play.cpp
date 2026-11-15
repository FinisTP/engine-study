#include "Play.hpp"

#include "Art.hpp"

#include <BoardView.hpp>
#include <NightBots.hpp>
#include <NightSave.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cstdio>

using namespace umbra::night;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }

    int ParseInt(const std::string &text)
    {
        int value = -1;
        const char *end = text.data() + text.size();
        return !text.empty() && std::from_chars(text.data(), end, value).ptr == end ? value : -1;
    }

    const char *StatName(StatKind stat)
    {
        switch (stat)
        {
        case StatKind::Might: return "Might";
        case StatKind::Sanity: return "Sanity";
        case StatKind::Speed: return "Speed";
        default: return "";
        }
    }

    // What an event's outcome does, in a few words.
    std::string EffectText(Effect effect, int amount)
    {
        const std::string n = (amount > 0 ? "+" : "") + std::to_string(amount);
        switch (effect)
        {
        case Effect::Hp: return n + " HP";
        case Effect::MaxHp: return n + " max HP";
        case Effect::Might: return n + " Might";
        case Effect::Sanity: return n + " Sanity";
        case Effect::Speed: return n + " Speed";
        case Effect::Xp: return n + " XP";
        case Effect::Item: return "an item";
        case Effect::Back: return "pushed back to the room you came from";
        default: return "nothing happens";
        }
    }

    // A Taken's power in a line (powers.json).
    std::string PowerText(const PowerDef &p)
    {
        const std::string damage = std::to_string(p.damage), range = std::to_string(p.range);
        switch (p.kind)
        {
        case PowerKind::Line: return damage + " damage to a guest in a straight line of open passages";
        case PowerKind::Area: return damage + " damage to everyone within " + range + " rooms";
        case PowerKind::Flood: return "floods the rooms within " + range + "; " + damage + " damage to whoever ends a turn there";
        case PowerKind::Pull: return "drags a guest within " + range + " rooms to its side";
        case PowerKind::Seal: return "seals a passage within " + range + " rooms for " + std::to_string(p.rounds) + " rounds";
        case PowerKind::Swap: return "trades places with a guest";
        case PowerKind::Reflect: return "turns the next blow back on the striker";
        }
        return "";
    }

    const char *KindName(ItemKind kind)
    {
        switch (kind)
        {
        case ItemKind::Heal: return "healing";
        case ItemKind::Boost: return "fight boost";
        case ItemKind::Worn: return "worn";
        case ItemKind::Relic: return "relic";
        case ItemKind::Keepsake: return "keepsake";
        }
        return "";
    }
}

namespace
{
    // The bots' own choice, timed.
    Play::Decider Timed(const NightDef &def)
    {
        return [&def](const NightState &state, int guest, float &seconds) {
            const auto start = std::chrono::steady_clock::now();
            const Action action = ChooseAction(def, state, guest);
            seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
            return action;
        };
    }
}

Play::Play(const NightDef &def, std::uint64_t seed, int player) : def_(def), state_(NewNight(def, seed, player)), decider_(Timed(def)) {}

Play::Play(const NightDef &def, const NightState &state) : def_(def), state_(state), decider_(Timed(def)) {}

bool Play::PlayerTurn() const
{
    const int acting = Current(state_);
    // A dead player only watches; their allies go back to their bots.
    return state_.player >= 0 && acting >= 0 && !state_.guests[U(state_.player)].dead && Controls(state_, state_.player, acting);
}

int Play::Me() const { return PlayerTurn() ? Current(state_) : state_.player; }

bool Play::Do(const Action &action)
{
    // 1. Apply on the player's turn only.
    if (!PlayerTurn())
        return false;
    const NightState before = state_;
    if (!Apply(def_, state_, action))
        return false;
    Notice(before.logCount);
    // 2. An event drawn on this move: shown before its roll (a new bit in eventsUsed).
    const std::uint64_t drawn = state_.eventsUsed & ~before.eventsUsed;
    if (drawn)
    {
        event_ = std::countr_zero(drawn);
        // The room moved into (the guest may have been pushed back out of it).
        const int from = before.guests[U(Current(before))].cell;
        eventCell_ = action.kind == ActionKind::Move && action.a < transportMove ? Neighbour(def_, from, action.a) : state_.guests[U(Current(before))].cell;
        eventStat_ = Stat(def_, before, Current(before), def_.events[U(event_)].stat);
    }
    // 3. Every roll the player's side made, with what came of it.
    for (const LogEntry &e : NewLog(state_, before.logCount))
    {
        if (e.kind != LogKind::Check || !Controls(state_, state_.player, e.who))
            continue;
        Dice d{e.a, e.other, e.b, "", "", e.a + e.other >= e.b};
        if (action.kind == ActionKind::Answer)
        {
            d.stat = action.a == 0 ? "Might" : "Sanity";
            d.result = d.passed ? "An approval." : "No approval this time.";
        }
        else if (drawn)
        {
            const EventDef &ev = def_.events[U(event_)];
            d.stat = StatName(ev.stat);
            d.result = d.passed ? EffectText(ev.passEffect, ev.passAmount) : EffectText(ev.failEffect, ev.failAmount);
        }
        dice_.push_back(d);
    }
    return true;
}

void Play::Click(int cell, bool right)
{
    // 1. Inspect: a right click on the board, or any click on a bot's turn; an opened cell only.
    const std::string screen = Screen();
    const bool board = screen == "menu" || screen == "picking" || screen == "" || screen == "caption";
    if (board && (right || !PlayerTurn()))
    {
        if (cell >= 0 && cell < maxCells && state_.cells[U(cell)].opened)
            inspect_ = cell;
        return;
    }
    // 2. A walk to a lit square the moves left can pay for, from the menu or while picking.
    if ((screen != "menu" && screen != "picking") || walking_ || slide_.guest >= 0)
        return;
    const int me = Current(state_);
    std::vector<int> path = PathTo(def_, state_, me, cell);
    if (path.empty() || static_cast<int>(path.size()) > state_.guests[U(me)].movesLeft)
        return;
    walk_ = std::move(path);
    walking_ = true;
    mode_ = Mode::Picking;
}

Play::Preview Play::Previewed() const
{
    if (mode_ != Mode::Picking || walking_ || !PlayerTurn())
        return {};
    const int me = Current(state_);
    Preview p{PathTo(def_, state_, me, hover_), 0};
    p.affordable = std::min<int>(static_cast<int>(p.path.size()), state_.guests[U(me)].movesLeft);
    return p;
}

bool Play::Step(int to)
{
    // The legal Move from the acting guest's cell into `to`, then its slide.
    const int me = Current(state_), from = state_.guests[U(me)].cell;
    for (const Action &a : LegalActions(def_, state_))
        if (a.kind == ActionKind::Move && a.a < transportMove && Neighbour(def_, from, a.a) == to && Do(a))
        {
            slide_ = {me, from, state_.guests[U(me)].cell, 0.0F};
            return true;
        }
    return false;
}

void Play::WalkOn()
{
    // 1. The walk ended: back to the menu.
    if (walk_.empty())
    {
        if (walking_)
            walking_ = false, mode_ = Mode::Menu;
        return;
    }
    // 2. A door to a room not yet opened: Stay or Back.
    const int next = walk_.front();
    if (!state_.cells[U(next)].opened)
    {
        mode_ = Mode::Confirm;
        return;
    }
    // 3. A plain step; anything the step wrote (a roll, a fight, an item) or no moves left ends the walk.
    const std::uint32_t logBefore = state_.logCount;
    walk_.erase(walk_.begin());
    if (!Step(next) || state_.logCount != logBefore || state_.guests[U(Current(state_))].movesLeft <= 0 || Screen() != "picking")
        walk_.clear();
}

void Play::Key(umbra::platform::InputKey key)
{
    using umbra::platform::InputKey;
    // 1. Arrows: a move north, east, south or west.
    int direction = -1;
    if (key == InputKey::Up) direction = 0;
    if (key == InputKey::Right) direction = 1;
    if (key == InputKey::Down) direction = 2;
    if (key == InputKey::Left) direction = 3;
    // 2. Each key does what a legal action of this turn allows, or nothing; never behind a dialog.
    if (key == InputKey::Space && !PlayerTurn())
        fast_ = true, waitTap_ = false;
    if (key == InputKey::I && (Screen() == "menu" || Screen() == "bag") && !state_.chat.active)
        bag_ = !bag_;
    const std::string screen = Screen();
    if (screen == "confirm" && key == InputKey::Enter)
        (void)HandleUi("confirm", "stay", "");
    // 3. Esc: close what has a way back, else the save menu (the menu, a bot's turn), else nothing.
    if (key == InputKey::Escape)
    {
        if (screen == "pause")
            pause_ = false;
        else if (screen == "inspect")
            inspect_ = -1;
        else if (screen == "bag")
            bag_ = false;
        else if (screen == "confirm")
            (void)HandleUi("confirm", "back", "");
        else if (screen == "picking")
            walk_.clear(), walking_ = slide_.guest >= 0, mode_ = Mode::Menu;
        else if (screen == "menu" || screen == "" || screen == "caption")
            pause_ = true;
        return;
    }
    if ((screen != "menu" && screen != "picking") || walking_ || slide_.guest >= 0)
        return;
    // 4. Arrows: one step that way (picking); Enter: End turn in the menu, the hovered square while picking.
    if (direction >= 0)
    {
        const int me = Current(state_), to = Neighbour(def_, state_.guests[U(me)].cell, direction);
        const std::vector<Action> legal = LegalActions(def_, state_);
        if (std::find(legal.begin(), legal.end(), Action{ActionKind::Move, static_cast<std::int8_t>(direction)}) != legal.end())
            walk_ = {to}, walking_ = true, mode_ = Mode::Picking;
        return;
    }
    if (key == InputKey::Enter && screen == "picking")
        return Click(hover_);
    for (const Action &action : LegalActions(def_, state_))
        if (key == InputKey::Enter && action.kind == ActionKind::EndTurn && Do(action))
            return;
}

bool Play::HandleUi(const std::string &, const std::string &action, const std::string &arg)
{
    // 1. Screens that only show what happened: dismissed at any time.
    if (action == "roll" && event_ >= 0)
    {
        event_ = -1;
        return true;
    }
    if (action == "continue" && !dice_.empty())
    {
        dice_.pop_front();
        return true;
    }
    if (action == "begin" && turn_)
    {
        turn_ = false;
        return true;
    }
    if (action == "focus")
        return following_ = true;
    // A tap on a waiting beat; Skip runs the bots to the player's turn.
    if (action == "next" && waitTap_)
        return !(waitTap_ = false);
    if (action == "skip" && !PlayerTurn() && state_.player >= 0 && !state_.over)
    {
        skipping_ = fast_ = true, waitTap_ = false;
        return true;
    }
    if (action == "resume" && pause_)
        return !(pause_ = false);
    if (action == "close" && inspect_ >= 0)
        return (inspect_ = -1), true;
    if (!PlayerTurn())
        return false;
    // 2. Choices: answers, battle moves.
    for (const Action &a : LegalActions(def_, state_))
        if ((action == "answer" && a.kind == ActionKind::Answer && std::to_string(a.a) == arg) ||
            (action == "strike" && a.kind == ActionKind::Strike && std::to_string(a.a) == arg) ||
            (action == "guard" && a.kind == ActionKind::Guard) || (action == "leave" && a.kind == ActionKind::Leave))
            return Do(a);
    // 2. The turn menu and the door; Look opens the room at full size.
    if (action == "move" && Screen() == "menu")
        return (mode_ = Mode::Picking), true;
    if (action == "look" && Screen() == "menu")
        return looking_ = true;
    if (action == "back" && looking_)
        return !(looking_ = false);
    if (action == "back" && mode_ == Mode::Confirm)
    {
        walk_.clear(), walking_ = false, mode_ = Mode::Picking;
        return true;
    }
    if (action == "stay" && mode_ == Mode::Confirm && !walk_.empty())
    {
        const int door = walk_.front();
        walk_.clear(), mode_ = Mode::Picking;
        return Step(door);
    }
    if (action == "bag")
        return bag_ = true;
    if (action == "close")
        return !(bag_ = false);
    // 1. "act": the index of a legal action, as UiData listed it.
    const int n = ParseInt(arg);
    const std::vector<Action> legal = LegalActions(def_, state_);
    if (action == "act")
        return n >= 0 && n < static_cast<int>(legal.size()) && Do(legal[U(n)]);
    // 2. The bag: use or drop a slot, pick up a floor item.
    const ActionKind kind = action == "use" ? ActionKind::UseItem : action == "drop" ? ActionKind::DropItem : ActionKind::PickUp;
    if (action != "use" && action != "drop" && action != "pickup")
        return false;
    for (const Action &a : legal)
        if (a.kind == kind && a.a == n)
            return Do(a);
    return false;
}

std::string Play::Screen() const
{
    if (state_.over)
        return "end";
    if (pause_)
        return "pause";
    if (inspect_ >= 0)
        return "inspect";
    if (looking_)
        return "look";
    if (slide_.guest >= 0 && PlayerTurn()) // a slide takes its time; a dialog waits for it
        return "picking";
    if (event_ >= 0)
        return "event";
    if (!dice_.empty())
        return "dice";
    if (turn_)
        return turnTicket_ != 0 ? "" : "turn"; // its line is still coming
    if (!PlayerTurn())
        return (hold_ > 0.0F || waitTap_) && beat_.seen ? "caption" : "";
    if (state_.chat.active)
        return ChatWaiting() ? "" : "chat";
    if (state_.battle.active)
        return bag_ ? "bag" : "battle";
    if (bag_)
        return "bag";
    return mode_ == Mode::Confirm ? "confirm" : mode_ == Mode::Picking ? "picking" : "menu";
}

void Play::Update(float dt)
{
    // 1. Nothing moves without time; a slide plays out first; then the player's rolls wait.
    if (dt <= 0.0F || pause_ || inspect_ >= 0 || looking_)
        return;
    clock_ += dt;
    ResolveLines();
    if (slide_.guest >= 0)
    {
        slide_.t += dt / options_.slideSeconds;
        if (slide_.t < 1.0F)
            return;
        slide_ = {};
    }
    if (event_ >= 0 || !dice_.empty() || turn_ || waitTap_)
        return;
    hold_ -= dt;
    if (hold_ > 0.0F)
        return;
    // 2. The player's turn: the beats are over, Space wears off.
    const int acting = Current(state_);
    if (acting < 0 || PlayerTurn())
    {
        beat_ = {};
        fast_ = skipping_ = moment_ = false;
        if (acting >= 0 && !state_.chat.active && !state_.battle.active && !bag_ && mode_ != Mode::Confirm)
            WalkOn();
        return;
    }
    walk_.clear(), walking_ = false, mode_ = Mode::Menu; // a bot's turn
    // 3. A bot decides; before its first action of a turn, a bot in view thinks for
    //    max(thinkSeconds, the decision's own time) (user).
    bag_ = false;
    if (!pending_ || pendingGuest_ != acting)
    {
        float took = 0.0F;
        pendingAction_ = decider_(state_, acting, took);
        pending_ = true, pendingGuest_ = acting;
        AskLines(acting);
        const int turnKey = state_.round * 8 + state_.turn;
        const auto pawns = MakeView(def_, state_, state_.player).pawns;
        const bool seen = std::any_of(pawns.begin(), pawns.end(), [&](const PawnView &p) { return p.guest == acting; });
        if (turnKey != thoughtTurn_ && seen && !state_.battle.active && !state_.chat.active)
        {
            thoughtTurn_ = turnKey;
            think_ = skipping_ ? 0.0F : std::max(options_.thinkSeconds, took);
            if (skipping_)
                return;
            return;
        }
    }
    if (think_ > 0.0F)
    {
        think_ -= dt;
        if (think_ > 0.0F)
            return;
        think_ = 0.0F;
    }
    // 4. Its lines: the model's, up to lineSeconds since they were asked (user).
    lineWaiting_ = LineWaiting(captionTicket_, askedAt_) || LineWaiting(saidTicket_, askedAt_);
    if (lineWaiting_)
        return;
    // 5. It acts; the narrator says what the player could see of it; a step slides.
    pending_ = false;
    const NightState before = state_;
    const Action action = pendingAction_;
    if (!Apply(def_, state_, action))
        return;
    if (action.kind == ActionKind::Move && before.guests[U(acting)].cell != state_.guests[U(acting)].cell)
        slide_ = {acting, before.guests[U(acting)].cell, state_.guests[U(acting)].cell, 0.0F};
    Notice(before.logCount);
    ++beats_;
    beat_ = captions_.lines.empty() ? Beat{} : Narrate(def_, before, state_, NewLog(state_, before.logCount), action, acting, state_.player, captions_);
    if (captionTicket_ != 0)
        beat_.caption = words_->Take(captionTicket_, beat_.caption), captionTicket_ = 0;
    said_ = saidTicket_ != 0 ? words_->Take(saidTicket_, "") : "", saidTicket_ = 0;
    const bool notable = beat_.seen && Notable(before, NewLog(state_, before.logCount), action, acting);
    hold_ = !beat_.seen || skipping_ ? options_.unseenBeatSeconds : fast_ ? options_.fastBeatSeconds : options_.beatSeconds;
    waitTap_ = notable && options_.tapToContinue && !skipping_ && !fast_;
    moment_ = notable && options_.moments && !skipping_;
    if (!beat_.seen)
        slide_ = {}; // only what the player sees slides
}

std::string Play::Label(const Action &action) const
{
    const auto name = [&](int guest) { return guest >= 0 ? def_.guests[U(guest)].name : std::string{"?"}; };
    const auto monster = [&](int m) { return def_.monsters[U(state_.monsters[U(m)].kind)].name; };
    switch (action.kind)
    {
    case ActionKind::Move: return "Take the passage";
    case ActionKind::EndTurn: return "End turn";
    case ActionKind::PickUp: return "Pick up the " + def_.items[U(state_.floor[U(action.a)].item)].name;
    case ActionKind::UseItem: return "Use the " + def_.items[U(state_.guests[U(Me())].items[U(action.a)])].name;
    case ActionKind::DropItem: return "Drop the " + def_.items[U(state_.guests[U(Me())].items[U(action.a)])].name;
    case ActionKind::Chat: return "Chat with " + name(action.a);
    case ActionKind::Attack: return "Attack " + (action.a >= 0 ? "the " + monster(action.a) : name(action.b));
    case ActionKind::Strike:
    {
        const Fighter &f = state_.battle.fighters[U(action.a)];
        return "Strike " + (f.monster >= 0 ? "the " + monster(f.monster) : name(f.guest));
    }
    case ActionKind::Guard: return "Guard";
    case ActionKind::Leave: return "Leave the battle";
    case ActionKind::LevelUp: return action.a == 0 ? "Level up: +1 Might" : "Level up: +1 Sanity";
    case ActionKind::Power: return action.a >= 0 && action.b < 0 ? "Use your power on " + name(action.a) : "Use your power";
    case ActionKind::OpenChat: return "Talk";
    case ActionKind::Answer: return action.a == 0 ? "Answer: physical" : "Answer: magical";
    }
    return "";
}

nlohmann::json Play::UiData() const
{
    nlohmann::json data;
    data["writtenLines"] = words_ == nullptr || !words_->HasModel(); // the save menu says so
    data["dread"] = state_.taken.present;                              // blood runs down the screen
    data["canSkip"] = !PlayerTurn() && state_.player >= 0 && !state_.over;
    if (state_.player >= 0)
        data["here"] = HereData();
    if (looking_ && state_.player >= 0)
        data["look"] = {{"room", def_.rooms[U(state_.cells[U(state_.guests[U(state_.player)].cell)].room)].name}};
    if (state_.over)
        data["end"] = EndData();
    if (turn_ && !state_.over)
        data["turn"] = TurnData();
    if (inspect_ >= 0)
        data["inspect"] = InspectData();
    data["focus"] = !following_;
    if (beat_.seen && (hold_ > 0.0F || waitTap_))
        data["caption"] = {{"guest", def_.guests[U(beat_.guest)].name}, {"line", beat_.caption}, {"result", beat_.result}, {"tap", waitTap_},
                           {"said", said_}, {"saidBy", saidGuest_ >= 0 ? def_.guests[U(saidGuest_)].name : ""}};
    const int me = Me();
    if (me < 0)
        return data;
    const Guest &guest = state_.guests[U(me)];
    // 1. The clock and the acting guest.
    char clock[8];
    std::snprintf(clock, sizeof(clock), "%d:%02d", Hour(def_, state_), Minute(def_, state_));
    data["clock"] = clock;
    data["round"] = state_.round;
    data["rounds"] = def_.rules.clock.rounds;
    data["phase"] = state_.taken.present ? "the hunt" : "evening";
    std::string buffs;
    for (int f = 0; state_.battle.active && f < state_.battle.count; ++f)
        if (state_.battle.fighters[U(f)].guest == me && !state_.battle.fighters[U(f)].taken)
            for (const std::int8_t b : state_.battle.fighters[U(f)].buff)
                if (b >= 0)
                    buffs += (buffs.empty() ? "" : ", ") + def_.buffs[U(b)].name;
    data["me"] = {{"name", def_.guests[U(me)].name}, {"portrait", ArtPath("portraits", def_.guests[U(me)].id).generic_string()}, {"hp", guest.hp}, {"maxHp", Stat(def_, state_, me, StatKind::MaxHp)},
                  {"might", Stat(def_, state_, me, StatKind::Might)}, {"sanity", Stat(def_, state_, me, StatKind::Sanity)},
                  {"speed", Stat(def_, state_, me, StatKind::Speed)}, {"level", guest.level}, {"buffs", buffs}};
    data["moves"] = guest.movesLeft;
    // The player's own goal, and the goals it has learnt in chats.
    data["knownGoals"] = nlohmann::json::array();
    if (state_.player >= 0 && state_.guests[U(state_.player)].goal >= 0)
    {
        const Guest &mine = state_.guests[U(state_.player)];
        const GoalDef &goal = def_.goals[U(mine.goal)];
        const bool counted = goal.kind == GoalKind::Events || goal.kind == GoalKind::Monsters;
        data["goal"] = {{"text", goal.text}, {"done", GoalDone(def_, state_, state_.player)},
                        {"progress", counted ? std::to_string(std::min<int>(mine.goalCount, goal.count)) + " of " + std::to_string(goal.count) : ""}};
        for (int g = 0; g < state_.count; ++g)
            if (g != state_.player && ((mine.knows >> g) & 1u) && state_.guests[U(g)].goal >= 0)
                data["knownGoals"].push_back({{"name", def_.guests[U(g)].name}, {"text", def_.goals[U(state_.guests[U(g)].goal)].text}});
    }
    data["turnTitle"] = me == state_.player ? std::string{"Your turn"} : def_.guests[U(me)].name + "'s turn";

    // 2. The turn menu: Move first, then fights, chats and the rest, Use item, End turn last.
    data["actions"] = nlohmann::json::array();
    const std::vector<Action> legal = PlayerTurn() ? LegalActions(def_, state_) : std::vector<Action>{};
    if (std::any_of(legal.begin(), legal.end(), [](const Action &a) { return a.kind == ActionKind::Move && a.a < transportMove; }))
        data["actions"].push_back({{"label", "Move"}, {"action", "move"}, {"arg", ""}});
    if (Screen() == "menu")
        data["actions"].push_back({{"label", "Look around"}, {"action", "look"}, {"arg", ""}});
    int endTurn = -1;
    for (std::size_t i = 0; i < legal.size(); ++i)
    {
        const Action &a = legal[i];
        if ((a.kind == ActionKind::Move && a.a < transportMove) || a.kind == ActionKind::UseItem || a.kind == ActionKind::DropItem)
            continue;
        if (a.kind == ActionKind::EndTurn)
            endTurn = static_cast<int>(i);
        else
            data["actions"].push_back({{"label", Label(a)}, {"action", "act"}, {"arg", std::to_string(i)}});
    }
    int used = 0;
    for (int s = 0; s < def_.rules.carry; ++s)
        used += guest.items[U(s)] >= 0 ? 1 : 0;
    if (PlayerTurn() && !state_.battle.active && !state_.chat.active)
        data["actions"].push_back({{"label", "Use item (" + std::to_string(used) + " of " + std::to_string(def_.rules.carry) + ")"}, {"action", "bag"}, {"arg", ""}});
    if (endTurn >= 0)
        data["actions"].push_back({{"label", "End turn"}, {"action", "act"}, {"arg", std::to_string(endTurn)}});

    // 3. The company: allies, guests being won over, guests in the light.
    data["allies"] = data["winning"] = data["inLight"] = nlohmann::json::array();
    for (int g = 0; g < state_.count; ++g)
    {
        const Guest &other = state_.guests[U(g)];
        if (g == state_.player || other.dead)
            continue;
        if (Controls(state_, state_.player, g))
            data["allies"].push_back({{"name", def_.guests[U(g)].name}, {"hp", other.hp}, {"maxHp", Stat(def_, state_, g, StatKind::MaxHp)}});
        else if (const int won = state_.approvals[U(g * maxGuests + state_.player)]; won > 0)
            data["winning"].push_back({{"name", def_.guests[U(g)].name}, {"approvals", won}, {"goal", def_.rules.persuade.approvals}});
    }
    for (const PawnView &pawn : MakeView(def_, state_, state_.player).pawns)
        if (pawn.guest != me)
            data["inLight"].push_back({{"name", def_.guests[U(pawn.guest)].name}, {"room", def_.rooms[U(state_.cells[U(pawn.cell)].room)].name}});

    // 4. Rolls, events and chats, each with its chance by the rules.
    if (!state_.over && event_ >= 0)
    {
        const EventDef &ev = def_.events[U(event_)];
        data["event"] = {{"room", def_.rooms[U(state_.cells[U(eventCell_)].room)].name}, {"name", ev.name}, {"text", ev.text},
                         {"picture", ArtPath("events", ev.id).generic_string()},
                         {"stat", StatName(ev.stat)}, {"statValue", eventStat_}, {"target", ev.target},
                         {"chance", ChancePercent(eventStat_, ev.target)},
                         {"pass", EffectText(ev.passEffect, ev.passAmount)}, {"fail", EffectText(ev.failEffect, ev.failAmount)}};
    }
    if (!state_.over && !dice_.empty())
    {
        const Dice &d = dice_.front();
        data["dice"] = {{"die", d.die}, {"stat", d.stat}, {"statValue", d.statValue}, {"total", d.die + d.statValue},
                        {"target", d.target}, {"passed", d.passed}, {"failed", !d.passed}, {"result", d.result},
                        {"plain", d.stat.empty()}, {"withStat", !d.stat.empty()}};
    }
    if (!state_.over && PlayerTurn() && state_.chat.active)
    {
        const int asker = state_.chat.asker;
        const QuestionDef &q = def_.questions[U(state_.chat.question)];
        const int bond = Bond(state_, asker, state_.player);
        nlohmann::json answers = nlohmann::json::array();
        for (int a = 0; a < 2; ++a)
        {
            const int stat = Stat(def_, state_, state_.player, a == 0 ? StatKind::Might : StatKind::Sanity);
            const int target = AnswerTarget(def_, state_, a);
            answers.push_back({{"label", a == 0 ? q.physicalText : q.magicalText}, {"stat", a == 0 ? "Might" : "Sanity"},
                               {"kind", a == 0 ? "physical" : "magical"}, {"statValue", stat}, {"target", target},
                               {"chance", ChancePercent(stat, target)}, {"answer", std::to_string(a)}});
        }
        const std::string name = def_.guests[U(asker)].name;
        data["chat"] = {{"name", name}, {"portrait", ArtPath("portraits", def_.guests[U(asker)].id).generic_string()}, {"question", chatFor_ == ChatKey() && !chatWords_.empty() ? chatWords_ : q.text}, {"approvals", state_.approvals[U(asker * maxGuests + state_.player)]},
                        {"goal", def_.rules.persuade.approvals}, {"answers", answers},
                        {"bondNote", bond > 0 ? name + " likes you: the targets are lower." : bond < 0 ? name + " distrusts you: the targets are higher." : ""},
                        {"likes", bond > 0}, {"distrusts", bond < 0}};
    }
    if (!state_.over && PlayerTurn() && state_.battle.active)
        data["battle"] = BattleData();

    // 5. The bag and the floor of this room.
    data["bag"] = data["floor"] = nlohmann::json::array();
    for (int s = 0; s < def_.rules.carry; ++s)
        if (const int item = guest.items[U(s)]; item >= 0)
        {
            bool canUse = false;
            for (const Action &a : legal)
                canUse = canUse || (a.kind == ActionKind::UseItem && a.a == s);
            const ItemDef &def = def_.items[U(item)];
            data["bag"].push_back({{"name", def.name}, {"kind", KindName(def.kind)}, {"text", def.text}, {"canUse", canUse}, {"cannotUse", !canUse}, {"slot", s}});
        }
    for (std::size_t f = 0; f < state_.floor.size(); ++f)
        if (state_.floor[f].cell == guest.cell)
        {
            const ItemDef &def = def_.items[U(state_.floor[f].item)];
            data["floor"].push_back({{"name", def.name}, {"text", def.text}, {"index", f}});
        }
    data["bagUsed"] = used;
    data["bagEmpty"] = used == 0;
    data["noAllies"] = data["allies"].empty();
    data["carry"] = def_.rules.carry;
    return data;
}

nlohmann::json Play::BattleData() const
{
    // 1. Each fighter: name, HP, buffs; the acting guest's side is the company.
    const Battle &b = state_.battle;
    const Fighter &acting = b.fighters[U(b.turn)];
    const auto name = [&](const Fighter &f) {
        if (f.monster >= 0)
            return def_.monsters[U(state_.monsters[U(f.monster)].kind)].name;
        return f.taken ? def_.guests[U(f.guest)].taken : def_.guests[U(f.guest)].name;
    };
    nlohmann::json company = nlohmann::json::array(), enemies = nlohmann::json::array();
    for (int i = 0; i < b.count; ++i)
    {
        const Fighter &f = b.fighters[U(i)];
        std::string buffs;
        for (std::size_t k = 0; k < f.buff.size(); ++k)
            if (f.buff[k] >= 0)
                buffs += (buffs.empty() ? "" : ", ") + def_.buffs[U(f.buff[k])].name;
        int hp = 0, maxHp = 0;
        std::string note;
        if (f.monster >= 0)
        {
            const Monster &m = state_.monsters[U(f.monster)];
            const MonsterDef &md = def_.monsters[U(m.kind)];
            hp = m.hp, maxHp = std::max<int>(m.hp, md.hp);
            if (md.taken >= 0)
                note = "A minion: killing it takes " + std::to_string(md.hpShare) + "% of the Taken's HP.";
        }
        else
        {
            hp = state_.guests[U(f.guest)].hp, maxHp = Stat(def_, state_, f.guest, StatKind::MaxHp);
            if (f.taken)
                note = "The Taken. Its power: " + def_.powers[U(def_.guests[U(f.guest)].power)].id + ".";
        }
        if (hp <= 0 && f.monster >= 0)
            continue; // a destroyed monster
        nlohmann::json row = {{"name", name(f)}, {"hp", hp}, {"maxHp", std::max(maxHp, 1)}, {"buffs", buffs}, {"index", i}, {"note", note}};
        if (f.side == acting.side)
            row["acting"] = i == b.turn, company.push_back(row);
        else
            row["strike"] = Damage(def_, state_, acting, f), enemies.push_back(row);
    }
    // 2. The last blow, from the log.
    std::string last;
    const std::vector<LogEntry> recent = NewLog(state_, state_.logCount > 24 ? state_.logCount - 24 : 0);
    for (const LogEntry &e : recent)
        if (e.kind == LogKind::Hit)
        {
            const auto who = [&](int g, bool striker) {
                if (g >= 0)
                    return def_.guests[U(g)].name;
                for (int i = 0; i < b.count; ++i) // a monster: the first one standing in this battle
                    if (b.fighters[U(i)].monster >= 0)
                        return def_.monsters[U(state_.monsters[U(b.fighters[U(i)].monster)].kind)].name;
                return std::string{striker ? "Something" : "something"};
            };
            last = who(e.who, true) + " hits " + who(e.other, false) + " for " + std::to_string(e.a) + ".";
        }
    bool canLeave = false;
    for (const Action &a : LegalActions(def_, state_))
        canLeave = canLeave || a.kind == ActionKind::Leave;
    return {{"room", def_.rooms[U(state_.cells[U(b.cell)].room)].name}, {"round", b.round}, {"rounds", def_.rules.battle.rounds},
            {"company", company}, {"enemies", enemies}, {"canLeave", canLeave}, {"cannotLeave", !canLeave}, {"last", last}};
}

void Play::Notice(std::uint32_t since)
{
    for (const LogEntry &e : NewLog(state_, since))
        if (e.kind == LogKind::Turned)
            turn_ = true, AskTurn();
}

std::uint32_t Play::LineSeed(int kind) const { return static_cast<std::uint32_t>(state_.seed + state_.logCount * 4U + static_cast<std::uint32_t>(kind)); }

bool Play::LineWaiting(int ticket, float askedAt) const
{
    return ticket != 0 && !skipping_ && !words_->Ready(ticket) && clock_ - askedAt < options_.lineSeconds;
}

int Play::ChatKey() const { return ((state_.round * 8 + state_.turn) * 100 + state_.chat.question) * 10 + state_.chat.asker; }

bool Play::ChatWaiting() const
{
    // With a model, the chat screen waits until its question's line is resolved.
    return words_ != nullptr && words_->HasModel() && (chatFor_ != ChatKey() || chatTicket_ != 0);
}

void Play::AskLines(int acting)
{
    // 1. A decision never acted on leaves its tickets: forget them.
    for (int *ticket : {&captionTicket_, &saidTicket_})
        if (*ticket != 0)
            words_->Take(*ticket, ""), *ticket = 0;
    saidGuest_ = -1, lineWaiting_ = false;
    if (words_ == nullptr || !words_->HasModel() || captions_.lines.empty())
        return;
    // 2. The beat this action will make, on a copy: only one the player will see is worded.
    NightState preview = state_;
    if (!Apply(def_, preview, pendingAction_))
        return;
    const std::vector<LogEntry> entries = NewLog(preview, state_.logCount);
    const Beat beat = Narrate(def_, state_, preview, entries, pendingAction_, acting, state_.player, captions_);
    if (!beat.seen)
        return;
    askedAt_ = clock_;
    // 3. Its caption, as the Taken's taunt when the Taken acts.
    const bool taken = IsTaken(state_, acting);
    if (!beat.caption.empty())
        captionTicket_ = words_->Ask({taken ? LineKind::Taken : LineKind::Caption, acting, taken, Named(def_, beat.caption), beat.caption, LineSeed(0)});
    // 4. A goal settled in it: that guest says so (the player's own first).
    const LogEntry *goal = nullptr;
    for (const LogEntry &e : entries)
        if (e.kind == LogKind::Goal && (goal == nullptr || e.who == state_.player))
            goal = &e;
    if (goal != nullptr)
    {
        saidGuest_ = goal->who;
        const std::string line = goal->a ? "I did what I came here to do tonight." : "I could not do what I came here to do tonight.";
        saidTicket_ = words_->Ask({goal->a ? LineKind::GoalDone : LineKind::GoalFailed, goal->who, false, {}, line, LineSeed(3)});
    }
}

void Play::AskTurn()
{
    turnWords_.clear();
    if (words_ == nullptr || !words_->HasModel() || state_.taken.guest < 0)
        return;
    const int t = state_.taken.guest;
    const GuestDef &g = def_.guests[U(t)];
    turnTicket_ = words_->Ask({LineKind::Turn, t, false, {t}, g.name + " becomes " + g.taken + ".", LineSeed(2)});
    turnAskedAt_ = clock_;
}

void Play::ResolveLines()
{
    if (words_ == nullptr)
        return;
    // 1. The Turn's words: in, or out of time.
    if (turnTicket_ != 0 && !LineWaiting(turnTicket_, turnAskedAt_))
        turnWords_ = words_->Take(turnTicket_, ""), turnTicket_ = 0;
    // 2. A chat question to the player: asked once, then in or out of time.
    if (!words_->HasModel() || !state_.chat.active || !PlayerTurn())
        return;
    if (chatFor_ != ChatKey())
    {
        const QuestionDef &q = def_.questions[U(state_.chat.question)];
        chatFor_ = ChatKey(), chatWords_.clear(), chatAskedAt_ = clock_;
        chatTicket_ = words_->Ask({LineKind::Question, state_.chat.asker, false, Named(def_, q.text), q.text, LineSeed(1)});
    }
    if (chatTicket_ != 0 && !LineWaiting(chatTicket_, chatAskedAt_))
        chatWords_ = words_->Take(chatTicket_, ""), chatTicket_ = 0;
}

nlohmann::json Play::Save() const { return SaveNight(def_, state_); }

bool Play::Restore(const NightDef &def, const nlohmann::json &saved, NightState &out)
{
    std::vector<umbra::core::Diagnostic> diagnostics;
    try
    {
        return saved.is_object() && LoadNightState(def, saved, out, diagnostics);
    }
    catch (const nlohmann::json::exception &)
    {
        return false; // a damaged save
    }
}

nlohmann::json Play::StartData(const NightDef &def, bool canContinue)
{
    nlohmann::json guests = nlohmann::json::array();
    for (const GuestDef &g : def.guests)
        guests.push_back({{"id", g.id}, {"name", g.name}, {"style", def.styles[U(g.style)].id}, {"hp", g.hp},
                          {"might", g.might}, {"sanity", g.sanity}, {"speed", g.speed}});
    return {{"start", {{"guests", guests}, {"canContinue", canContinue}}}};
}

nlohmann::json Play::TurnData() const
{
    // The curse takes a guest: who, as what, how strong, and the minions whose deaths weaken it.
    const int t = state_.taken.guest;
    const GuestDef &g = def_.guests[U(t)];
    int minions = 0, share = 0;
    std::string minionName;
    for (const Monster &m : state_.monsters)
        if (def_.monsters[U(m.kind)].taken == t)
            ++minions, minionName = def_.monsters[U(m.kind)].name, share = def_.monsters[U(m.kind)].hpShare;
    char clock[8];
    std::snprintf(clock, sizeof(clock), "%d:%02d", Hour(def_, state_), Minute(def_, state_));
    const PowerDef &power = def_.powers[U(g.power)];
    return {{"who", g.name}, {"isYou", t == state_.player}, {"form", g.taken}, {"hp", state_.guests[U(t)].hp}, {"words", turnWords_},
            {"power", power.id + ": " + PowerText(power)}, {"minions", minions}, {"minionName", minionName},
            {"minionShare", share}, {"clock", clock}};
}

nlohmann::json Play::EndData() const
{
    // 1. Who won.
    std::string outcome = "The night ends.";
    if (state_.outcome == Outcome::Company)
        outcome = "Dawn comes. The company survives the night.";
    if (state_.outcome == Outcome::Taken)
        outcome = "The Taken wins. The theatre keeps its guests.";
    // 2. A few lines from the night.
    int rooms = 0;
    for (const Cell &c : state_.cells)
        rooms += c.opened ? 1 : 0;
    nlohmann::json lines = nlohmann::json::array();
    lines.push_back("Rounds played: " + std::to_string(std::min<int>(state_.round, def_.rules.clock.rounds)));
    lines.push_back("Rooms opened: " + std::to_string(rooms));
    if (state_.taken.guest >= 0)
        lines.push_back("The curse took " + def_.guests[U(state_.taken.guest)].name + ", who rose as " + def_.guests[U(state_.taken.guest)].taken + ".");
    std::string fallen, allies;
    for (int g = 0; g < state_.count; ++g)
    {
        if (state_.guests[U(g)].dead)
            fallen += (fallen.empty() ? "" : ", ") + def_.guests[U(g)].name;
        else if (g != state_.player && state_.player >= 0 && Controls(state_, state_.player, g))
            allies += (allies.empty() ? "" : ", ") + def_.guests[U(g)].name;
    }
    if (!fallen.empty())
        lines.push_back("Fallen: " + fallen + ".");
    if (!allies.empty())
        lines.push_back("At your side at the end: " + allies + ".");
    return {{"outcome", outcome}, {"lines", lines}};
}

std::vector<std::pair<int, int>> Play::Rings() const
{
    // 1. Count each group's living members.
    std::array<int, 16> size{};
    for (int g = 0; g < state_.count; ++g)
        if (const Guest &guest = state_.guests[U(g)]; guest.group >= 0 && guest.group < 16 && !guest.dead)
            ++size[U(guest.group)];
    // 2. Number the groups as their first member comes up; the player's group is 1.
    const int mine = state_.player >= 0 ? state_.guests[U(state_.player)].group : -1;
    std::array<int, 16> number{};
    int next = 2;
    std::vector<std::pair<int, int>> rings;
    for (int g = 0; g < state_.count; ++g)
    {
        const Guest &guest = state_.guests[U(g)];
        if (guest.group < 0 || guest.group >= 16 || guest.dead || size[U(guest.group)] < 2)
            continue;
        int &n = number[U(guest.group)];
        if (n == 0)
            n = guest.group == mine ? 1 : next++;
        rings.push_back({g, n});
    }
    return rings;
}

int Play::CameraCell() const
{
    // A step sliding, a bot's beat or thought the player sees, else the acting guest on the player's turn.
    if (slide_.guest >= 0)
        return slide_.to;
    if (beat_.seen && hold_ > 0.0F && beat_.camera >= 0)
        return beat_.camera;
    if (ThinkingGuest() >= 0)
        return state_.guests[U(ThinkingGuest())].cell;
    return PlayerTurn() ? state_.guests[U(Current(state_))].cell : -1;
}

nlohmann::json Play::InspectData() const
{
    // 1. The room: its name and kind always; who and what is in it only when lit.
    const Cell &cell = state_.cells[U(inspect_)];
    const RoomDef &room = def_.rooms[U(cell.room)];
    const BoardView view = MakeView(def_, state_, state_.player);
    const bool lit = view.cells[U(inspect_)].lit;
    static const char *kinds[] = {"foyer", "under-stage", "plain room", "item room", "event room", "trap", "passage room", "bonus room"};
    nlohmann::json data = {{"room", {{"name", room.name}, {"kind", kinds[static_cast<int>(room.kind)]}, {"dark", !lit},
                                     {"waiting", !cell.resolved && (room.kind == RoomKind::Item || room.kind == RoomKind::Event || room.kind == RoomKind::Trap)}}},
                           {"guests", nlohmann::json::array()}, {"items", nlohmann::json::array()}, {"monsters", nlohmann::json::array()}};
    if (!lit)
        return data;
    // 2. Guests, with their goal when the player knows it; floor items; monsters.
    const Guest &mine = state_.guests[U(std::max<int>(state_.player, 0))];
    for (const PawnView &pawn : view.pawns)
    {
        if (pawn.cell != inspect_)
            continue;
        const int g = pawn.guest;
        const Guest &guest = state_.guests[U(g)];
        nlohmann::json bag = nlohmann::json::array();
        for (const std::int8_t item : guest.items)
            if (item >= 0)
                bag.push_back(def_.items[U(item)].name);
        const bool known = guest.goal >= 0 && state_.player >= 0 && (g == state_.player || ((mine.knows >> g) & 1u));
        data["guests"].push_back({{"name", def_.guests[U(g)].name}, {"hp", guest.hp}, {"maxHp", Stat(def_, state_, g, StatKind::MaxHp)},
                                  {"might", Stat(def_, state_, g, StatKind::Might)}, {"sanity", Stat(def_, state_, g, StatKind::Sanity)},
                                  {"speed", Stat(def_, state_, g, StatKind::Speed)}, {"level", guest.level}, {"group", guest.group},
                                  {"bag", bag}, {"goal", known ? def_.goals[U(guest.goal)].text : ""}});
    }
    for (const FloorItem &f : state_.floor)
        if (f.cell == inspect_)
            data["items"].push_back(def_.items[U(f.item)].name);
    for (const MonsterView &m : view.monsters)
        if (m.cell == inspect_)
            data["monsters"].push_back(def_.monsters[U(state_.monsters[U(m.monster)].kind)].name);
    return data;
}

int Play::StageCell() const
{
    // 1. Look: the player's own room.
    if (looking_ && state_.player >= 0)
        return state_.guests[U(state_.player)].cell;
    // 2. Something that happened in the player's sight (options "moments"): their event, a bot's beat.
    if (options_.moments && (event_ >= 0 || !dice_.empty()) && state_.player >= 0)
        return eventCell_ >= 0 ? eventCell_ : state_.guests[U(state_.player)].cell;
    if (moment_ && beat_.seen && beat_.camera >= 0 && (waitTap_ || hold_ > 0.0F))
        return beat_.camera;
    // 3. A battle the player can see (their own, or bots' in their light).
    if (state_.battle.active && state_.battle.cell >= 0 && MakeView(def_, state_, state_.player).cells[U(state_.battle.cell)].lit)
        return state_.battle.cell;
    return -1;
}

std::vector<Play::Actor> Play::StageActors() const
{
    std::vector<Actor> actors;
    const int cell = StageCell();
    if (cell < 0)
        return actors;
    // 1. A battle: its fighters, by side.
    if (state_.battle.active && cell == state_.battle.cell)
    {
        for (int f = 0; f < state_.battle.count; ++f)
        {
            const Fighter &fighter = state_.battle.fighters[U(f)];
            const int kind = fighter.monster >= 0 ? state_.monsters[U(fighter.monster)].kind : -1;
            actors.push_back({fighter.guest, kind, fighter.side, fighter.taken});
        }
        return actors;
    }
    // 2. Look: the guests in the room, then its monsters.
    for (int g = 0; g < state_.count; ++g)
        if (state_.guests[U(g)].cell == cell && !state_.guests[U(g)].dead)
            actors.push_back({g, -1, 0, IsTaken(state_, g)});
    for (const Monster &m : state_.monsters)
        if (m.cell == cell && m.hp > 0)
            actors.push_back({-1, m.kind, 1, false});
    return actors;
}

bool Play::LookDoor(int side)
{
    // The board's Move through that side, if it is legal; the look ends with it.
    if (!looking_ || state_.player < 0)
        return false;
    const int to = Neighbour(def_, state_.guests[U(state_.player)].cell, side);
    looking_ = false;
    if (to >= 0 && Step(to))
        return true;
    looking_ = true;
    return false;
}

bool Play::Notable(const NightState &before, const std::vector<LogEntry> &entries, const Action &action, int actor) const
{
    // Something happened: an item, a roll, a fight, a fall, a chat, a clearing, a goal, the Turn.
    for (const LogEntry &e : entries)
        switch (e.kind)
        {
        case LogKind::Item: case LogKind::Check: case LogKind::Battle: case LogKind::Hit: case LogKind::Down: case LogKind::Died:
        case LogKind::Persuade: case LogKind::Cleared: case LogKind::Goal: case LogKind::Turned: case LogKind::Power: case LogKind::Monster:
            return true;
        default:
            break;
        }
    if (action.kind == ActionKind::OpenChat)
        return true;
    // ... or a guest walked into a featured room.
    const int to = state_.guests[U(actor)].cell;
    return to != before.guests[U(actor)].cell && Featured(def_, state_, to);
}

nlohmann::json Play::HereData() const
{
    // The player's room: its name, what waits there (and how to clear it), what lies there, who is there.
    const int cell = state_.guests[U(state_.player)].cell;
    const Cell &c = state_.cells[U(cell)];
    const RoomDef &room = def_.rooms[U(c.room)];
    std::string what;
    if (c.event >= 0)
    {
        const EventDef &e = def_.events[U(c.event)];
        what = e.name + (e.trap ? " (a trap)" : "") + " waits here. Clear it: pass a " + StatName(e.stat) + " roll";
        if (e.clear == ClearKind::Stat)
            what += ", or walk in with " + std::string(StatName(e.clearStat)) + " " + std::to_string(e.clearAmount) + "+";
        else if (e.clear == ClearKind::Carry)
            what += ", or carry the " + def_.items[U(e.clearItem)].name;
        what += ".";
    }
    else if (!c.resolved && room.kind == RoomKind::Trap)
        what = "A trap waits here.";
    else if (!c.resolved && room.kind == RoomKind::Event)
        what = "Something waits here.";
    else if (!c.resolved && room.kind == RoomKind::Item)
        what = "Something lies hidden here.";
    else if (room.kind == RoomKind::Bonus)
        what = "Once for each guest: +" + std::to_string(room.amount) + " " + StatName(room.stat) + ".";
    else
        what = "Quiet now.";
    std::string items, people;
    for (const FloorItem &f : state_.floor)
        if (f.cell == cell && f.item >= 0)
            items += (items.empty() ? "" : ", ") + def_.items[U(f.item)].name;
    for (int g = 0; g < state_.count; ++g)
        if (g != state_.player && state_.guests[U(g)].cell == cell && !state_.guests[U(g)].dead)
            people += (people.empty() ? "" : ", ") + def_.guests[U(g)].name;
    return {{"name", room.name}, {"what", what}, {"items", items}, {"people", people}};
}
