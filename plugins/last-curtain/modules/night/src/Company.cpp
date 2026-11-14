#include "NightInner.hpp"

#include <algorithm>

// Bonds, the player's allies and omens (Night.md "The company", "Omens").
namespace umbra::night
{
    using namespace inner;

    namespace
    {
        bool Up(const Guest &g) { return !g.down && !g.dead; }

        int GroupSize(const NightState &state, int group)
        {
            return static_cast<int>(std::count_if(state.guests.begin(), state.guests.begin() + state.count,
                                                   [&](const Guest &g) { return g.group == group && !g.dead; }));
        }
    }

    int Bond(const NightState &state, int from, int to) { return state.bonds[U(from * maxGuests + to)]; }

    bool Controls(const NightState &state, int controller, int guest)
    {
        return controller >= 0 && (guest == controller || (state.guests[U(controller)].group == 0 && state.guests[U(guest)].group == 0));
    }

    Outcome Result(const NightState &state) { return state.outcome; }

    int CurseTarget(const NightDef &def, const NightState &state)
    {
        const auto &omens = def.rules.omens;
        const int hours = (state.round - 1) * 30 / 60;
        return omens.targets[U(std::min<int>(state.omensTaken, static_cast<int>(omens.targets.size()) - 1))] + omens.perHour * hours;
    }

    void inner::CompanyLegal(const NightDef &, const NightState &state, int g, std::vector<Action> &legal)
    {
        const Guest &me = state.guests[U(g)];
        for (int o = 0; o < state.count; ++o)
        {
            const Guest &other = state.guests[U(o)];
            if (o == g || other.cell != me.cell || !Up(other))
                continue;
            // 1. The only fight between guests is with the Taken, after midnight.
            if (state.taken.present && (IsTaken(state, g) || IsTaken(state, o)))
                legal.push_back({ActionKind::Attack, -1, I8(o)});
            // 2. Only the player persuades, into a group of at most 3, before midnight.
            if (!state.taken.present && g == state.player && other.group != 0 && GroupSize(state, 0) < 3 &&
                state.round >= me.askedUntilRound[U(o)])
                legal.push_back({ActionKind::Persuade, I8(o)});
        }
    }

    void inner::Persuade(const NightDef &def, NightState &state, int asker, int target)
    {
        const auto &rules = def.rules.persuade;
        const int bond = Bond(state, target, asker);
        const int number = rules.target + (bond > 0 ? -rules.bond : bond < 0 ? rules.bond : 0);
        const Check check = RollCheck(def, state, asker, StatKind::Sanity, number);
        if (check.passed)
            state.guests[U(asker)].group = state.guests[U(target)].group = 0;
        else
            state.guests[U(asker)].askedUntilRound[U(target)] = I8(state.round + rules.retryRounds);
        Log(state, LogKind::Persuade, asker, target, check.passed ? 1 : 0);
    }

    void inner::EndNight(NightState &state, Outcome outcome)
    {
        state.outcome = outcome;
        state.over = true;
        state.battle.active = false;
        Log(state, LogKind::Ended, -1, -1, static_cast<int>(outcome));
    }

    void inner::OfferOmen(const NightDef &def, NightState &state, int cell)
    {
        // The room's omen is drawn the first time anyone walks in.
        Cell &c = state.cells[U(cell)];
        if (c.omen < 0)
        {
            std::vector<int> left;
            for (std::size_t o = 0; o < def.omens.size() && o < 8; ++o)
                if (!((state.omensUsed >> o) & 1u))
                    left.push_back(static_cast<int>(o));
            if (left.empty())
            {
                c.resolved = true;
                return;
            }
            c.omen = I8(left[U(Below(state, static_cast<int>(left.size())))]);
            state.omensUsed = static_cast<std::uint8_t>(state.omensUsed | (1u << c.omen));
        }
        state.omenOffer = I8(cell);
    }

    bool inner::TakeOmen(const NightDef &def, NightState &state, int g)
    {
        Guest &guest = state.guests[U(g)];
        Cell &c = state.cells[U(guest.cell)];
        const OmenDef &omen = def.omens[U(c.omen)];
        // 1. The curse roll: a plain d20, the same odds for everyone, against a target that rises
        //    with every omen taken and with every hour of the night.
        const int target = CurseTarget(def, state);
        const Check check = RollCheck(def, state, g, StatKind::None, target);
        // 2. The gift.
        guest.omens = static_cast<std::uint8_t>(guest.omens | (1u << c.omen));
        guest.hp = I16(guest.hp + omen.maxhp);
        ++state.omensTaken;
        c.resolved = true;
        Log(state, LogKind::Omen, g, -1, c.omen, check.passed ? 1 : 0);
        // 3. A failed roll: this guest turns, now.
        if (!check.passed)
            state.marked = I8(g);
        return !check.passed;
    }
}
