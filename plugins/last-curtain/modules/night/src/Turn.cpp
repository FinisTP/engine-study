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
            if (RoomOf(def, state, cell).kind != RoomKind::Transport)
                return -1;
            for (int c = 0; c < maxCells; ++c)
                if (c != cell && state.cells[U(c)].opened && RoomOf(def, state, c).kind == RoomKind::Transport)
                    return c;
            return -1;
        }

        // Entering a room: one with something still in it stops movement.
        void Enter(const NightDef &def, NightState &state, int g, int cell)
        {
            Guest &guest = G(state, g);
            guest.cell = I8(cell);
            const Cell &c = state.cells[U(cell)];
            if (IsTaken(state, g))
                return;
            switch (RoomOf(def, state, cell).kind)
            {
            case RoomKind::Item:
            case RoomKind::Event:
            case RoomKind::Trap:
            case RoomKind::Omen:
                if (!c.resolved)
                    guest.movesLeft = 0;
                break;
            case RoomKind::Bonus:
                if (!((guest.bonusUsed >> cell) & 1u))
                    guest.movesLeft = 0;
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

        // The first guest into an item, event or trap room takes care of it.
        void Resolve(const NightDef &def, NightState &state, int g, int cell)
        {
            Cell &c = state.cells[U(cell)];
            c.resolved = true;
            const RoomKind kind = RoomOf(def, state, cell).kind;
            if (kind == RoomKind::Item)
                return GiveItem(def, state, g, DrawItem(def, state));
            const int e = DrawEvent(def, state, kind == RoomKind::Trap);
            if (e < 0)
                return;
            const EventDef &event = def.events[U(e)];
            const Check check = RollCheck(def, state, g, event.stat, event.target);
            if (check.passed)
                ApplyEffect(def, state, g, event.passEffect, event.passAmount);
            else
                ApplyEffect(def, state, g, event.failEffect, event.failAmount);
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
        return state.guests[U(g)].down || state.guests[U(g)].dead ? -1 : g;
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
        if (IsTaken(state, g))
            TakenTurnStart(state);
        if (g >= 0)
            G(state, g).movesLeft = I8(Stat(def, state, g, StatKind::Speed));
    }

    void inner::EndTurn(const NightDef &def, NightState &state)
    {
        // 1. A bonus room pays once per guest when the turn ends there.
        const int g = BoardGuest(state);
        if (g >= 0)
        {
            Guest &guest = G(state, g);
            const RoomDef &room = RoomOf(def, state, guest.cell);
            if (room.kind == RoomKind::Bonus && !((guest.bonusUsed >> guest.cell) & 1u))
            {
                guest.bonusUsed |= 1ULL << guest.cell;
                std::int16_t *stat = room.stat == StatKind::Might ? &guest.might : room.stat == StatKind::Sanity ? &guest.sanity
                                   : room.stat == StatKind::Speed ? &guest.speed : room.stat == StatKind::MaxHp ? &guest.maxHp : nullptr;
                if (stat)
                    *stat = I16(*stat + room.amount);
            }
            guest.movesLeft = 0;
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
                if (state.round == def.rules.clock.turnAfterRound + 1 && !state.taken.present)
                    TheTurn(def, state);
                if (state.round > def.rules.clock.rounds)
                {
                    if (state.outcome == Outcome::Running)
                        EndNight(state, state.taken.present ? Outcome::Taken : Outcome::Running);
                    state.over = true;
                    return;
                }
                RoundStart(def, state);
            }
            const Guest &next = G(state, state.order[U(state.turn)]);
            if (!next.down && !next.dead)
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
            return state.player; // the player answers a question
        for (int g = 0; g < state.count; ++g)
            if (state.guests[U(g)].pending > 0 && !state.guests[U(g)].dead)
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
        if (state.omenOffer >= 0)
        {
            legal.push_back({ActionKind::TakeOmen});
            legal.push_back({ActionKind::LeaveOmen});
            return legal;
        }

        // 1. Moving: through passages and doorways, and between the transport pair.
        if (guest.movesLeft > 0)
        {
            for (std::int8_t d = 0; d < 4; ++d)
                if (Passage(def, state, guest.cell, d) || Doorway(def, state, guest.cell, d))
                    legal.push_back({ActionKind::Move, d});
            if (Pair(def, state, guest.cell) >= 0)
                legal.push_back({ActionKind::Move, transportMove});
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

        if (taken)
            PowerLegal(def, state, legal);
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
            if (action.b >= 0)
                ContactBattle(def, state, g, guest.cell); // only with the Taken, after midnight
            else
                StartBattle(def, state, guest.cell, Side(state, g), {Fighter{-1, action.a}});
            break;
        case ActionKind::TakeOmen:
        case ActionKind::LeaveOmen:
            state.omenOffer = -1;
            if (action.kind == ActionKind::TakeOmen && TakeOmen(def, state, g))
                TurnNow(def, state); // the curse took hold: everyone wakes at midnight
            else
                inner::EndTurn(def, state);
            break;
        case ActionKind::Power:
            UsePower(def, state, action);
            break;
        case ActionKind::Chat:
            OpenChat(def, state, action.a, false);
            break;
        case ActionKind::OpenChat:
            OpenChat(def, state, g, true);
            break;
        case ActionKind::Answer:
        {
            // A chat the player opened uses their turn; one a bot opened does not.
            const bool byBot = state.chat.byBot;
            AnswerChat(def, state, action.a);
            if (!byBot)
                inner::EndTurn(def, state);
            break;
        }
        case ActionKind::Move:
        {
            int to = action.a == transportMove ? Pair(def, state, guest.cell) : Neighbour(def, guest.cell, action.a);
            if (action.a != transportMove && !Passage(def, state, guest.cell, action.a))
            {
                to = Open(def, state, guest.cell, action.a);
                Log(state, LogKind::Opened, g, -1, to);
            }
            --guest.movesLeft;
            guest.from = guest.cell;
            Enter(def, state, g, to);
            // After midnight: the Taken and the company meet; the Taken passes monsters and rooms by.
            if (state.taken.present && ContactBattle(def, state, g, to))
            {
                guest.movesLeft = 0;
                break;
            }
            if (IsTaken(state, g))
                break;
            // A monster in the room: a fight at once, the monster's side first.
            if (const int monster = MonsterAt(state, to); monster >= 0)
            {
                guest.movesLeft = 0;
                StartBattle(def, state, to, {Fighter{-1, I8(monster)}}, Side(state, g));
                break;
            }
            // Resolving an item, event or trap room ends the turn; an omen is offered.
            const RoomKind kind = RoomOf(def, state, to).kind;
            if (kind == RoomKind::Omen && !state.cells[U(to)].resolved)
                OfferOmen(def, state, to);
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

    void Hurt(const NightDef &, NightState &state, int g, int amount, int byGuest)
    {
        Guest &guest = G(state, g);
        if (guest.down || guest.dead || amount <= 0 || (IsTaken(state, g) && byGuest < 0))
            return;
        guest.hp = I16(std::max(0, guest.hp - amount));
        if (guest.hp > 0)
            return;
        if (state.taken.present)
        {
            // After midnight 0 HP is death: items fall; the night may end.
            guest.dead = true;
            for (std::int8_t &item : guest.items)
                if (item >= 0)
                    state.floor.push_back({guest.cell, item}), item = -1;
            Log(state, LogKind::Died, g, byGuest);
            if (IsTaken(state, g))
                EndNight(state, CompanyAlive(state) ? Outcome::Company : Outcome::Taken);
            else if (!CompanyAlive(state))
                EndNight(state, Outcome::Taken);
            return;
        }
        guest.down = true;
        guest.downedBy = I8(byGuest);
        Log(state, LogKind::Down, g, byGuest);
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
        return (def.rules.clock.startHour + (state.round - 1) * 30 / 60) % 24;
    }

    int Minute(const NightDef &, const NightState &state) { return (state.round - 1) * 30 % 60; }

    int Stat(const NightDef &def, const NightState &state, int g, StatKind kind)
    {
        const Guest &guest = state.guests[U(g)];
        int value = BuffBonus(def, state, g, kind);
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
        // 3. Omens held.
        for (std::size_t o = 0; o < def.omens.size(); ++o)
            if ((guest.omens >> o) & 1u)
            {
                const OmenDef &omen = def.omens[o];
                value += kind == StatKind::Might ? omen.might : kind == StatKind::Sanity ? omen.sanity : kind == StatKind::Speed ? omen.speed
                       : kind == StatKind::MaxHp ? omen.maxhp : 0;
            }
        return value;
    }
}
