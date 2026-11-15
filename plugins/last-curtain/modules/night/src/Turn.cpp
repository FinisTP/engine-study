#include "NightInner.hpp"

#include <algorithm>

namespace umbra::night
{
    using namespace inner;

    namespace
    {
        Guest &G(NightState &state, int g) { return state.guests[U(g)]; }

        int Carried(const Guest &guest)
        {
            return static_cast<int>(std::count_if(guest.items.begin(), guest.items.end(), [](std::int8_t i) { return i >= 0; }));
        }

        // The other transport room, if it is on the board.
        int Pair(const NightDef &def, const NightState &state, int cell)
        {
            if (!state.cells[U(cell)].opened || RoomOf(def, state, cell).kind != RoomKind::Transport)
                return -1;
            for (int c = 0; c < maxCells; ++c)
                if (c != cell && state.cells[U(c)].opened && RoomOf(def, state, c).kind == RoomKind::Transport)
                    return c;
            return -1;
        }

        // A commit: no more moves this turn, counted from here (Night.md "Moves").
        void Stop(Guest &guest) { guest.movesLeft = guest.budget = 0, guest.origin = guest.cell; }

        // Entering a room: one with something still in it stops movement.
        void Enter(const NightDef &def, NightState &state, int g, int cell)
        {
            Guest &guest = G(state, g);
            guest.cell = I8(cell);
            const Cell &c = state.cells[U(cell)];
            if (guest.goal >= 0 && def.goals[U(guest.goal)].kind == GoalKind::Room && def.goals[U(guest.goal)].target == c.room)
                guest.goalRoomSeen = true;
            if (IsTaken(state, g))
                return;
            switch (RoomOf(def, state, cell).kind)
            {
            case RoomKind::Item:
            case RoomKind::Event:
            case RoomKind::Trap:
                if (!c.resolved)
                    Stop(guest);
                break;
            case RoomKind::Bonus:
                if (!guest.bonusUsed[U(cell)])
                    Stop(guest);
                break;
            default:
                break;
            }
        }

        void Heal(const NightDef &def, NightState &state, int g, int amount) { HealGuest(def, state, g, amount); }

        // An unused row, uniformly (events) or by weight (items); -1 when none is left.
        int DrawEvent(const NightDef &def, NightState &state, bool trap)
        {
            std::vector<int> rows;
            for (std::size_t e = 0; e < def.events.size() && e < 64; ++e)
                if (def.events[e].trap == trap && !((state.eventsUsed >> e) & 1u))
                    rows.push_back(static_cast<int>(e));
            if (rows.empty())
                return -1;
            const int e = rows[U(Below(state, static_cast<int>(rows.size())))];
            state.eventsUsed |= 1ULL << e;
            return e;
        }

        int DrawItem(const NightDef &def, NightState &state)
        {
            int total = 0;
            for (std::size_t i = 0; i < def.items.size() && i < 64; ++i)
                total += (state.itemsUsed >> i) & 1u ? 0 : def.items[i].weight;
            if (total == 0)
                return -1;
            int pick = Below(state, total);
            for (std::size_t i = 0; i < def.items.size() && i < 64; ++i)
                if (!((state.itemsUsed >> i) & 1u) && (pick -= def.items[i].weight) < 0)
                {
                    state.itemsUsed |= 1ULL << i;
                    return static_cast<int>(i);
                }
            return -1;
        }

        // Into a free slot, or onto the floor when the guest carries all they may.
        void GiveItem(const NightDef &def, NightState &state, int g, int item)
        {
            if (item < 0)
                return;
            Guest &guest = G(state, g);
            auto slot = std::find(guest.items.begin(), guest.items.end(), std::int8_t(-1));
            if (Carried(guest) < def.rules.carry && slot != guest.items.end())
                *slot = I8(item);
            else
                state.floor.push_back({guest.cell, I8(item)});
            Log(state, LogKind::Item, g, -1, item);
        }

