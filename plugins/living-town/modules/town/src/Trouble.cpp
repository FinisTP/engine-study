#include "TownInner.hpp"

#include <algorithm>

// Trouble (S24): what can go wrong in a town, and what it does. Town.md "Trouble".
namespace umbra::town
{
    using namespace inner;

    namespace
    {
        constexpr int illDays = 2;   // an illness keeps someone in for two days
        constexpr int poor = 10;     // fewer coins than this and stealing becomes thinkable
        constexpr int worthIt = 20;  // nobody steals from someone with less

        bool Here(const TownState &state, int resident)
        {
            return resident >= 0 && static_cast<std::size_t>(resident) < state.residents.size() && state.residents[static_cast<std::size_t>(resident)].gone == Gone::No;
        }
        const ResidentState &Of(const TownState &state, int resident) { return state.residents[static_cast<std::size_t>(resident)]; }
        bool IllNow(const TownState &state, int resident) { return state.tick < Of(state, resident).illUntil; }
        int WorstNeed(const TownState &state, int resident)
        {
            const auto &needs = Of(state, resident).needs;
            return *std::max_element(needs.begin(), needs.end()) / ticksPerDay;
        }
        // How many people `resident` has fallen out with.
        int Rivals(const TownState &state, int resident)
        {
            int rivals = 0;
            for (std::size_t other = 0; other < state.residents.size(); ++other)
                rivals += Here(state, static_cast<int>(other)) && state.Bond(static_cast<std::size_t>(resident), other) <= rivalsAt;
            return rivals;
        }
        void SetBond(TownState &state, int a, int b, int amount)
        {
            std::int16_t &bond = state.bonds[static_cast<std::size_t>(a) * state.residents.size() + static_cast<std::size_t>(b)];
            bond = static_cast<std::int16_t>(std::clamp(bond + amount, -1000, 1000));
        }
    }

    bool Possible(const TownDef &, const TownState &state, const Trouble &t)
    {
        if (!Here(state, t.who))
            return false;
        const bool pair = Here(state, t.other) && t.other != t.who;
        switch (t.kind)
        {
        case TroubleKind::Ill: return !IllNow(state, t.who);
        case TroubleKind::Theft: return pair && Of(state, t.other).money > 0;
        case TroubleKind::Feud: return pair;
        case TroubleKind::Leave:
        case TroubleKind::Death: return true;
        }
        return false;
    }

    std::vector<Trouble> Proposals(const TownDef &def, const TownState &state)
    {
        std::vector<Trouble> out;
        const int count = static_cast<int>(state.residents.size());
        for (int who = 0; who < count; ++who)
        {
            if (!Here(state, who))
                continue;
            const ResidentState &r = Of(state, who);
            // 1. Alone: the worn-down fall ill, the ill may die, the friendless or broke may leave.
            if (!IllNow(state, who) && WorstNeed(state, who) >= def.urgent)
                out.push_back({TroubleKind::Ill, who, -1});
            if (IllNow(state, who) && r.illUntil - state.tick <= ticksPerDay)
                out.push_back({TroubleKind::Death, who, -1});
            if (Rivals(state, who) > 0 || (r.money < poor && r.crisis != 0))
                out.push_back({TroubleKind::Leave, who, -1});
            // 2. With someone in the same place: the poor steal, those who dislike each other row.
            for (int other = 0; other < count; ++other)
            {
                if (other == who || !Here(state, other) || Of(state, other).place != r.place)
                    continue;
                const int feeling = state.Bond(static_cast<std::size_t>(who), static_cast<std::size_t>(other));
                if (r.money < poor && Of(state, other).money >= worthIt && feeling < friendsAt)
                    out.push_back({TroubleKind::Theft, who, other});
                if (who < other && (feeling < 0 || state.Bond(static_cast<std::size_t>(other), static_cast<std::size_t>(who)) < 0))
                    out.push_back({TroubleKind::Feud, who, other});
            }
        }
        return out;
    }

    int Likely(const TownDef &, const TownState &state, const Trouble &t)
    {
        const ResidentState &r = Of(state, t.who);
        const auto bond = [&](int a, int b) { return state.Bond(static_cast<std::size_t>(a), static_cast<std::size_t>(b)); };
        switch (t.kind)
        {
        case TroubleKind::Ill: return WorstNeed(state, t.who);
        case TroubleKind::Theft: return Of(state, t.other).money - 10 * r.money - bond(t.who, t.other);
        case TroubleKind::Feud: return -bond(t.who, t.other) - bond(t.other, t.who) + (Of(state, t.other).place == r.place ? 200 : 0);
        case TroubleKind::Leave: return 300 * Rivals(state, t.who) - r.money;
        case TroubleKind::Death: return (IllNow(state, t.who) ? 2000 : 0) + WorstNeed(state, t.who);
        }
        return 0;
    }

    bool Happen(const TownDef &def, TownState &state, const Trouble &t)
    {
        if (!Possible(def, state, t))
            return false;
        ResidentState &r = state.residents[static_cast<std::size_t>(t.who)];
        Log(state, EventKind::Trouble, t.who, t.other, static_cast<int>(t.kind));
        switch (t.kind)
        {
        case TroubleKind::Ill:
            r.illUntil = state.tick + illDays * ticksPerDay;
            Remember(state, FactKind::Ill, t.who, -1, 0, true);
            break;
        case TroubleKind::Theft:
        {
            // A quarter of the victim's coins, but 10 to 60 (or all they have). Whoever is there sees it.
            ResidentState &victim = state.residents[static_cast<std::size_t>(t.other)];
            const int amount = std::min<int>(victim.money, std::clamp<int>(victim.money / 4, 10, 60));
            victim.money -= amount;
            r.money += amount;
            Remember(state, FactKind::Theft, t.who, t.other, amount, true);
            Learn(state, state.facts.back(), t.other); // the victim finds out wherever they are
            for (int i = 0; i < static_cast<int>(state.residents.size()); ++i)
                if (i != t.who && (state.facts.back().knows & Bit(i)))
                    SetBond(state, i, t.who, i == t.other ? -400 : -100);
            break;
        }
        case TroubleKind::Feud:
            SetBond(state, t.who, t.other, -350);
            SetBond(state, t.other, t.who, -350);
            Remember(state, FactKind::Feud, t.who, t.other, 0, true);
            Learn(state, state.facts.back(), t.other);
            break;
        case TroubleKind::Leave:
        case TroubleKind::Death:
            // Remembered first (they were there), then gone: their coins leave with them, and a
            // favour the player was on that needs them is open again (never asked while they are gone).
            Remember(state, t.kind == TroubleKind::Leave ? FactKind::Left : FactKind::Died, t.who, -1, 0, true);
            state.minted -= r.money;
            r.money = 0;
            r.gone = t.kind == TroubleKind::Leave ? Gone::Left : Gone::Dead;
            for (std::size_t f = 0; f < state.favours.size(); ++f)
                if (state.favours[f].status == FavourStatus::Asked && (def.favours[f].resident == t.who || def.favours[f].target == t.who))
                    state.favours[f] = {};
            break;
        }
        return true;
    }

    std::string Sentence(const TownDef &def, const Trouble &t)
    {
        static const FactKind kinds[] = {FactKind::Ill, FactKind::Theft, FactKind::Feud, FactKind::Left, FactKind::Died};
        return Sentence(def, Fact{0, kinds[static_cast<int>(t.kind)], static_cast<std::int16_t>(t.who), static_cast<std::int16_t>(t.other), 0, 0, 0, 0});
    }
}
