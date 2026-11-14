#include "NightInner.hpp"

#include <algorithm>

// The Turn at midnight and the hunt (Night.md "The Turn and the hunt").
namespace umbra::night
{
    using namespace inner;

    bool IsTaken(const NightState &state, int guest) { return state.taken.present && state.taken.guest == guest; }

    bool inner::CompanyAlive(const NightState &state)
    {
        for (int g = 0; g < state.count; ++g)
            if (!IsTaken(state, g) && !state.guests[U(g)].dead)
                return true;
        return false;
    }

    void inner::TheTurn(const NightDef &def, NightState &state)
    {
        // 1. Nobody failed a curse roll: chance picks an omen holder, all alike; with no omen taken, anyone.
        if (state.marked < 0)
        {
            std::vector<int> holders, everyone;
            for (int g = 0; g < state.count; ++g)
            {
                everyone.push_back(g);
                if (state.guests[U(g)].omens)
                    holders.push_back(g);
            }
            const std::vector<int> &from = holders.empty() ? everyone : holders;
            state.marked = I8(from[U(Below(state, static_cast<int>(from.size())))]);
        }
        // 2. The marked guest leaves every group and rises as the Taken in the under-stage, at full HP.
        if (state.understage < 0)
            ForceUnderStage(def, state);
        const int t = state.marked;
        Guest &taken = state.guests[U(t)];
        state.taken = {true, I8(t), 0, false};
        taken.group = -1;
        taken.down = false;
        taken.cell = state.understage;
        taken.hp = I16(def.guests[U(t)].takenHp);
        // 3. Everyone else gathers in the foyer; the downed stand.
        for (int g = 0; g < state.count; ++g)
        {
            Guest &guest = state.guests[U(g)];
            if (g == t || guest.dead)
                continue;
            guest.cell = state.foyer;
            if (guest.down)
                guest.down = false, guest.hp = I16(def.rules.guests.downedReturnHp);
        }
        // 4. The Taken acts last in every round.
        auto at = std::find(state.order.begin(), state.order.begin() + state.count, I8(t));
        std::rotate(at, at + 1, state.order.begin() + state.count);
        Log(state, LogKind::Turned, t, -1, state.understage);

        // 5. Ordinary monsters are gone; the Taken's minions wait in the rooms farthest from the foyer.
        state.monsters.clear();
        std::vector<int> rooms;
        for (int c = 0; c < maxCells; ++c)
            if (state.cells[U(c)].opened && c != state.foyer && c != state.understage)
                rooms.push_back(c);
        std::stable_sort(rooms.begin(), rooms.end(),
                         [&](int a, int b) { return Steps(def, state, state.foyer, a) > Steps(def, state, state.foyer, b); });
        std::size_t next = 0;
        for (std::size_t kind = 0; kind < def.monsters.size(); ++kind)
        {
            const MonsterDef &m = def.monsters[kind];
            for (int n = 0; m.taken == t && n < m.count; ++n)
            {
                const int cell = next < rooms.size() ? rooms[next++] : state.understage;
                state.monsters.push_back({I8(kind), I8(cell), 127, I16(m.hp), I16(m.might), I16(m.sanity)});
                Log(state, LogKind::Monster, -1, -1, static_cast<int>(kind), cell);
            }
        }
    }

    void inner::TurnNow(const NightDef &def, NightState &state)
    {
        // Everyone's remaining evening is skipped: the hunt starts at midnight, the company first.
        state.round = I8(def.rules.clock.turnAfterRound + 1);
        state.turn = 0;
        TheTurn(def, state);
        if (Current(state) < 0 && !state.over)
            EndTurn(def, state); // the first in the order cannot act: on to the next
        else
            StartTurn(def, state);
    }

    bool inner::ContactBattle(const NightDef &def, NightState &state, int mover, int cell)
    {
        // The Taken and every company guest standing in the room; monsters there side with the Taken.
        const int t = state.taken.guest;
        if (state.guests[U(t)].cell != cell || state.guests[U(t)].dead)
            return false;
        std::vector<Fighter> company, theirs{Fighter{I8(t), -1, true}};
        for (int g = 0; g < state.count; ++g)
            if (g != t && state.guests[U(g)].cell == cell && !state.guests[U(g)].dead && !state.guests[U(g)].down)
                company.push_back(Fighter{I8(g)});
        if (company.empty())
            return false;
        for (std::size_t m = 0; m < state.monsters.size() && theirs.size() + company.size() < 8; ++m)
            if (state.monsters[m].cell == cell && state.monsters[m].hp > 0)
                theirs.push_back(Fighter{-1, I8(m)});
        if (mover == t)
            StartBattle(def, state, cell, theirs, company);
        else
            StartBattle(def, state, cell, company, theirs);
        return true;
    }
}

// The seven powers: one code path per kind; every number from powers.json (Night.md "Powers").
namespace umbra::night
{
    using namespace inner;

    namespace
    {
        const PowerDef &PowerOf(const NightDef &def, const NightState &state)
        {
            return def.powers[U(def.guests[U(state.taken.guest)].power)];
        }