        void ApplyEffect(const NightDef &def, NightState &state, int g, Effect effect, int amount)
        {
            Guest &guest = G(state, g);
            auto raise = [&](std::int16_t &stat) { stat = I16(std::max(0, stat + amount)); };
            switch (effect)
            {
            case Effect::Hp:
                if (amount < 0)
                    Hurt(def, state, g, -amount, -1);
                else
                    Heal(def, state, g, amount);
                break;
            case Effect::MaxHp:
                raise(guest.maxHp);
                guest.hp = I16(std::clamp(guest.hp + std::max(0, amount), 0, Stat(def, state, g, StatKind::MaxHp)));
                break;
            case Effect::Might: raise(guest.might); break;
            case Effect::Sanity: raise(guest.sanity); break;
            case Effect::Speed: raise(guest.speed); break;
            case Effect::Xp: guest.xp = I16(guest.xp + amount); break;
            case Effect::Back: // back through the doorway they just came by
                if (guest.from >= 0)
                    guest.cell = guest.from;
                break;
            case Effect::Item:
                for (int n = 0; n < amount; ++n)
                    GiveItem(def, state, g, DrawItem(def, state));
                break;
            default:
                return;
            }
            Log(state, LogKind::Effect, g, -1, static_cast<int>(effect), amount);
        }

        // A persistent event's other way to clear: the stat, or the item carried.
        bool MeetsClear(const NightDef &def, const NightState &state, int g, const EventDef &event)
        {
            const Guest &guest = state.guests[U(g)];
            if (event.clear == ClearKind::Stat)
                return Stat(def, state, g, event.clearStat) >= event.clearAmount;
            return event.clear == ClearKind::Carry && std::find(guest.items.begin(), guest.items.end(), I8(event.clearItem)) != guest.items.end();
        }

        // The first guest into an item, event or trap room takes care of it; a persistent event or
        // trap stays armed (Cell::event) and catches every guest until one clears it (Night.md).
        void Resolve(const NightDef &def, NightState &state, int g, int cell)
        {
            Cell &c = state.cells[U(cell)];
            const RoomKind kind = RoomOf(def, state, cell).kind;
            if (kind == RoomKind::Item)
            {
                c.resolved = true;
                GiveItem(def, state, g, DrawItem(def, state));
                return;
            }
            // 1. The armed one, or a new draw.
            const int e = c.event >= 0 ? c.event : DrawEvent(def, state, kind == RoomKind::Trap);
            if (e < 0)
            {
                c.resolved = true;
                return;
            }
            const EventDef &event = def.events[U(e)];
            const auto clear = [&] {
                c.resolved = true, c.event = -1;
                if (event.persist)
                    Log(state, LogKind::Cleared, g, -1, e);
            };
            // 2. Its other condition met: cleared at the door, no roll (it counts as passed for an endure quest).
            const auto passed = [&] {
                if (!event.trap)
                    QuestCount(def, state, g, QuestKind::Endure);
            };
            if (event.persist && MeetsClear(def, state, g, event))
            {
                passed();
                return clear();
            }
            const Check check = RollCheck(def, state, g, event.stat, event.target);
            if (check.passed)
                passed();
            if (check.passed)
                ApplyEffect(def, state, g, event.passEffect, event.passAmount);
            else
                ApplyEffect(def, state, g, event.failEffect, event.failAmount);
            // 3. Once only, or passed: gone; failed and persistent: it stays.
            if (!event.persist || check.passed)
                clear();
            else
                c.event = I8(e);
        }

        void UseItem(const NightDef &def, NightState &state, int g, int slot, int target)
        {
            Guest &guest = G(state, g);
            const ItemDef &item = def.items[U(guest.items[U(slot)])];
            if (item.kind == ItemKind::Relic)
            {
                Guest &fallen = G(state, target);
                fallen.down = false;
                fallen.hp = I16(def.rules.guests.downedReturnHp);
                Log(state, LogKind::Woke, target, g);
            }
            else if (item.effect == Effect::HpRoom)
            {
                // Self and every ally of the same group in the room.
                for (int o = 0; o < state.count; ++o)
                    if (o == g || (guest.group >= 0 && G(state, o).group == guest.group && G(state, o).cell == guest.cell && !G(state, o).down))
                        Heal(def, state, o, item.amount);
            }
            else
                Heal(def, state, g, item.amount);
            guest.items[U(slot)] = -1;
        }
    }

