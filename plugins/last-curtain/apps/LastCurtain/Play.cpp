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

Play::Play(const NightDef &def, std::uint64_t seed, int player) : def_(def), state_(NewNight(def, seed, player)), decider_(Timed(def)) { opening_ = true; }

Play::Play(const NightDef &def, const NightState &state) : def_(def), state_(state), decider_(Timed(def)) {}

bool Play::PlayerTurn() const
{
    const int acting = Current(state_);
    // A dead player plays on as a ghost of the house (Night.md "Ghosts").
    return state_.player >= 0 && acting >= 0 && InPlay(state_, state_.player) && Controls(state_, state_.player, acting);
}

BoardView Play::View() const { return MakeView(def_, state_, state_.player, options_.light); }

void Play::SetCutscenes(nlohmann::json scenes)
{
    scenes_ = std::move(scenes);
    if (std::exchange(opening_, false) && !state_.over)
        StartScene("opening");
}

void Play::StartScene(const std::string &kind)
{
    // A scene with shots plays now, or after the one playing.
    if (!scenes_.contains(kind) || !scenes_[kind].is_array() || scenes_[kind].empty())
        return;
    if (scene_.empty())
        scene_ = kind, shot_ = 0, shotT_ = 0.0F;
    else
        queued_.push_back(kind);
}

void Play::NextScene()
{
    scene_.clear();
    if (!queued_.empty())
    {
        const std::string next = queued_.front();
        queued_.erase(queued_.begin());
        StartScene(next);
    }
}

Play::CameraShot Play::Shot() const
{
    if (scene_.empty() || Screen() != "cutscene")
        return {};
    const nlohmann::json &s = scenes_[scene_][U(shot_)];
    // 1. Where: the board's middle (the opened room nearest it), the foyer, the player, the Taken, where it fell.
    const std::string at = s.value("at", "board");
    const int taken = state_.taken.guest >= 0 ? state_.taken.guest : state_.marked;
    CameraShot shot;
    shot.index = shot_;
    shot.clip = s.value("clip", "none") == "none" ? "" : s.value("clip", "");
    if (at == "foyer")
        shot.cell = state_.foyer, shot.clipGuest = state_.player;
    else if (at == "killer")
        shot.cell = state_.killer.cell;
    else if (at == "player" && state_.player >= 0)
        shot.cell = state_.guests[U(state_.player)].cell, shot.clipGuest = state_.player;
    else if ((at == "taken" || at == "fall") && taken >= 0)
        shot.cell = at == "fall" && fall_.cell >= 0 ? fall_.cell : state_.guests[U(taken)].cell, shot.clipGuest = at == "fall" ? fall_.killer : taken;
    if (shot.cell < 0)
    {
        float x = 0.0F, y = 0.0F, n = 0.0F;
        for (int c = 0; c < maxCells; ++c)
            if (state_.cells[U(c)].opened)
                x += static_cast<float>(c % def_.rules.board.width), y += static_cast<float>(c / def_.rules.board.width), n += 1.0F;
        float best = 1.0e9F;
        for (int c = 0; c < maxCells; ++c)
            if (const float dx = static_cast<float>(c % def_.rules.board.width) - x / n, dy = static_cast<float>(c / def_.rules.board.width) - y / n;
                state_.cells[U(c)].opened && dx * dx + dy * dy < best)
                best = dx * dx + dy * dy, shot.cell = c;
    }
    // 2. How: distance and turn eased (smoothstep) from the shot's start to its end.
    const float seconds = s.value("seconds", 4.0F), t = std::clamp(shotT_ / seconds, 0.0F, 1.0F), e = t * t * (3.0F - 2.0F * t);
    const auto ease = [&](const char *key, float fallback) {
        const nlohmann::json pair = s.value(key, nlohmann::json::array({fallback, fallback}));
        return pair[0].get<float>() + (pair[1].get<float>() - pair[0].get<float>()) * e;
    };
    shot.distance = ease("distance", 14.0F), shot.yaw = ease("yaw", 0.0F), shot.tilt = s.value("tilt", 50.0F);
    return shot;
}

std::string Play::SceneLine() const
{
    // The shot's line with its names filled in.
    std::string line = scenes_[scene_][U(shot_)].value("line", "");
    std::string survivors;
    for (int g = 0; g < state_.count; ++g)
        if (Living(state_, g))
            survivors += (survivors.empty() ? "" : ", ") + def_.guests[U(g)].name;
    const std::pair<const char *, std::string> names[] = {
        {"{player}", state_.player >= 0 ? def_.guests[U(state_.player)].name : "the company"},
        {"{night}", std::to_string(state_.night)},
        {"{survivors}", survivors.empty() ? "No one" : survivors}};
    for (const auto &[key, value] : names)
        for (std::size_t at = line.find(key); at != std::string::npos; at = line.find(key, at + value.size()))
            line.replace(at, std::string(key).size(), value);
    return line;
}

Play::Banner Play::TurnBanner() const
{
    if (banner_ <= 0.0F || options_.turnBannerSeconds <= 0.0F || Acting() < 0)
        return {};
    return {Acting(), 1.0F - banner_ / options_.turnBannerSeconds};
}

