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
            // After midnight the only fight between guests is with the Taken.
            if (state.taken.present)
            {
                if (IsTaken(state, g) || IsTaken(state, o))
                    legal.push_back({ActionKind::Attack, -1, I8(o)});
                continue;
            }
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
        const int cell = state.guests[U(defender)].cell;
        if (state.taken.present && ContactBattle(def, state, attacker, cell))
            return;
        // 1. Everyone whose light reaches the room thinks worse of the attacker.
        for (int w = 0; w < state.count; ++w)
        {
            const Guest &watcher = state.guests[U(w)];
            if (w != attacker && Up(watcher) && Lit(def, state, watcher.cell, cell, Stat(def, state, w, StatKind::Light)))
            {
                std::int16_t &bond = state.bonds[U(w * maxGuests + attacker)];
                bond = I16(std::max(-1000, bond - def.rules.guests.attackBondLoss));
            }
        }
        state.guests[U(defender)].attackedBy = static_cast<std::uint8_t>(state.guests[U(defender)].attackedBy | (1u << attacker));
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

namespace umbra::night
{
    using namespace inner;

    Seen Knows(const NightState &state, int viewer, int guest)
    {
        const Guest &v = state.guests[U(viewer)];
        return {((v.sawOmen >> guest) & 1u) != 0, ((v.sawFail >> guest) & 1u) != 0};
    }

    Outcome Result(const NightState &state) { return state.outcome; }

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

    void inner::TakeOmen(const NightDef &def, NightState &state, int g)
    {
        Guest &guest = state.guests[U(g)];
        Cell &c = state.cells[U(guest.cell)];
        const OmenDef &omen = def.omens[U(c.omen)];
        // 1. The curse roll, with the Sanity before the gift, against a target that rises.
        const auto &targets = def.rules.omens.targets;
        const int target = targets[U(std::min<int>(state.omensTaken, static_cast<int>(targets.size()) - 1))];
        const Check check = RollCheck(def, state, g, StatKind::Sanity, target);
        // 2. The gift.
        guest.omens = static_cast<std::uint8_t>(guest.omens | (1u << c.omen));
        guest.hp = I16(guest.hp + omen.maxhp);
        ++state.omensTaken;
        c.resolved = true;
        Log(state, LogKind::Omen, g, -1, c.omen, check.passed ? 1 : 0);
        if (!check.passed && state.marked < 0)
        {
            state.marked = I8(g);
            Log(state, LogKind::Marked, g);
        }
        // 3. Only those whose light reaches the room know.
        for (int w = 0; w < state.count; ++w)
        {
            Guest &watcher = state.guests[U(w)];
            if (w == g || watcher.down || watcher.dead || !Lit(def, state, watcher.cell, guest.cell, Stat(def, state, w, StatKind::Light)))
                continue;
            watcher.sawOmen = static_cast<std::uint8_t>(watcher.sawOmen | (1u << g));
            if (!check.passed)
                watcher.sawFail = static_cast<std::uint8_t>(watcher.sawFail | (1u << g));
        }
    }
}