    void inner::HealGuest(const NightDef &def, NightState &state, int g, int amount)
    {
        Guest &guest = G(state, g);
        guest.hp = I16(std::min(guest.hp + amount, Stat(def, state, g, StatKind::MaxHp)));
    }

    int inner::BoardGuest(const NightState &state)
    {
        if (state.over || state.turn < 0 || state.turn >= state.count)
            return -1;
        const int g = state.order[U(state.turn)];
        return InPlay(state, g) ? g : -1;
    }

    int inner::MonsterAt(const NightState &state, int cell)
    {
        for (std::size_t m = 0; m < state.monsters.size(); ++m)
            if (state.monsters[m].cell == cell && state.monsters[m].hp > 0)
                return static_cast<int>(m);
        return -1;
    }

    std::array<std::int8_t, maxGuests> inner::Order(const std::array<int, maxGuests> &rolls, const std::array<int, maxGuests> &speeds, int count)
    {
        std::array<std::int8_t, maxGuests> order{};
        for (int g = 0; g < maxGuests; ++g)
            order[U(g)] = I8(g);
        std::stable_sort(order.begin(), order.begin() + count, [&](std::int8_t a, std::int8_t b) {
            if (rolls[U(a)] != rolls[U(b)])
                return rolls[U(a)] > rolls[U(b)];
            return speeds[U(a)] > speeds[U(b)];
        });
        return order;
    }

    void inner::StartTurn(const NightDef &def, NightState &state)
    {
        const int g = BoardGuest(state); // not Current(): a level being chosen is not a turn
        if (g >= 0)
        {
            Guest &guest = G(state, g);
            guest.powerCooldown = I8(std::max(0, guest.powerCooldown - 1)), guest.reflect = false; // a power cools a turn
            guest.movesLeft = guest.budget = I8(Stat(def, state, g, StatKind::Speed));
            guest.origin = guest.cell, guest.steps = 0;
            guest.visited.reset(), guest.visited.set(U(guest.cell));
        }
    }

    void inner::EndTurn(const NightDef &def, NightState &state)
    {
        // 1. A bonus room pays once per guest when the turn ends there.
        const int g = BoardGuest(state);
        if (g >= 0)
        {
            Guest &guest = G(state, g);
            const RoomDef &room = RoomOf(def, state, guest.cell);
            if (room.kind == RoomKind::Bonus && !guest.bonusUsed[U(guest.cell)])
            {
                guest.bonusUsed.set(U(guest.cell));
                std::int16_t *stat = room.stat == StatKind::Might ? &guest.might : room.stat == StatKind::Sanity ? &guest.sanity
                                   : room.stat == StatKind::Speed ? &guest.speed : room.stat == StatKind::MaxHp ? &guest.maxHp : nullptr;
                if (stat)
                    *stat = I16(*stat + room.amount);
            }
            Stop(guest);
            FloodEndTurn(def, state, g);
            if (state.over)
                return;
        }

        // 2. The next guest who can act; a new round after the last.
        for (;;)
        {
            if (++state.turn >= state.count)
            {
                state.turn = 0;
                ++state.round;
                if (state.round > def.rules.clock.rounds)
                    Dawn(def, state); // two or more still alive: another night, a new map
                if (state.over)
                    return;
                RoundStart(def, state);
            }
            if (InPlay(state, state.order[U(state.turn)]))
                break;
        }
        StartTurn(def, state);
    }

    int Current(const NightState &state)
    {
        if (state.over)
            return -1;
        // 1. A battle: the guest fighter to act. 2. A level to choose. 3. The board.
        if (state.battle.active)
            return state.battle.fighters[U(state.battle.turn)].guest;
        if (state.chat.active)
            return state.chat.answerer;
        for (int g = 0; g < state.count; ++g)
            if (state.guests[U(g)].pending > 0 && Living(state, g))
                return g;
        return BoardGuest(state);
    }

