#include "NightInner.hpp"

#include <algorithm>
#include <bit>

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
        // 1. Nobody failed: chance marks an omen holder, weighted by omens held; with no omen taken, anyone.
        if (state.marked < 0)
        {
            std::array<int, maxGuests> weight{};
            int total = 0;
            for (int g = 0; g < state.count; ++g)
                total += weight[U(g)] = std::popcount(state.guests[U(g)].omens);
            if (total == 0)
                for (int g = 0; g < state.count; ++g)
                    total += weight[U(g)] = 1;
            int pick = Below(state, total);
            for (int g = 0; g < state.count && state.marked < 0; ++g)
                if ((pick -= weight[U(g)]) < 0)
                    state.marked = I8(g);
        }
        // 2. The marked guest leaves every group and rises as the Taken in the under-stage, at full HP.
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
