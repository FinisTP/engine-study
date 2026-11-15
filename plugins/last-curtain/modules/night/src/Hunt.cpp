#include "NightInner.hpp"

#include <algorithm>

// The Turn at midnight and the hunt (Night.md "The Turn and the hunt").
namespace umbra::night
{
    using namespace inner;

    bool IsTaken(const NightState &state, int guest) { return state.taken.present && state.taken.guest == guest; }

    std::string ShownName(const NightDef &def, const NightState &state, int guest)
    {
        return IsTaken(state, guest) ? def.guests[U(guest)].taken : def.guests[U(guest)].name;
    }

    bool Living(const NightState &state, int g) { return g >= 0 && g < state.count && !state.guests[U(g)].dead; }

    bool InPlay(const NightState &state, int g)
    {
        return Living(state, g) || (g >= 0 && g < state.count && state.guests[U(g)].ghost && state.guests[U(g)].cell >= 0);
    }

    int LivingCount(const NightState &state)
    {
        int n = 0;
        for (int g = 0; g < state.count; ++g)
            n += Living(state, g) ? 1 : 0;
        return n;
    }

    bool inner::CompanyAlive(const NightState &state)
    {
        for (int g = 0; g < state.count; ++g)
            if (!IsTaken(state, g) && !state.guests[U(g)].dead)
                return true;
        return false;
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
            taken.visited.set(U(taken.cell)); // a room it has now stood in this turn
            break;
        case PowerKind::Reflect:
            state.taken.reflect = true;
            break;
        }
    }
}