int Play::Acting() const { return state_.over ? -1 : resting_ ? rester_ : Current(state_); }

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
    Follow();
    // 1b. A room resolved on a move ends the turn in the rules; the player may still look around it.
    resting_ = action.kind == ActionKind::Move && !PlayerTurn() && !state_.over && !state_.battle.active;
    if (resting_)
        walk_.clear(), walking_ = false, mode_ = Mode::Menu, rester_ = Current(before); // the rest of a walk is dropped
    // 2. What the room did, shown step by step: the event drawn (a new bit in eventsUsed) or the lasting
    //    one armed there; a find; a bonus. The room is the one moved into (a push may have sent the guest back).
    const int g = Current(before);
    const std::vector<LogEntry> entries = NewLog(state_, before.logCount);
    const auto logged = [&](LogKind kind) {
        return std::find_if(entries.begin(), entries.end(), [&](const LogEntry &e) { return e.kind == kind && e.who == g; });
    };
    int entered = -1;
    event_ = RoomEvent(before, state_, action, g, entered);
    const bool drawn = (state_.eventsUsed & ~before.eventsUsed) != 0;
    const bool rolled = logged(LogKind::Check) != entries.end(), cleared = logged(LogKind::Cleared) != entries.end();
    lasting_ = cleared_ = "";
    if (event_ >= 0)
    {
        const EventDef &ev = def_.events[U(event_)];
        eventCell_ = entered >= 0 ? entered : state_.guests[U(g)].cell;
        eventStat_ = Stat(def_, before, g, ev.stat);
        if (!drawn)
            lasting_ = "It is still here, and stays until someone passes its roll.";
        if (cleared && !rolled)
            cleared_ = ev.clear == ClearKind::Carry ? "You carry the " + def_.items[U(ev.clearItem)].name + ": it cannot touch you. " + ev.name + " is gone for everyone."
                                                    : "Your " + std::string(StatName(ev.clearStat)) + " " + std::to_string(Stat(def_, before, g, ev.clearStat)) +
                                                          " carries you through. " + ev.name + " is gone for everyone.";
    }
    found_ = entered >= 0 && logged(LogKind::Item) != entries.end() && state_.cells[U(entered)].room >= 0 && def_.rooms[U(state_.cells[U(entered)].room)].kind == RoomKind::Item ? logged(LogKind::Item)->a : -1;
    const bool bonusAhead = entered >= 0 && state_.cells[U(entered)].room >= 0 && def_.rooms[U(state_.cells[U(entered)].room)].kind == RoomKind::Bonus &&
                            !before.guests[U(g)].bonusUsed[U(entered)];
    if (entered >= 0 && (event_ >= 0 || found_ >= 0 || bonusAhead || logged(LogKind::Effect) != entries.end()))
    {
        const RoomDef &room = def_.rooms[U(state_.cells[U(entered)].room)];
        const char *kinds[] = {"ROOM", "EVENT ROOM", "TRAP ROOM", "ITEM ROOM", "BONUS ROOM"};
        const int k = room.kind == RoomKind::Event ? 1 : room.kind == RoomKind::Trap ? 2 : room.kind == RoomKind::Item ? 3 : room.kind == RoomKind::Bonus ? 4 : 0;
        const std::string line = event_ >= 0 ? "Something is waiting for you here." : found_ >= 0 ? "Something has been left here."
                                 : room.kind == RoomKind::Bonus ? "+" + std::to_string(room.amount) + " " + StatName(room.stat) + " when your turn ends here (once for each guest)."
                                                                : "The room stirs.";
        arrive_ = {entered, kinds[k], line};
    }
    // 2b. A bonus paid as the turn ended (a new bit in bonusUsed): said plainly.
    if ((state_.guests[U(g)].bonusUsed & ~before.guests[U(g)].bonusUsed).any())
    {
        const int cell = state_.guests[U(g)].cell;
        const RoomDef &room = def_.rooms[U(state_.cells[U(cell)].room)];
        arrive_ = {cell, "BONUS ROOM", "+" + std::to_string(room.amount) + " " + StatName(room.stat) + ". Your " + StatName(room.stat) + " is now " +
                                           std::to_string(Stat(def_, state_, g, room.stat)) + "."};
    }
    // 3. Every roll the player's side made, with what came of it.
    for (const LogEntry &e : entries)
    {
        if (e.kind != LogKind::Check || !Controls(state_, state_.player, e.who))
            continue;
        Dice d{e.a, e.other, e.b, "", "", e.a + e.other >= e.b, 0, "", "", 0, ""};
        if (action.kind == ActionKind::Answer)
        {
            d.stat = action.a == 0 ? "Might" : "Sanity";
            d.result = d.passed ? "An approval." : "No approval this time.";
            // The asker answers back (captions.json reply lines, one by the seed; the model rewords it).
            const int asker = before.chat.asker;
            const auto lines = captions_.lines.find(d.passed ? "replyYes" : "replyNo");
            if (asker >= 0 && lines != captions_.lines.end())
            {
                std::vector<std::string> options;
                for (std::size_t at = 0, bar; at <= lines->second.size(); at = bar + 1)
                    bar = std::min(lines->second.find('|', at), lines->second.size()), options.push_back(lines->second.substr(at, bar - at));
                d.reply = options[LineSeed(4) % options.size()];
                d.replyBy = ShownName(def_, state_, asker);
                if (words_ != nullptr && words_->HasModel())
                    d.replyTicket = words_->Ask({LineKind::Reply, asker, IsTaken(state_, asker), {state_.player}, d.reply, LineSeed(4)});
            }
        }
        else if (event_ >= 0)
        {
            const EventDef &ev = def_.events[U(event_)];
            d.stat = StatName(ev.stat);
            d.result = RollResult(ev, d.passed, e.who);
            d.hp = (d.passed ? ev.passEffect : ev.failEffect) == Effect::Hp ? (d.passed ? ev.passAmount : ev.failAmount) : 0;
        }
        dice_.push_back(d);
    }
    diceClock_ = 0.0F;
    return true;
}

void Play::Click(int cell, bool right)
{
    // 1. Inspect: a right click on the board only (the user); an opened cell only. A left click on a bot's turn does nothing.
    const std::string screen = Screen();
    const bool board = screen == "menu" || screen == "picking" || screen == "" || screen == "caption";
    if (board && right)
    {
        if (cell >= 0 && cell < maxCells && state_.cells[U(cell)].opened)
            inspect_ = cell;
        return;
    }
    if (!PlayerTurn())
        return;
    // 2. A walk to a lit square the moves left can pay for, from the menu or while picking.
    if ((screen != "menu" && screen != "picking") || walking_ || slide_.guest >= 0)
        return;
    const int me = Current(state_);
    std::vector<int> path = PathTo(def_, state_, me, cell);
    if (path.empty() || Affordable(me, path) < static_cast<int>(path.size()))
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
    p.affordable = Affordable(me, p.path);
    return p;
}