    std::vector<Action> LegalActions(const NightDef &def, const NightState &state)
    {
        std::vector<Action> legal;
        const int g = Current(state);
        if (g < 0 || state.round > def.rules.clock.rounds)
            return legal;
        const Guest &guest = state.guests[U(g)];
        if (state.battle.active)
        {
            BattleLegal(def, state, legal);
            return legal;
        }
        if (state.chat.active)
        {
            legal.push_back({ActionKind::Answer, 0});
            legal.push_back({ActionKind::Answer, 1});
            return legal;
        }
        if (guest.pending > 0)
        {
            legal.push_back({ActionKind::LevelUp, 0});
            legal.push_back({ActionKind::LevelUp, 1});
            return legal;
        }

        // 1. Moving: through passages (a step costs only if it leads further from the origin) and between
        //    the transport pair, within the turn's step cap. The map is whole: no move opens a room.
        if (guest.steps < def.rules.moves.stepsPerSpeed * Stat(def, state, g, StatKind::Speed))
        {
            const auto within = [&](int to) { const int d = Distance(def, state, guest.origin, to); return d >= 0 && d <= guest.budget; };
            for (std::int8_t d = 0; d < 4; ++d)
                if (Passage(def, state, guest.cell, d) && within(Neighbour(def, guest.cell, d)))
                    legal.push_back({ActionKind::Move, d});
            if (const int pair = Pair(def, state, guest.cell); pair >= 0 && within(pair))
                legal.push_back({ActionKind::Move, transportMove});
        }
        // 1b. A ghost: it moves, attacks the living in its room, ends its turn; nothing else.
        if (guest.ghost)
        {
            for (int o = 0; o < state.count; ++o)
                if (Living(state, o) && state.guests[U(o)].cell == guest.cell)
                    legal.push_back({ActionKind::Attack, -1, I8(o)});
            legal.push_back({ActionKind::EndTurn});
            return legal;
        }

        // 2. Items, all free: heal items used, any item dropped, a floor item picked up.
        const bool taken = IsTaken(state, g);
        for (std::int8_t s = 0; s < maxCarry && !taken; ++s)
        {
            if (guest.items[U(s)] < 0)
                continue;
            const ItemDef &item = def.items[U(guest.items[U(s)])];
            if (item.kind == ItemKind::Heal)
                legal.push_back({ActionKind::UseItem, s});
            if (item.kind == ItemKind::Relic)
                for (int o = 0; o < state.count; ++o)
                    if (state.guests[U(o)].down && !state.guests[U(o)].dead && state.guests[U(o)].cell == guest.cell)
                        legal.push_back({ActionKind::UseItem, s, I8(o)});
            legal.push_back({ActionKind::DropItem, s});
        }
        if (Carried(guest) < def.rules.carry && !taken)
            for (std::size_t f = 0; f < state.floor.size(); ++f)
                if (state.floor[f].cell == guest.cell)
                    legal.push_back({ActionKind::PickUp, I8(f)});

        // 3. Fighting a monster in the room.
        const int monster = MonsterAt(state, guest.cell);
        if (monster >= 0 && !taken)
            legal.push_back({ActionKind::Attack, I8(monster)});
        CompanyLegal(def, state, g, legal);

        PowerLegal(def, state, g, legal); // a power won by a quest, when it has cooled
        legal.push_back({ActionKind::EndTurn});
        return legal;
    }