        bool Hunted(const NightState &state, int g)
        {
            const Guest &guest = state.guests[U(g)];
            return !IsTaken(state, g) && !guest.dead && !guest.down && guest.cell >= 0;
        }

        void Add(std::array<Mark, 16> &marks, Mark mark)
        {
            for (Mark &m : marks)
                if (m.cell < 0)
                {
                    m = mark;
                    return;
                }
        }

        // Would sealing this passage cut a company guest off from the Taken?
        bool SealCuts(const NightDef &def, const NightState &state, int cell, int direction)
        {
            NightState trial = state;
            Add(trial.sealed, {I8(cell), I8(direction), 0});
            const int at = state.guests[U(state.taken.guest)].cell;
            for (int g = 0; g < state.count; ++g)
                if (Hunted(state, g) && Steps(def, state, state.guests[U(g)].cell, at) >= 0 && Steps(def, trial, state.guests[U(g)].cell, at) < 0)
                    return true;
            return false;
        }
    }

    bool inner::Flooded(const NightState &state, int cell)
    {
        for (const Mark &m : state.flooded)
            if (m.cell >= 0 && m.cell == cell && state.round <= m.untilRound)
                return true;
        return false;
    }

    void inner::TakenTurnStart(NightState &state)
    {
        if (state.taken.cooldown > 0)
            --state.taken.cooldown;
        state.taken.reflect = false;
    }

    void inner::FloodEndTurn(const NightDef &def, NightState &state, int g)
    {
        if (state.taken.present && !IsTaken(state, g) && Flooded(state, state.guests[U(g)].cell))
            Hurt(def, state, g, PowerOf(def, state).damage, state.taken.guest);
    }

    void inner::PowerLegal(const NightDef &def, const NightState &state, std::vector<Action> &legal)
    {
        if (state.taken.cooldown > 0)
            return;
        const PowerDef &p = PowerOf(def, state);
        const int at = state.guests[U(state.taken.guest)].cell;
        auto steps = [&](int g) { return Steps(def, state, at, state.guests[U(g)].cell); };
        bool any = false;
        switch (p.kind)
        {
        case PowerKind::Line:
        case PowerKind::Pull:
        case PowerKind::Swap:
            for (int g = 0; g < state.count; ++g)
                if (Hunted(state, g) && state.cells[U(state.guests[U(g)].cell)].opened &&
                    (p.kind == PowerKind::Swap || (p.kind == PowerKind::Line ? InLine(def, state, at, state.guests[U(g)].cell)
                                                                             : steps(g) >= 1 && steps(g) <= p.range)))
                    legal.push_back({ActionKind::Power, I8(g)});
            break;
        case PowerKind::Area:
            for (int g = 0; g < state.count; ++g)
                any = any || (Hunted(state, g) && steps(g) >= 0 && steps(g) <= p.range);
            if (any)
                legal.push_back({ActionKind::Power});
            break;
        case PowerKind::Flood:
        case PowerKind::Reflect:
            legal.push_back({ActionKind::Power});
            break;
        case PowerKind::Seal:
            for (int c = 0; c < maxCells; ++c)
            {
                const int dist = Steps(def, state, at, c);
                if (dist < 0 || dist > p.range)
                    continue;
                for (int d = 0; d < 4; ++d)
                    if (Passage(def, state, c, d) && !SealCuts(def, state, c, d))
                        legal.push_back({ActionKind::Power, I8(c), I8(d)});
            }
            break;
        }
    }

    void inner::UsePower(const NightDef &def, NightState &state, const Action &action)
    {
        const PowerDef &p = PowerOf(def, state);
        const int t = state.taken.guest;
        Guest &taken = state.guests[U(t)];
        const int at = taken.cell;
        state.taken.cooldown = I8(p.cooldown);
        Log(state, LogKind::Power, t, action.a, static_cast<int>(p.kind), action.b);
        switch (p.kind)
        {
        case PowerKind::Line:
            Hurt(def, state, action.a, p.damage, t);
            break;
        case PowerKind::Area:
            for (int g = 0; g < state.count; ++g)
            {
                const int n = Steps(def, state, at, state.guests[U(g)].cell);
                if (Hunted(state, g) && n >= 0 && n <= p.range)
                    Hurt(def, state, g, p.damage, t);
            }
            break;
        case PowerKind::Flood:
            // The Taken's room and every room joined to it by a passage.
            Add(state.flooded, {I8(at), -1, I8(state.round + p.rounds)});
            for (int d = 0; d < 4; ++d)
                if (Passage(def, state, at, d))
                    Add(state.flooded, {I8(Neighbour(def, at, d)), -1, I8(state.round + p.rounds)});
            break;
        case PowerKind::Pull:
            state.guests[U(action.a)].cell = I8(at);
            Hurt(def, state, action.a, p.damage, t);
            if (!state.over)
                ContactBattle(def, state, t, at);
            break;
        case PowerKind::Seal:
            Add(state.sealed, {action.a, action.b, I8(state.round + p.rounds)});
            break;
        case PowerKind::Swap:
            std::swap(taken.cell, state.guests[U(action.a)].cell);
            break;
        case PowerKind::Reflect:
            state.taken.reflect = true;
            break;
        }
    }
}