int Play::Affordable(int guest, const std::vector<int> &path) const
{
    // Steps along the path the moves can pay for: an opened room within the budget of the way back to
    // the origin, a new room one more than the room before it (Night.md "Moves").
    const Guest &g = state_.guests[U(guest)];
    int n = 0, last = g.cell;
    for (const int cell : path)
    {
        const int cost = state_.cells[U(cell)].opened ? Distance(def_, state_, g.origin, cell) : Distance(def_, state_, g.origin, last) + 1;
        if (cost < 0 || cost > g.budget || g.steps + n >= def_.rules.moves.stepsPerSpeed * Stat(def_, state_, guest, StatKind::Speed))
            break;
        ++n, last = cell;
    }
    return n;
}

bool Play::Step(int to)
{
    // The legal Move from the acting guest's cell into `to`, then its slide.
    const int me = Current(state_), from = state_.guests[U(me)].cell;
    for (const Action &a : LegalActions(def_, state_))
        if (a.kind == ActionKind::Move && a.a < transportMove && Neighbour(def_, from, a.a) == to && Do(a))
        {
            slide_ = {me, from, state_.guests[U(me)].cell, 0.0F};
            cues_.push_back("step");
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
    // 1b. A scene: Space shows the next shot, Esc skips it.
    if (Screen() == "cutscene" && (key == InputKey::Space || key == InputKey::Escape))
    {
        (void)HandleUi("cutscene", key == InputKey::Escape ? "skip" : "next", "");
        return;
    }
    // 2. Each key does what a legal action of this turn allows, or nothing; never behind a dialog.
    if (key == InputKey::Space && waitTap_)
        (void)HandleUi("caption", "next", "");
    else if (key == InputKey::Space && !PlayerTurn())
        fast_ = true;
    if (key == InputKey::Enter && Screen() == "rest")
        resting_ = false;
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

bool Play::HandleUi(const std::string &screen, const std::string &action, const std::string &arg)
{
    // A tap that goes on (a dialog's box or its button) counts once per tapGap: spam clicks skip nothing.
    const bool tap = action == "next" || action == "onward" || action == "continue" || action == "roll" || action == "begin" ||
                     action == "take" || action == "leave";
    if (tap && tapClock_ < options_.tapGap)
        return false;
    const bool done = Handle(screen, action, arg);
    if (tap && done)
        tapClock_ = 0.0F;
    if (done && Screen() == "dice" && (action == "roll" || action == "onward")) // the die is cast as its screen opens
        cues_.push_back("dice");
    return done;
}

bool Play::Handle(const std::string &, const std::string &action, const std::string &arg)
{
    if (Screen() == "cutscene" && (action == "next" || action == "skip")) // a click: the next shot; Skip: the scene's end
    {
        if (action == "skip" || ++shot_ >= static_cast<int>(scenes_[scene_].size()))
            NextScene();
        shotT_ = 0.0F;
        return true;
    }
    // 1. Screens that only show what happened: dismissed at any time.
    if (action == "onward" && arrive_.cell >= 0)
        return (arrive_ = {}), true;
    if (action == "roll" && event_ >= 0)
    {
        event_ = -1, diceClock_ = 0.0F;
        return true;
    }
    if (action == "take" && found_ >= 0)
        return (found_ = -1), true;
    if (action == "leave" && found_ >= 0 && state_.player >= 0)
    {
        // The found item back on the floor (the last slot holding it: the one just filled).
        const auto &items = state_.guests[U(state_.player)].items;
        for (int s = maxCarry - 1; s >= 0; --s)
            if (items[U(s)] == found_)
                return LeaveItem(state_, state_.player, s) && (found_ = -1, true);
        return false;
    }
    if (action == "done" && mode_ == Mode::Picking && !walking_) // the walk picker's Done
        return (mode_ = Mode::Menu), true;
    if (action == "continue" && !dice_.empty() && diceClock_ >= options_.diceSeconds + DiceSettle())
    {
        // The HUD shows an HP change for a moment.
        const int hp = dice_.front().hp;
        flash_ = hp == 0 ? "" : (hp > 0 ? "+" : "") + std::to_string(hp) + " HP";
        flashClock_ = hp == 0 ? 0.0F : 2.0F;
        dice_.pop_front(), diceClock_ = 0.0F;
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
    {
        // A preview gives way to the bot's roll (if any) and the result, which waits again.
        if (preview_)
        {
            preview_ = false, beat_.result = heldResult_;
            if (botDice_)
                dice_.push_back(*botDice_), diceClock_ = 0.0F, botDice_.reset(), cues_.push_back("dice");
            return true;
        }
        return !(waitTap_ = false);
    }
    if (action == "skip" && !PlayerTurn() && state_.player >= 0 && !state_.over)
    {
        skipping_ = fast_ = true, waitTap_ = false;
        return true;
    }
    if (action == "resume" && pause_)
        return !(pause_ = false);
    if (action == "close" && inspect_ >= 0)
        return (inspect_ = -1), true;
    if (action == "continue" && Screen() == "fall") // the fall seen: the night goes on
        return fallSeen_ = true;
    if (action == "goals") // the goal panel folds and opens
        return goalOpen_ = !goalOpen_, true;
    if (action == "look") // the view switch: the player's own, on their turn or its rest (the user)
        return CanView() && (roomView_ = !roomView_, true);
    if (resting_ && Screen() == "rest" && action == "end")
        return !(resting_ = false);
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
    if (fall_.cell >= 0 && !fallSeen_)
        return "fall";
    if (!scene_.empty())
        return "cutscene";
    if (state_.over)
        return "end";
    if (pause_)
        return "pause";
    if (inspect_ >= 0)
        return "inspect";
    if (slide_.guest >= 0 && PlayerTurn() && Controls(state_, state_.player, slide_.guest)) // the player's own slide; a dialog waits for it
        return "picking";
    if (arrive_.cell >= 0)
        return "arrive";
    if (event_ >= 0)
        return "event";
    if (found_ >= 0)
        return "found";
    if (!dice_.empty())
        return "dice";
    if (turn_)
        return turnTicket_ != 0 ? "" : "turn"; // its line is still coming
    if (resting_)
        return "rest";
    if (waitTap_ && beat_.seen) // a bot's beat waits (once it has walked in), even one that handed the turn on
        return slide_.guest >= 0 ? "" : "caption";
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
    if (dt <= 0.0F || pause_ || inspect_ >= 0)
        return;
    clock_ += dt, tapClock_ += dt;
    diceClock_ += dt, flashClock_ = std::max(0.0F, flashClock_ - dt);
    std::erase_if(popups_, [&](const Popup &p) { return p.until <= clock_; });
    if (Screen() == "fall") // a death on show: the night waits for Continue
        return;
    // 0. A scene holds everything while its shots play, each for its seconds.
    if (Screen() == "cutscene")
    {
        shotT_ += dt;
        if (shotT_ >= scenes_[scene_][U(shot_)].value("seconds", 4.0F))
        {
            shotT_ = 0.0F;
            if (++shot_ >= static_cast<int>(scenes_[scene_].size()))
                NextScene();
        }
        return;
    }
    ResolveLines();
    if (slide_.guest >= 0)
    {
        slide_.t += dt / options_.slideSeconds;
        if (slide_.t < 1.0F)
            return;
        slide_ = {};
    }
    if (arrive_.cell >= 0 || event_ >= 0 || found_ >= 0 || !dice_.empty() || turn_ || waitTap_ || resting_)
        return;
    hold_ -= dt;
    if (hold_ > 0.0F)
        return;
    // 1b. A new turn opens with its banner (the user); a bot waits for it, the player need not.
    const int acting = Current(state_);
    const int turnKey = state_.round * 8 + state_.turn;
    if (acting >= 0 && !state_.chat.active && !state_.battle.active && turnKey != bannerKey_)
        bannerKey_ = turnKey, banner_ = skipping_ ? 0.0F : options_.turnBannerSeconds, cues_.push_back("turn");
    if (banner_ > 0.0F)
    {
        banner_ -= dt;
        if (banner_ > 0.0F && !PlayerTurn())
            return;
    }
    // 2. The player's turn: the beats are over, Space wears off.
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
        const auto pawns = View().pawns;
        const bool seen = std::any_of(pawns.begin(), pawns.end(), [&](const PawnView &p) { return p.guest == acting; });
        if (turnKey != thoughtTurn_ && seen && !state_.battle.active && !state_.chat.active)
        {
            thoughtTurn_ = turnKey;
            think_ = skipping_ ? 0.0F : std::max(options_.thinkSeconds, took);
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
    pending_ = preview_ = false;
    const NightState before = state_;
    const Action action = pendingAction_;
    if (!Apply(def_, state_, action))
        return;
    Follow();
    if (action.kind == ActionKind::Move && before.guests[U(acting)].cell != state_.guests[U(acting)].cell)
        slide_ = {acting, before.guests[U(acting)].cell, state_.guests[U(acting)].cell, 0.0F};
    Notice(before.logCount);
    ++beats_;
    beat_ = captions_.lines.empty() ? Beat{} : Narrate(def_, before, state_, NewLog(state_, before.logCount), action, acting, state_.player, captions_, options_.light);
    if (captionTicket_ != 0)
        beat_.caption = words_->Take(captionTicket_, beat_.caption), captionTicket_ = 0;
    said_ = saidTicket_ != 0 ? words_->Take(saidTicket_, "") : "", saidTicket_ = 0;
    int entered = -1;
    const bool told = beat_.seen && !beat_.dark; // a deed in the dark is not told (T217)
    beatEvent_ = told ? RoomEvent(before, state_, action, acting, entered) : -1; // the caption names it
    if (told && (state_.guests[U(acting)].bonusUsed & ~before.guests[U(acting)].bonusUsed).any()) // a bonus room paid
    {
        const RoomDef &room = def_.rooms[U(state_.cells[U(state_.guests[U(acting)].cell)].room)];
        beat_.result += (beat_.result.empty() ? "" : "; ") + std::string("+") + std::to_string(room.amount) + " " + StatName(room.stat) + " from the " + room.name;
    }
    const bool notable = told && NotableBeat(NewLog(state_, before.logCount), action);
    hold_ = !beat_.seen || skipping_ ? options_.unseenBeatSeconds : fast_ ? options_.fastBeatSeconds : notable ? options_.beatSeconds : options_.plainBeatSeconds;
    waitTap_ = notable && options_.tapToContinue && !skipping_ && !fast_;
    moment_ = notable && options_.moments && !skipping_;
    // 6. A deed the player waits on (a room, a fight, a chat, a power) shows in two steps (Play.md "Bots"):
    //    once the bot has walked in, what it faces (no result); after a tap its roll, then the result.
    const bool facing = action.kind == ActionKind::Move || action.kind == ActionKind::Attack || action.kind == ActionKind::Power ||
                        action.kind == ActionKind::Chat || action.kind == ActionKind::OpenChat;
    if (waitTap_ && facing)
    {
        preview_ = true, heldResult_ = beat_.result, beat_.result.clear(), botDice_.reset();
        for (const LogEntry &e : NewLog(state_, before.logCount))
            if (e.kind == LogKind::Check && e.who == acting)
            {
                Dice d{e.a, e.other, e.b, "", "", e.a + e.other >= e.b, 0, "", "", 0, ""};
                d.who = ShownName(def_, state_, acting);
                if (beatEvent_ >= 0)
                    d.stat = StatName(def_.events[U(beatEvent_)].stat), d.result = RollResult(def_.events[U(beatEvent_)], d.passed, acting);
                botDice_ = d;
            }
    }
    if (!beat_.seen)
        slide_ = {}; // only what the player sees slides
    else if (slide_.guest >= 0)
        cues_.push_back("step");
}

std::string Play::Label(const Action &action) const
{
    const auto name = [&](int guest) { return guest >= 0 ? ShownName(def_, state_, guest) : std::string{"?"}; };
    const auto monster = [&](int m) { return def_.monsters[U(state_.monsters[U(m)].kind)].name; };
    switch (action.kind)
    {
    case ActionKind::Move: return "Take the passage";
    case ActionKind::EndTurn: return "End turn";
    case ActionKind::PickUp: return "Pick up the " + def_.items[U(state_.floor[U(action.a)].item)].name;
    case ActionKind::UseItem: return "Use the " + def_.items[U(state_.guests[U(Me())].items[U(action.a)])].name;
    case ActionKind::DropItem: return "Drop the " + def_.items[U(state_.guests[U(Me())].items[U(action.a)])].name;
    case ActionKind::Chat: // a request (Night.md "Persuading")
        return action.b == -2 ? "Ask " + name(action.a) + " for an item"
             : action.b >= 0  ? "Ask " + name(action.a) + " to go after " + name(action.b)
                              : "Ask " + name(action.a) + " to spare you";
    case ActionKind::Attack: return "Attack " + (action.a >= 0 ? "the " + monster(action.a) : name(action.b));
    case ActionKind::Strike:
    {
        const Fighter &f = state_.battle.fighters[U(action.a)];
        return "Strike " + (f.monster >= 0 ? "the " + monster(f.monster) : name(f.guest));
    }
    case ActionKind::Guard: return "Guard";
    case ActionKind::Leave: return "Leave the battle";
    case ActionKind::LevelUp: return action.a == 0 ? "Level up: +1 Might" : "Level up: +1 Sanity";
    case ActionKind::Power:
    {
        const std::string power = def_.powers[U(state_.guests[U(Me())].power)].id;
        return action.a >= 0 && action.b < 0 ? "Power (" + power + ") on " + name(action.a) : "Power (" + power + ")";
    }
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
    data["popups"] = nlohmann::json::array();
    for (const Popup &p : popups_)
        if (p.until > clock_)
            data["popups"].push_back({{"guest", p.guest}, {"text", p.text}});
    data["canSkip"] = !PlayerTurn() && state_.player >= 0 && !state_.over;
    if (const Banner b = TurnBanner(); b.guest >= 0)
        data["banner"] = {{"name", ShownName(def_, state_, b.guest)}, {"yours", b.guest == state_.player}, {"theirs", b.guest != state_.player},
                          {"portrait", ArtPath("portraits", def_.guests[U(b.guest)].id).generic_string()}};
    // The hunt: the Taken's life, always in sight (the user, 2026-11-16); its fall when it comes.
    if (state_.taken.present && state_.taken.guest >= 0)
    {
        const int t = state_.taken.guest;
        data["taken"] = {{"name", def_.guests[U(t)].taken}, {"hp", state_.guests[U(t)].hp}, {"maxHp", Stat(def_, state_, t, StatKind::MaxHp)}};
    }
    if (Screen() == "cutscene")
        data["cutscene"] = {{"line", SceneLine()}, {"shot", shot_ + 1}, {"shots", scenes_[scene_].size()}};
    if (Screen() == "fall")
        data["fall"] = {{"title", fall_.title}, {"cause", fall_.cause},
                        {"portrait", fall_.killer >= 0 ? ArtPath("portraits", def_.guests[U(fall_.killer)].id).generic_string() : std::string{}}};
    data["noScene"] = Screen() != "cutscene"; // the HUD and labels hide during a scene
    data["goalOpen"] = goalOpen_, data["goalFolded"] = !goalOpen_;
    data["view"] = roomView_ ? "Board view" : "Room view"; // the switch's label
    data["canView"] = CanView();
    if (state_.over)
        data["end"] = EndData();
    if (turn_ && !state_.over)
        data["turn"] = TurnData();
    if (inspect_ >= 0)
        data["inspect"] = InspectData();
    if (beat_.seen && (hold_ > 0.0F || waitTap_))
        data["caption"] = {{"guest", ShownName(def_, state_, beat_.guest)}, {"line", beat_.caption}, {"result", beat_.result}, {"tap", waitTap_},
                           {"preview", preview_}, {"event", beatEvent_ >= 0 ? def_.events[U(beatEvent_)].name : ""}, {"eventText", beatEvent_ >= 0 ? def_.events[U(beatEvent_)].text : ""},
                           {"said", said_}, {"saidBy", saidGuest_ >= 0 ? ShownName(def_, state_, saidGuest_) : ""}};
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
    data["phase"] = state_.killer.cell >= 0 ? "the killer walks" : "evening";
    std::string buffs;
    for (int f = 0; state_.battle.active && f < state_.battle.count; ++f)
        if (state_.battle.fighters[U(f)].guest == me && !state_.battle.fighters[U(f)].taken)
            for (const std::int8_t b : state_.battle.fighters[U(f)].buff)
                if (b >= 0)
                    buffs += (buffs.empty() ? "" : ", ") + def_.buffs[U(b)].name;
    data["me"] = {{"name", def_.guests[U(me)].name}, {"portrait", ArtPath("portraits", def_.guests[U(me)].id).generic_string()}, {"hp", guest.hp}, {"maxHp", Stat(def_, state_, me, StatKind::MaxHp)},
                  {"might", Stat(def_, state_, me, StatKind::Might)}, {"sanity", Stat(def_, state_, me, StatKind::Sanity)},
                  {"speed", Stat(def_, state_, me, StatKind::Speed)}, {"level", guest.level}, {"buffs", buffs},
                  {"flash", flashClock_ > 0.0F ? flash_ : ""}, // an HP change, for a moment
                  {"down", guest.down}, {"ghost", guest.ghost}};
    // The battle royale's HUD: the night, the quests (Night.md "Quests"), the player's power, the killer near.
    data["night"] = state_.night;
    data["quests"] = data["openQuests"] = data["takenQuests"] = nlohmann::json::array();
    for (std::size_t q = 0; q < def_.quests.size(); ++q)
    {
        const QuestDef &quest = def_.quests[q];
        const int by = state_.questBy[q] - 1;
        const std::string stat = quest.stat == StatKind::Might ? "Might" : quest.stat == StatKind::Sanity ? "Sanity" : quest.stat == StatKind::Speed ? "Speed" : "max HP";
        data["quests"].push_back({{"name", quest.name}, {"text", quest.text}, {"open", by < 0}, {"by", by >= 0 ? def_.guests[U(by)].name : ""},
                                  {"reward", def_.powers[U(quest.power)].id + " power, +" + std::to_string(quest.amount) + " " + stat}});
        data[by < 0 ? "openQuests" : "takenQuests"].push_back(data["quests"].back());
    }
    if (guest.power >= 0)
        data["power"] = {{"name", def_.powers[U(guest.power)].id}, {"cooldown", guest.powerCooldown}};
    data["killerNear"] = KillerNear();
    // The top bar: whoever acts (a bot on its turn, marked as theirs), else the player; its portrait,
    // a list of one so it can be clicked, brings the camera back to them ("focus").
    const int top = Acting() >= 0 ? Acting() : me;
    const Guest &t = state_.guests[U(top)];
    data["top"] = {{"name", ShownName(def_, state_, top)}, {"portrait", ArtPath("portraits", def_.guests[U(top)].id).generic_string()},
                   {"hp", t.hp}, {"maxHp", Stat(def_, state_, top, StatKind::MaxHp)}, {"might", Stat(def_, state_, top, StatKind::Might)},
                   {"sanity", Stat(def_, state_, top, StatKind::Sanity)}, {"speed", Stat(def_, state_, top, StatKind::Speed)},
                   {"level", t.level}, {"theirs", top != me}};
    data["topList"] = nlohmann::json::array({data["top"]});
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
    data["turnTitle"] = me == state_.player ? std::string{"Your turn"} : ShownName(def_, state_, me) + "'s turn";

    // 2. The turn menu: Move first, then fights, chats and the rest, Use item, End turn last.
    data["actions"] = nlohmann::json::array();
    const std::vector<Action> legal = PlayerTurn() ? LegalActions(def_, state_) : std::vector<Action>{};
    if (std::any_of(legal.begin(), legal.end(), [](const Action &a) { return a.kind == ActionKind::Move && a.a < transportMove; }))
        data["actions"].push_back({{"label", "Move"}, {"action", "move"}, {"arg", ""}});
    if (Screen() == "menu")
        data["actions"].push_back({{"label", roomView_ ? "Board view" : "Room view"}, {"action", "look"}, {"arg", ""}});
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

    // 3. The company: allies, guests being won over; the round's order, the acting guest marked.
    data["allies"] = data["winning"] = data["turnOrder"] = nlohmann::json::array();
    for (int i = 0; i < state_.count; ++i)
        if (const int g = state_.order[U(i)]; g >= 0)
            data["turnOrder"].push_back({{"guest", g}, {"now", g == Acting()}, {"dead", state_.guests[U(g)].dead},
                                         {"portrait", ArtPath("portraits", def_.guests[U(g)].id).generic_string()}});
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

    // 4. Rolls, events and chats, each with its chance by the rules.
    if (!state_.over && event_ >= 0)
    {
        const EventDef &ev = def_.events[U(event_)];
        data["event"] = {{"room", def_.rooms[U(state_.cells[U(eventCell_)].room)].name}, {"name", ev.name}, {"text", ev.text},
                         {"picture", ArtPath("events", ev.id).generic_string()},
                         {"stat", StatName(ev.stat)}, {"statValue", eventStat_}, {"target", ev.target},
                         {"chance", ChancePercent(eventStat_, ev.target)},
                         {"pass", EffectText(ev.passEffect, ev.passAmount)}, {"fail", EffectText(ev.failEffect, ev.failAmount)},
                         {"kind", ev.trap ? "TRAP" : "EVENT"}, {"lasting", lasting_}, {"cleared", cleared_}, {"rolls", cleared_.empty()},
                         {"button", cleared_.empty() ? "Roll" : "Continue"}};
    }
    if (!state_.over && arrive_.cell >= 0)
        data["arrive"] = {{"kind", arrive_.kind}, {"room", def_.rooms[U(state_.cells[U(arrive_.cell)].room)].name}, {"line", arrive_.line}};
    if (!state_.over && found_ >= 0)
    {
        const ItemDef &item = def_.items[U(found_)];
        data["found"] = {{"name", item.name}, {"does", item.text}, {"kind", KindName(item.kind)}, {"picture", ArtPath("items", item.id).generic_string()}};
    }
    if (!state_.over && !dice_.empty())
    {
        // While it tumbles the face changes, more and more slowly (fewer flips as sqrt flattens); then the
        // final number holds a moment before the verdict is told.
        const Dice &d = dice_.front();
        const bool rolling = diceClock_ < options_.diceSeconds, told = diceClock_ >= options_.diceSeconds + DiceSettle();
        const int flips = rolling ? static_cast<int>(14.0F * std::sqrt(diceClock_ / options_.diceSeconds)) : 0;
        const int face = rolling ? 1 + (flips * 7 + d.die * 3) % 20 : d.die;
        const bool landed = told;
        data["dice"] = {{"die", face}, {"stat", d.stat}, {"statValue", d.statValue}, {"total", d.die + d.statValue},
                        {"target", d.target}, {"passed", landed && d.passed}, {"failed", landed && !d.passed}, {"result", landed ? d.result : ""},
                        {"plain", landed && d.stat.empty()}, {"withStat", landed && !d.stat.empty()}, {"rolling", rolling}, {"landed", !rolling},
                        {"reply", landed ? d.reply : ""}, {"replyBy", d.replyBy}, {"told", told}, {"who", d.who}};
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
                    return ShownName(def_, state_, g);
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
    const std::vector<LogEntry> entries = NewLog(state_, since);
    for (const LogEntry &e : entries) // the sounds of what happened (all of it heard: the house is loud)
        if (const char *cue = e.kind == LogKind::Hit ? "hit" : e.kind == LogKind::Died || e.kind == LogKind::Down ? "death" : e.kind == LogKind::Opened ? "door"
                            : e.kind == LogKind::Item ? "item" : e.kind == LogKind::Killer && e.b == 0 ? "bell" : nullptr)
            cues_.push_back(cue);
    // The killer's steps, when it comes within hearing (Night.md "The killer").
    if (KillerNear() && std::any_of(entries.begin(), entries.end(), [](const LogEntry &e) { return e.kind == LogKind::Killer && e.b == 1; }))
        cues_.push_back("killer");
    for (const LogEntry &e : entries)
        if (e.kind == LogKind::Killer && e.b == 0) // midnight: the killer wakes
            StartScene("midnight");
        else if (e.kind == LogKind::Dawn) // the survivors' next night
            StartScene("dawn");
        else if (e.kind == LogKind::Ended) // after the fall (if any), the ending's scene
            StartScene(static_cast<Outcome>(e.a) == Outcome::House ? "ending_house" : "ending_escaped");
        else if (e.kind == LogKind::Hit && e.other >= 0) // the damage over the struck guest, for a moment
            popups_.push_back({e.other, "-" + std::to_string(e.a), clock_ + 2.0F});
        else if (e.kind == LogKind::Monster && e.other == 1 && def_.monsters[U(e.a)].taken >= 0 && state_.taken.guest >= 0)
        {
            // A minion bound to the Taken fell: its share of the Taken's life goes too, said over the Taken.
            const MonsterDef &m = def_.monsters[U(e.a)];
            popups_.push_back({state_.taken.guest, "-" + std::to_string(def_.guests[U(m.taken)].takenHp * m.hpShare / 100) + " (its " + m.name + ")", clock_ + 3.0F});
        }
        else if (e.kind == LogKind::Died && e.a == 1 && state_.player >= 0 && (e.who == state_.player || e.other == state_.player || SawFall(e.b)))
            fall_ = Fall(entries, e), fallSeen_ = false; // a death in sight is staged (Play.md "The fall")
}

bool Play::KillerNear() const
{
    // The living player within hearSteps rooms of the killer.
    const int me = state_.player;
    if (me < 0 || !Living(state_, me) || state_.killer.cell < 0 || state_.guests[U(me)].cell < 0)
        return false;
    const int near = Steps(def_, state_, state_.guests[U(me)].cell, state_.killer.cell);
    return near >= 0 && near <= def_.rules.killer.hearSteps;
}

bool Play::SawFall(int cell) const
{
    return cell >= 0 && View().cells[U(cell)].lit; // the room the guest died in
}

Play::Falling Play::Fall(const std::vector<LogEntry> &entries, const LogEntry &died) const
{
    // 1. Who dealt the last of their life, and how: a blow, a power, the killer, the house's traps and monsters.
    const int fallen = died.who, killer = died.other;
    const std::string it = def_.guests[U(fallen)].name, who = killer >= 0 ? def_.guests[U(killer)].name : std::string{};
    std::string cause = killer >= 0 ? who + " brought " + it + " down." : "The house took " + it + ".";
    for (const LogEntry &e : entries)
        if (e.kind == LogKind::Hit && e.other == fallen && e.who == killer && killer >= 0)
            cause = who + " struck the last blow: " + std::to_string(e.a) + " damage, and " + it + " had nothing left.";
        else if (e.kind == LogKind::Power && e.who == killer && killer >= 0)
            cause = who + "'s power struck " + it + " down.";

    if (killer < 0 && state_.killer.cell >= 0 && state_.killer.cell == died.b)
        cause = "The killer found " + it + ". It cannot be fought; only fled.";
    // 2. Staged where it fell (the Died entry keeps the room).
    return {died.b, it + " falls", cause, killer, fallen};
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
    // 1b. A chat reply: the model's words replace the written ones as soon as they are in.
    if (!dice_.empty() && dice_.front().replyTicket != 0 && words_->Ready(dice_.front().replyTicket))
        dice_.front().reply = words_->Take(dice_.front().replyTicket, dice_.front().reply), dice_.front().replyTicket = 0;
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

nlohmann::json Play::StartData(const NightDef &def, bool canContinue, int chosen)
{
    nlohmann::json guests = nlohmann::json::array();
    for (const GuestDef &g : def.guests)
        guests.push_back({{"id", g.id}, {"name", g.name}, {"style", def.styles[U(g.style)].id}, {"hp", g.hp},
                          {"might", g.might}, {"sanity", g.sanity}, {"speed", g.speed}, {"bio", g.bio},
                          {"weapon", g.weapon == Weapon::Magical ? "magical (Sanity)" : "physical (Might)"},
                          {"portrait", ArtPath("portraits", g.id).generic_string()}});
    nlohmann::json data = {{"start", {{"guests", guests}, {"canContinue", canContinue}}}};
    if (chosen >= 0 && U(chosen) < def.guests.size()) // a guest picked: their page, Start or Back
        data["bio"] = guests[U(chosen)];
    return data;
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
    return {{"who", g.name}, {"isYou", t == state_.player}, {"form", g.taken}, {"story", g.takenStory}, {"hp", state_.guests[U(t)].hp}, {"words", turnWords_},
            {"power", power.id + ": " + PowerText(power)}, {"minions", minions}, {"minionName", minionName},
            {"minionShare", share}, {"clock", clock}};
}

nlohmann::json Play::EndData() const
{
    // 1. Who won.
    std::string outcome = "The night ends.";
    if (state_.outcome == Outcome::Escaped && state_.winner >= 0)
        outcome = state_.winner == state_.player ? "You walk out of the Opera House alone. You escaped."
                                                 : def_.guests[U(state_.winner)].name + " walks out alone. The house keeps the rest.";
    if (state_.outcome == Outcome::House)
        outcome = state_.player >= 0 && state_.guests[U(state_.player)].ghost ? "No guest is left. The house wins, and you with it."
                                                                               : "No guest is left. The house wins.";
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
    if (PlayerTurn() && !waitTap_ && Current(state_) >= 0) // the player's turn: on them at once
        return state_.guests[U(Current(state_))].cell;
    if (beat_.seen && hold_ > 0.0F && beat_.camera >= 0)
        return beat_.camera;
    if (ThinkingGuest() >= 0)
        return state_.guests[U(ThinkingGuest())].cell;
    // Whoever acts: the player's guest always, a bot while the player can see it.
    const int acting = Current(state_);
    if (acting < 0 || state_.over)
        return -1;
    const int cell = state_.guests[U(acting)].cell;
    return PlayerTurn() || state_.player < 0 || View().cells[U(cell)].lit ? cell : -1;
}

void Play::Follow()
{
    // A new turn: the camera follows again (a drag frees it only until then; a click on the top bar's portrait brings it back at once).
    const int key = state_.round * 8 + state_.turn;
    if (key != followTurn_)
        following_ = true, followTurn_ = key;
}

nlohmann::json Play::InspectData() const
{
    // 1. The room: its name and kind always; who and what is in it only when lit.
    const Cell &cell = state_.cells[U(inspect_)];
    const RoomDef &room = def_.rooms[U(cell.room)];
    const BoardView view = View();
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
        data["guests"].push_back({{"name", ShownName(def_, state_, g)}, {"hp", guest.hp}, {"maxHp", Stat(def_, state_, g, StatKind::MaxHp)},
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
    // 0. A death in sight, where it fell.
    if (Screen() == "fall")
        return fall_.cell;
    // 1. Something that happened in the player's sight (options "moments"): their event, a bot's beat.
    if (options_.moments && arrive_.cell >= 0)
        return arrive_.cell;
    if (options_.moments && (event_ >= 0 || found_ >= 0 || !dice_.empty()) && state_.player >= 0)
        return event_ >= 0 && eventCell_ >= 0 ? eventCell_ : state_.guests[U(state_.player)].cell;
    if (moment_ && beat_.seen && beat_.camera >= 0 && (waitTap_ || hold_ > 0.0F))
        return beat_.camera;
    // A chat with the player, from the moment it opens.
    if (options_.moments && state_.chat.active && state_.player >= 0 && (state_.chat.asker == state_.player || state_.chat.answerer == state_.player))
        return state_.guests[U(state_.chat.asker)].cell;
    // 3. A battle the player can see (their own, or bots' in their light).
    if (state_.battle.active && state_.battle.cell >= 0 && View().cells[U(state_.battle.cell)].lit)
        return state_.battle.cell;
    return -1;
}

std::vector<Play::Actor> Play::StageActors() const
{
    std::vector<Actor> actors;
    const int cell = StageCell();
    if (cell < 0)
        return actors;
    // 0. The fall: the one who dealt it facing the fallen, who lies where they fell.
    if (Screen() == "fall")
    {
        if (fall_.killer >= 0 && InPlay(state_, fall_.killer)) // a ghost may have struck
            actors.push_back({fall_.killer, -1, 0, false});
        actors.push_back({fall_.guest, -1, 1, true});
        return actors;
    }
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

bool Play::ClickItem(int floor)
{
    // A floor item where the player stands: picked up (the bag's pickup).
    return HandleUi("bag", "pickup", std::to_string(floor));
}

bool Play::ClickGuest(int guest)
{
    // A guest: talk to them, else fight them (after midnight); else nothing (inspect is the right click's).
    if (!PlayerTurn() || Screen() != "menu" || guest < 0 || guest >= state_.count)
        return false;
    for (const Action &a : LegalActions(def_, state_))
        if (a.kind == ActionKind::Chat && a.a == guest) // talk; attacking is the menu's (no fight by a stray click)
            return Do(a);
    return false;
}

bool Play::ClickMonster(int monster)
{
    // A monster: fight it if it can be fought from here; else nothing.
    if (!PlayerTurn() || Screen() != "menu" || monster < 0 || static_cast<std::size_t>(monster) >= state_.monsters.size())
        return false;
    for (const Action &a : LegalActions(def_, state_))
        if (a.kind == ActionKind::Attack && a.a == monster)
            return Do(a);
    return false;
}

bool Play::Notable(const std::vector<LogEntry> &entries, const Action &action) const
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
    // ... or a chat; a walk alone stays on the table (the user, 2026-11-15).
    return action.kind == ActionKind::OpenChat;
}

bool Play::NotableBeat(const std::vector<LogEntry> &entries, const Action &action) const
{
    // A bot that only waits stays on the table, unless someone falls (a flood, say).
    if (action.kind == ActionKind::EndTurn)
        return std::any_of(entries.begin(), entries.end(), [](const LogEntry &e) { return e.kind == LogKind::Down || e.kind == LogKind::Died; });
    return Notable(entries, action);
}

std::string Play::RollResult(const EventDef &ev, bool passed, int guest) const
{
    // In plain words: how it went (captions.json "<pass|fail><Stat>"), what it did, and what is left.
    const std::string key = std::string(passed ? "pass" : "fail") + StatName(ev.stat);
    const auto line = captions_.lines.find(key);
    std::string text = line != captions_.lines.end() ? line->second : std::string("Your ") + StatName(ev.stat) + (passed ? " holds." : " fails you.");
    const Effect effect = passed ? ev.passEffect : ev.failEffect;
    const int amount = passed ? ev.passAmount : ev.failAmount;
    if (effect == Effect::None)
        return text + " Nothing else happens.";
    text += " " + EffectText(effect, amount);
    if (effect == Effect::Hp)
        text += " (now " + std::to_string(state_.guests[U(guest)].hp) + " of " + std::to_string(Stat(def_, state_, guest, StatKind::MaxHp)) + ")";
    if (state_.guests[U(guest)].down) // 0 HP: down, not dead (Night.md)
        return text + ". You are down: you lie where you fell until someone helps you up, or until midnight, when you stand with 1 HP.";
    return text + ".";
}

int Play::RoomEvent(const NightState &before, const NightState &after, const Action &action, int guest, int &entered) const
{
    // The room moved into (a push may have sent the guest back), and the event it brought: a new draw
    // (a new bit in eventsUsed) or the lasting one armed there, if the guest rolled or cleared it.
    const int from = before.guests[U(guest)].cell;
    entered = action.kind != ActionKind::Move ? -1 : action.a < transportMove ? Neighbour(def_, from, action.a) : after.guests[U(guest)].cell;
    if (const std::uint64_t drawn = after.eventsUsed & ~before.eventsUsed)
        return std::countr_zero(drawn);
    bool faced = false;
    for (const LogEntry &e : NewLog(after, before.logCount))
        faced = faced || ((e.kind == LogKind::Check || e.kind == LogKind::Cleared) && e.who == guest);
    return entered >= 0 && faced ? before.cells[U(entered)].event : -1;
}

float Play::DiceSettle() const { return options_.diceSeconds > 0.0F ? 0.4F : 0.0F; } // the landed number holds this long

std::vector<int> Play::GoalCells() const
{
    std::vector<int> cells;
    if (state_.player < 0 || state_.guests[U(state_.player)].goal < 0)
        return cells;
    const GoalDef &goal = def_.goals[U(state_.guests[U(state_.player)].goal)];
    for (int c = 0; goal.kind == GoalKind::Room && c < maxCells; ++c)
        if (state_.cells[U(c)].opened && state_.cells[U(c)].room == goal.target)
            cells.push_back(c);
    return cells;
}