    bool Apply(const NightDef &def, NightState &state, const Action &action)
    {
        const std::vector<Action> legal = LegalActions(def, state);
        if (std::find(legal.begin(), legal.end(), action) == legal.end())
            return false;
        const int g = Current(state);
        Guest &guest = G(state, g);
        if (state.battle.active)
        {
            BattleApply(def, state, action);
            return true;
        }
        switch (action.kind)
        {
        case ActionKind::LevelUp:
            guest.maxHp = I16(guest.maxHp + def.rules.levels.hp);
            (action.a == 0 ? guest.might : guest.sanity) += I16(def.rules.levels.stat);
            ++guest.level;
            --guest.pending;
            Log(state, LogKind::Level, g, -1, guest.level);
            break;
        case ActionKind::Attack:
            if (action.b >= 0) // a guest (or a ghost) against another, one on one
                StartBattle(def, state, guest.cell, {Fighter{I8(g)}}, {Fighter{action.b}});
            else
                StartBattle(def, state, guest.cell, Side(state, g), {Fighter{-1, action.a}});
            break;
        case ActionKind::Power:
            UsePower(def, state, g, action);
            break;
        case ActionKind::Chat:
            OpenChat(def, state, g, action.a, false, action.b); // the other guest asks; b is the request
            break;
        case ActionKind::OpenChat:
            OpenChat(def, state, g, action.a, true); // the bot asks the player
            break;
        case ActionKind::Answer:
        {
            // A chat the answerer opened uses its turn; a question from a bot on its turn does not.
            const bool opened = state.chat.answerer == BoardGuest(state);
            AnswerChat(def, state, action.a);
            if (opened)
                inner::EndTurn(def, state);
            break;
        }
        case ActionKind::Move:
        {
            const int to = action.a == transportMove ? Pair(def, state, guest.cell) : Neighbour(def, guest.cell, action.a);
            // 1. The step: a new room commits (moves count from it with what is left); otherwise what is
            //    left is the budget less the shortest way back to the origin (Night.md "Moves").
            guest.steps = I8(guest.steps + 1);
            guest.from = guest.cell;
            guest.cell = I8(to);
            guest.visited.set(U(to));
            guest.movesLeft = I8(guest.budget - std::max(0, Distance(def, state, guest.origin, to)));
            if (guest.ghost) // a ghost passes rooms and monsters by
                break;
            if (IntoKiller(def, state, g)) // walked into the killer's room: dead, the turn passes
            {
                if (!state.over)
                    inner::EndTurn(def, state);
                break;
            }
            Enter(def, state, g, to);
            Quests(def, state, g);
            // A monster in the room: a fight at once, the monster's side first.
            if (const int monster = MonsterAt(state, to); monster >= 0)
            {
                Stop(guest);
                StartBattle(def, state, to, {Fighter{-1, I8(monster)}}, Side(state, g));
                break;
            }
            // Resolving an item, event or trap room ends the turn.
            const RoomKind kind = RoomOf(def, state, to).kind;
            if (!state.cells[U(to)].resolved && (kind == RoomKind::Item || kind == RoomKind::Event || kind == RoomKind::Trap))
            {
                Resolve(def, state, g, to);
                inner::EndTurn(def, state);
            }
            break;
        }
        case ActionKind::UseItem:
            UseItem(def, state, g, action.a, action.b);
            break;
        case ActionKind::DropItem:
            state.floor.push_back({guest.cell, guest.items[U(action.a)]});
            guest.items[U(action.a)] = -1;
            break;
        case ActionKind::PickUp:
        {
            auto slot = std::find(guest.items.begin(), guest.items.end(), std::int8_t(-1));
            *slot = state.floor[U(action.a)].item;
            state.floor.erase(state.floor.begin() + action.a);
            Quests(def, state, g);
            break;
        }
        case ActionKind::EndTurn:
            inner::EndTurn(def, state);
            break;
        default:
            return false;
        }
        return true;
    }

    Check RollCheck(const NightDef &def, NightState &state, int g, StatKind stat, int target)
    {
        Check check{Roll(state, 20), Stat(def, state, g, stat), target, false};
        check.passed = check.die + check.stat >= target;
        Log(state, LogKind::Check, g, check.stat, check.die, target);
        return check;
    }

    void Hurt(const NightDef &def, NightState &state, int g, int amount, int byGuest)
    {
        Guest &guest = G(state, g);
        if (!InPlay(state, g) || amount <= 0)
            return;
        guest.hp = I16(std::max(0, guest.hp - amount));
        if (guest.hp > 0)
            return;
        Log(state, LogKind::Died, g, byGuest, guest.ghost ? 0 : 1, guest.cell); // a 1: a living guest died; b: where
        // 1. A ghost struck down leaves the board and comes back later (Night.md "Ghosts").
        if (guest.ghost)
        {
            guest.cell = -1, guest.returnRound = I8(state.round + def.rules.ghost.respawnRounds);
            return;
        }
        // 2. A living guest dies: its items fall where it stood; it rises as a ghost next round.
        for (std::int8_t &item : guest.items)
            if (item >= 0)
                state.floor.push_back({guest.cell, item}), item = -1;
        guest.dead = guest.ghost = true;
        guest.cell = -1, guest.returnRound = I8(state.round + 1);
        // 3. One left alive escapes; none left, the house wins.
        if (const int living = LivingCount(state); living <= 1)
        {
            for (int o = 0; o < state.count; ++o)
                if (Living(state, o))
                    state.winner = I8(o);
            EndNight(state, living == 1 ? Outcome::Escaped : Outcome::House);
        }
    }

