#include "NightInner.hpp"

#include <algorithm>

// Guest against guest, bonds and the player's allies (Night.md "The company").
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

        bool Allies(const NightState &state, int a, int b)
        {
            return state.guests[U(a)].group >= 0 && state.guests[U(a)].group == state.guests[U(b)].group;
        }
    }

    int Bond(const NightState &state, int from, int to) { return state.bonds[U(from * maxGuests + to)]; }

    bool Controls(const NightState &state, int controller, int guest)
    {
        return controller >= 0 && (guest == controller || (state.guests[U(controller)].group == 0 && state.guests[U(guest)].group == 0));
    }

    std::vector<int> Joiners(const NightDef &, const NightState &state, int attacker, int defender)
    {
        // Bots in the room, standing, not allied to either side, not the player's.
        std::vector<int> joiners;
        const int cell = state.guests[U(defender)].cell;
        for (int g = 0; g < state.count; ++g)
            if (g != attacker && g != defender && state.guests[U(g)].cell == cell && Up(state.guests[U(g)]) &&
                !Allies(state, g, attacker) && !Allies(state, g, defender) && !Controls(state, state.player, g))
                joiners.push_back(g);
        return joiners;
    }

    void inner::CompanyLegal(const NightDef &, const NightState &state, int g, std::vector<Action> &legal)
    {
        const Guest &me = state.guests[U(g)];
        for (int o = 0; o < state.count; ++o)
        {
            const Guest &other = state.guests[U(o)];
            if (o == g || other.cell != me.cell || !Up(other))
                continue;
            // 1. Anyone may attack anyone in the room but an ally.
            if (!Allies(state, g, o))
                legal.push_back({ActionKind::Attack, -1, I8(o)});
            // 2. Only the player persuades, into a group of at most 3.
            if (g == state.player && other.group != 0 && GroupSize(state, 0) < 3 && state.round >= me.askedUntilRound[U(o)])
                legal.push_back({ActionKind::Persuade, I8(o)});
        }
    }

    void inner::AttackGuest(const NightDef &def, NightState &state, int attacker, int defender)
    {
        // 1. Everyone whose light reaches the room thinks worse of the attacker.
        const int cell = state.guests[U(defender)].cell;
        for (int w = 0; w < state.count; ++w)
        {
            const Guest &watcher = state.guests[U(w)];
            if (w != attacker && Up(watcher) && Lit(def, state, watcher.cell, cell, Stat(def, state, w, StatKind::Light)))
            {
                std::int16_t &bond = state.bonds[U(w * maxGuests + attacker)];
                bond = I16(std::max(-1000, bond - def.rules.guests.attackBondLoss));
            }
        }
        // 2. The battle; bystanders are asked first.
        std::uint8_t asking = 0;
        for (int g : Joiners(def, state, attacker, defender))
            asking = static_cast<std::uint8_t>(asking | (1u << g));
        StartBattle(def, state, cell, Side(state, attacker), Side(state, defender), asking);
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
}
