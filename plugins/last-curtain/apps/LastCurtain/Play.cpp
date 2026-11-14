#include "Play.hpp"

#include <BoardView.hpp>
#include <NightBots.hpp>

#include <charconv>
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

    const char *KindName(ItemKind kind)
    {
        switch (kind)
        {
        case ItemKind::Heal: return "healing";
        case ItemKind::Boost: return "fight boost";
        case ItemKind::Worn: return "worn";
        case ItemKind::Relic: return "relic";
        }
        return "";
    }
}

Play::Play(const NightDef &def, std::uint64_t seed, int player) : def_(def), state_(NewNight(def, seed, player)) {}

Play::Play(const NightDef &def, const NightState &state) : def_(def), state_(state) {}

bool Play::PlayerTurn() const
{
    const int acting = Current(state_);
    return state_.player >= 0 && acting >= 0 && Controls(state_, state_.player, acting);
}

int Play::Me() const { return PlayerTurn() ? Current(state_) : state_.player; }

bool Play::Do(const Action &action)
{
    return PlayerTurn() && Apply(def_, state_, action);
}

void Play::Click(int cell)
{
    if (!PlayerTurn() || cell < 0 || cell >= maxCells)
        return;
    const int step = MakeView(def_, state_, state_.player).cells[U(cell)].step;
    if (step >= 0)
        (void)Do(LegalActions(def_, state_)[U(step)]);
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
    // 2. Each key does what a legal action of this turn allows, or nothing.
    for (const Action &action : LegalActions(def_, state_))
        if ((action.kind == ActionKind::Move && action.a == direction) || (key == InputKey::Enter && action.kind == ActionKind::EndTurn))
            if (Do(action))
                return;
    if (key == InputKey::I && PlayerTurn() && !state_.battle.active && !state_.chat.active)
        bag_ = !bag_;
}

bool Play::HandleUi(const std::string &, const std::string &action, const std::string &arg)
{
    if (!PlayerTurn())
        return false;
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
    if (!PlayerTurn())
        return "";
    return bag_ ? "bag" : "actions";
}

void Play::Update(float)
{
    // A bot's turn: one action per call (the next task paces them).
    const int acting = Current(state_);
    if (acting < 0 || PlayerTurn())
        return;
    bag_ = false;
    (void)Apply(def_, state_, ChooseAction(def_, state_, acting));
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
    case ActionKind::TakeOmen: return "Take the omen";
    case ActionKind::LeaveOmen: return "Leave the omen";
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
    data["me"] = {{"name", def_.guests[U(me)].name}, {"hp", guest.hp}, {"maxHp", Stat(def_, state_, me, StatKind::MaxHp)},
                  {"might", Stat(def_, state_, me, StatKind::Might)}, {"sanity", Stat(def_, state_, me, StatKind::Sanity)},
                  {"speed", Stat(def_, state_, me, StatKind::Speed)}, {"level", guest.level}, {"buffs", buffs}};
    data["moves"] = guest.movesLeft;
    data["turnTitle"] = me == state_.player ? std::string{"Your turn"} : def_.guests[U(me)].name + "'s turn";

    // 2. What the turn allows besides walking; the bag before End turn, End turn last.
    data["actions"] = nlohmann::json::array();
    const std::vector<Action> legal = PlayerTurn() ? LegalActions(def_, state_) : std::vector<Action>{};
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
        data["actions"].push_back({{"label", "Bag (" + std::to_string(used) + " of " + std::to_string(def_.rules.carry) + ")"}, {"action", "bag"}, {"arg", ""}});
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
        else if (other.approvals > 0)
            data["winning"].push_back({{"name", def_.guests[U(g)].name}, {"approvals", other.approvals}, {"goal", def_.rules.persuade.approvals}});
    }
    for (const PawnView &pawn : MakeView(def_, state_, state_.player).pawns)
        if (pawn.guest != me)
            data["inLight"].push_back({{"name", def_.guests[U(pawn.guest)].name}, {"room", def_.rooms[U(state_.cells[U(pawn.cell)].room)].name}});

    // 4. The bag and the floor of this room.
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