    std::vector<LogEntry> NewLog(const NightState &state, std::size_t since)
    {
        std::vector<LogEntry> entries;
        const std::size_t size = state.log.size();
        for (std::size_t i = std::max<std::size_t>(since, state.logCount > size ? state.logCount - size : 0); i < state.logCount; ++i)
            entries.push_back(state.log[i % size]);
        return entries;
    }

    int Hour(const NightDef &def, const NightState &state)
    {
        return (def.rules.clock.startHour + (state.round - 1) * def.rules.clock.minutesPerRound / 60) % 24;
    }

    int Minute(const NightDef &def, const NightState &state) { return (state.round - 1) * def.rules.clock.minutesPerRound % 60; }

    int Stat(const NightDef &def, const NightState &state, int g, StatKind kind)
    {
        const Guest &guest = state.guests[U(g)];
        int value = BuffBonus(def, state, g, kind);
        if (guest.ghost) // weak and bare: the ghost's own numbers (rules.json "ghost")
        {
            const auto &h = def.rules.ghost;
            return value + (kind == StatKind::Might ? h.might : kind == StatKind::Sanity ? h.sanity : kind == StatKind::Speed ? h.speed
                          : kind == StatKind::MaxHp ? h.hp : kind == StatKind::Light ? maxCells : 0);
        }
        if (IsTaken(state, g))
        {
            // The Taken's strength is its form's, fixed.
            const GuestDef &d = def.guests[U(g)];
            return value + (kind == StatKind::Might ? d.takenMight : kind == StatKind::Sanity ? d.takenSanity : kind == StatKind::Speed ? d.takenSpeed
                          : kind == StatKind::MaxHp ? d.takenHp : kind == StatKind::Light ? def.rules.light.steps : 0);
        }
        value += kind == StatKind::Might ? guest.might : kind == StatKind::Sanity ? guest.sanity : kind == StatKind::Speed ? guest.speed
               : kind == StatKind::MaxHp ? guest.maxHp : kind == StatKind::Light ? def.rules.light.steps : 0;
        // 1. Worn items while carried.
        static constexpr Effect wornEffect[] = {Effect::Might, Effect::Sanity, Effect::Speed, Effect::MaxHp, Effect::Light};
        if (kind != StatKind::None)
            for (std::int8_t i : guest.items)
                if (i >= 0 && def.items[U(i)].kind == ItemKind::Worn && def.items[U(i)].effect == wornEffect[static_cast<int>(kind)])
                    value += def.items[U(i)].amount;
        // 2. Water slows anyone but the Taken to 1.
        if (kind == StatKind::Speed && Flooded(state, guest.cell))
            return 1;
        return value;
    }
}

namespace umbra::night
{
    int Distance(const NightDef &def, const NightState &state, int from, int to)
    {
        // Breadth-first through passages and the transport pair.
        if (from < 0 || to < 0)
            return -1;
        std::array<int, maxCells> steps;
        steps.fill(-1);
        std::array<int, maxCells> queue{};
        int head = 0, tail = 0;
        steps[inner::U(from)] = 0, queue[inner::U(tail++)] = from;
        while (head < tail)
        {
            const int at = queue[inner::U(head++)];
            if (at == to)
                return steps[inner::U(at)];
            const auto visit = [&](int next) {
                if (next >= 0 && steps[inner::U(next)] < 0)
                    steps[inner::U(next)] = steps[inner::U(at)] + 1, queue[inner::U(tail++)] = next;
            };
            for (int d = 0; d < 4; ++d)
                if (Passage(def, state, at, d))
                    visit(Neighbour(def, at, d));
            visit(Pair(def, state, at));
        }
        return -1;
    }
}

namespace umbra::night
{
    bool LeaveItem(NightState &state, int guest, int slot)
    {
        if (guest < 0 || guest >= state.count || slot < 0 || slot >= maxCarry)
            return false;
        Guest &g = state.guests[inner::U(guest)];
        if (g.items[inner::U(slot)] < 0)
            return false;
        state.floor.push_back({g.cell, g.items[inner::U(slot)]});
        g.items[inner::U(slot)] = -1;
        return true;
    }
}
