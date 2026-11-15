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

}

// The seven powers, won by quests: one code path per kind; every number from powers.json (Night.md "Powers").
namespace umbra::night
{
    using namespace inner;

    namespace
    {
        const PowerDef &PowerOf(const NightDef &def, const NightState &state, int g) { return def.powers[U(state.guests[U(g)].power)]; }

        // A power's target: any other living guest on the board.
        bool Target(const NightState &state, int user, int g) { return g != user && Living(state, g) && state.guests[U(g)].cell >= 0; }

        void Add(std::array<Mark, 16> &marks, Mark mark)
        {
            for (Mark &m : marks)
                if (m.cell < 0)
                {
                    m = mark;
                    return;
                }
        }
    }

    bool inner::Flooded(const NightState &state, int cell)
    {
        for (const Mark &m : state.flooded)
            if (m.cell >= 0 && m.cell == cell && state.round <= m.untilRound)
                return true;
        return false;
    }

    void inner::FloodEndTurn(const NightDef &def, NightState &state, int g)
    {
        // Ending a turn in someone else's water hurts, by their power's damage.
        for (const Mark &m : state.flooded)
            if (m.cell >= 0 && m.cell == state.guests[U(g)].cell && state.round <= m.untilRound && m.by >= 0 && m.by != g &&
                state.guests[U(m.by)].power >= 0)
            {
                Hurt(def, state, g, PowerOf(def, state, m.by).damage, m.by);
                return;
            }
    }

    void inner::PowerLegal(const NightDef &def, const NightState &state, int user, std::vector<Action> &legal)
    {
        const Guest &me = state.guests[U(user)];
        if (me.power < 0 || me.powerCooldown > 0 || me.ghost)
            return;
        const PowerDef &p = PowerOf(def, state, user);
        const int at = me.cell;
        auto steps = [&](int g) { return Steps(def, state, at, state.guests[U(g)].cell); };
        bool any = false;
        switch (p.kind)
        {
        case PowerKind::Line:
        case PowerKind::Pull:
        case PowerKind::Swap:
            for (int g = 0; g < state.count; ++g)
                if (Target(state, user, g) && (p.kind == PowerKind::Swap || (p.kind == PowerKind::Line ? InLine(def, state, at, state.guests[U(g)].cell)
                                                                                                         : steps(g) >= 1 && steps(g) <= p.range)))
                    legal.push_back({ActionKind::Power, I8(g)});
            break;
        case PowerKind::Area:
            for (int g = 0; g < state.count; ++g)
                any = any || (Target(state, user, g) && steps(g) >= 0 && steps(g) <= p.range);
            if (any)
                legal.push_back({ActionKind::Power});
            break;
        case PowerKind::Flood:
        case PowerKind::Reflect:
            legal.push_back({ActionKind::Power});
            break;
        case PowerKind::Seal: // any passage within range: shut for a while (it stops the killer too)
            for (int c = 0; c < maxCells; ++c)
                if (const int dist = Steps(def, state, at, c); dist >= 0 && dist <= p.range)
                    for (int d = 0; d < 4; ++d)
                        if (Passage(def, state, c, d))
                            legal.push_back({ActionKind::Power, I8(c), I8(d)});
            break;
        }
    }

    void inner::UsePower(const NightDef &def, NightState &state, int user, const Action &action)
    {
        const PowerDef &p = PowerOf(def, state, user);
        Guest &me = state.guests[U(user)];
        const int at = me.cell;
        me.powerCooldown = I8(p.cooldown);
        Log(state, LogKind::Power, user, action.a, static_cast<int>(p.kind), action.b);
        switch (p.kind)
        {
        case PowerKind::Line:
            Hurt(def, state, action.a, p.damage, user);
            break;
        case PowerKind::Area:
            for (int g = 0; g < state.count; ++g)
                if (const int n = Steps(def, state, at, state.guests[U(g)].cell); Target(state, user, g) && n >= 0 && n <= p.range)
                    Hurt(def, state, g, p.damage, user);
            break;
        case PowerKind::Flood: // its room and every room joined to it, for a while
            Add(state.flooded, {I8(at), -1, I8(state.round + p.rounds), I8(user)});
            for (int d = 0; d < 4; ++d)
                if (Passage(def, state, at, d))
                    Add(state.flooded, {I8(Neighbour(def, at, d)), -1, I8(state.round + p.rounds), I8(user)});
            break;
        case PowerKind::Pull:
            state.guests[U(action.a)].cell = I8(at);
            Hurt(def, state, action.a, p.damage, user);
            break;
        case PowerKind::Seal:
            Add(state.sealed, {action.a, action.b, I8(state.round + p.rounds), I8(user)});
            break;
        case PowerKind::Swap:
            std::swap(me.cell, state.guests[U(action.a)].cell);
            me.visited.set(U(me.cell));
            break;
        case PowerKind::Reflect:
            me.reflect = true;
            break;
        }
    }
}
