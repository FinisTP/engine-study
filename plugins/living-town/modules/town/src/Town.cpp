#include "Town.hpp"

#include <algorithm>

namespace umbra::town
{
    namespace
    {
        constexpr int needLimit = needFull * ticksPerDay; // needs are stored times ticksPerDay
        constexpr int xpPerLevel = 100;
        constexpr int friendsAt = 300, rivalsAt = -300;

        // xorshift64*: three shifts and a multiply, the same on every compiler.
        std::uint64_t Next(TownState &state)
        {
            state.random ^= state.random >> 12;
            state.random ^= state.random << 25;
            state.random ^= state.random >> 27;
            return state.random * 2685821657736338717ULL;
        }
        int Below(TownState &state, int count) { return static_cast<int>((Next(state) >> 33) % static_cast<std::uint64_t>(count)); }

        void Log(TownState &state, EventKind kind, int who, int other, int what)
        {
            if (state.events.size() == TownState::maxEvents)
                state.events.erase(state.events.begin());
            state.events.push_back({state.tick, kind, static_cast<std::int16_t>(who), static_cast<std::int16_t>(other), what});
        }

        int PlaceOf(const TownDef &def, int resident, const ActivityDef &activity)
        {
            const ResidentDef &r = def.residents[static_cast<std::size_t>(resident)];
            return activity.where == "home" ? r.home : activity.where == "work" ? r.work : activity.place;
        }

        bool OpenAt(const ActivityDef &a, int hour)
        {
            return a.openFrom <= a.openTo ? hour >= a.openFrom && hour < a.openTo : hour >= a.openFrom || hour < a.openTo;
        }

        // Ticks an activity started now would last: its hours, or less if its closing time comes first.
        int Length(const ActivityDef &a, const TownState &state)
        {
            const int full = a.hours * ticksPerHour;
            if (a.openFrom == 0 && a.openTo == 24)
                return full;
            const int now = state.tick % ticksPerDay, closing = a.openTo * ticksPerHour;
            const int left = closing > now ? closing - now : closing + ticksPerDay - now;
            return std::min(full, left);
        }
        int HoursOf(int ticks) { return (ticks + ticksPerHour - 1) / ticksPerHour; } // started hours are paid in full

        void ChangeBond(const TownDef &def, TownState &state, int a, int b, int amount)
        {
            std::int16_t &bond = state.bonds[static_cast<std::size_t>(a) * def.residents.size() + static_cast<std::size_t>(b)];
            const int before = bond;
            bond = static_cast<std::int16_t>(std::clamp(before + amount, -1000, 1000));
            if (before < friendsAt && bond >= friendsAt)
                Log(state, EventKind::Friends, a, b, bond);
            if (before > rivalsAt && bond <= rivalsAt)
                Log(state, EventKind::Rivals, a, b, bond);
        }
    }

    TownState NewTown(const TownDef &def, std::uint64_t seed)
    {
        TownState state;
        state.seed = seed;
        state.random = seed * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL; // never 0
        if (state.random == 0)
            state.random = 1;
        state.residents.resize(def.residents.size());
        state.bonds.assign(def.residents.size() * def.residents.size(), 0);
        for (std::size_t i = 0; i < def.residents.size(); ++i)
        {
            ResidentState &r = state.residents[i];
            r.place = static_cast<std::int16_t>(def.residents[i].home);
            r.money = def.residents[i].money;
            // People start the day a little different: 0..299 on every need.
            for (std::size_t n = 0; n < def.needs.size(); ++n)
                r.needs[n] = Below(state, 300) * ticksPerDay;
            for (const auto &[other, value] : def.residents[i].bonds)
                state.bonds[i * def.residents.size() + static_cast<std::size_t>(other)] = static_cast<std::int16_t>(value);
        }
        return state;
    }

    Refusal Legal(const TownDef &def, const TownState &state, const Intent &intent)
    {
        if (intent.resident < 0 || static_cast<std::size_t>(intent.resident) >= state.residents.size())
            return Refusal::UnknownResident;
        if (intent.activity < 0 || static_cast<std::size_t>(intent.activity) >= def.activities.size())
            return Refusal::UnknownActivity;
        const ResidentState &r = state.residents[static_cast<std::size_t>(intent.resident)];
        const ActivityDef &a = def.activities[static_cast<std::size_t>(intent.activity)];
        if (state.tick < r.until)
            return Refusal::Busy;
        if (!OpenAt(a, state.Hour()))
            return Refusal::Closed;
        // Paying yourself (the owner eating at their own inn) costs nothing.
        const bool own = def.places[static_cast<std::size_t>(PlaceOf(def, intent.resident, a))].owner == intent.resident;
        return !own && r.money < a.cost * HoursOf(Length(a, state)) ? Refusal::CannotPay : Refusal::None;
    }

    std::vector<Intent> LegalIntents(const TownDef &def, const TownState &state, int resident)
    {
        std::vector<Intent> legal;
        for (int a = 0; a < static_cast<int>(def.activities.size()); ++a)
            if (Legal(def, state, {resident, a}) == Refusal::None)
                legal.push_back({resident, a});
        return legal;
    }

    Intent Choose(const TownDef &def, TownState &state, int resident)
    {
        const ResidentState &r = state.residents[static_cast<std::size_t>(resident)];
        // 1. The plan for this hour, if it can be done now; else rest at home (always possible).
        int plan = 0;
        for (const ScheduleEntry &entry : def.residents[static_cast<std::size_t>(resident)].schedule)
            if ((entry.from <= entry.to ? state.Hour() >= entry.from && state.Hour() < entry.to : state.Hour() >= entry.from || state.Hour() < entry.to) &&
                Legal(def, state, {resident, entry.activity}) == Refusal::None)
            {
                plan = entry.activity;
                break;
            }
        // 2. The most pressing need: as it is now, and as it would be when the plan ends (nobody
        //    goes to bed for seven hours on an empty stomach).
        const ActivityDef &planned = def.activities[static_cast<std::size_t>(plan)];
        const int ticks = Length(planned, state);
        std::size_t worst = 0;
        int worstLater = -1;
        for (std::size_t n = 0; n < def.needs.size(); ++n)
        {
            const int later = r.needs[n] + ticks * (def.needs[n].risePerDay - planned.relief[n] * (ticksPerDay / ticksPerHour));
            if (std::max(r.needs[n], later) > worstLater)
                worst = n, worstLater = std::max(r.needs[n], later);
        }
        if (r.needs[worst] >= def.urgent * ticksPerDay || worstLater >= needLimit)
        {
            // The legal activity that eases it most per coin.
            int best = -1, bestScore = 0, ties = 0;
            for (const Intent &intent : LegalIntents(def, state, resident))
            {
                const ActivityDef &a = def.activities[static_cast<std::size_t>(intent.activity)];
                const int score = a.relief[worst] * 100 / (a.cost + 10);
                if (score <= 0 || score < bestScore)
                    continue;
                // Equal scores: each has the same chance (reservoir pick with the town's own dice).
                ties = score == bestScore ? ties + 1 : 1;
                if (score > bestScore || Below(state, ties) == 0)
                    best = intent.activity;
                bestScore = score;
            }
            if (best >= 0)
                return {resident, best};
        }
        return {resident, plan};
    }

    Refusal Apply(const TownDef &def, TownState &state, const Intent &intent)
    {
        const Refusal refusal = Legal(def, state, intent);
        if (refusal != Refusal::None)
            return refusal;
        ResidentState &r = state.residents[static_cast<std::size_t>(intent.resident)];
        const ActivityDef &a = def.activities[static_cast<std::size_t>(intent.activity)];
        const int place = PlaceOf(def, intent.resident, a);
        const int owner = def.places[static_cast<std::size_t>(place)].owner;
        // 1. Go there and start (logged only when it is a change, so resting all night is one line).
        if (r.activity != intent.activity || r.place != place)
            Log(state, EventKind::Started, intent.resident, -1, intent.activity);
        r.place = static_cast<std::int16_t>(place);
        r.activity = static_cast<std::int16_t>(intent.activity);
        const int ticks = Length(a, state), hours = HoursOf(ticks);
        r.until = state.tick + ticks;
        // 2. Pay for the whole stay now: to the owner, or out of the town if nobody owns the place.
        if (const int cost = a.cost * hours; cost > 0 && owner != intent.resident)
        {
            r.money -= cost;
            if (owner >= 0)
            {
                state.residents[static_cast<std::size_t>(owner)].money += cost;
                ChangeBond(def, state, owner, intent.resident, 3); // a customer is liked a little
            }
            else
                state.minted -= cost;
            Log(state, EventKind::Paid, intent.resident, owner, cost);
        }
        // 3. Earn and learn.
        if (const int wage = a.wage * hours; wage > 0)
        {
            r.money += wage;
            state.minted += wage;
            Log(state, EventKind::Earned, intent.resident, -1, wage);
        }
        r.xp += a.xp * hours;
        if (const int level = 1 + r.xp / xpPerLevel; level > r.level)
        {
            r.level = static_cast<std::int16_t>(level);
            Log(state, EventKind::LevelUp, intent.resident, -1, level);
        }
        return Refusal::None;
    }

    void Tick(const TownDef &def, TownState &state, Decider decide, void *user)
    {
        const int count = static_cast<int>(state.residents.size());
        // 1. Whoever is free decides what to do next (in resident order: the order is part of the rules).
        for (int i = 0; i < count; ++i)
        {
            if (state.tick < state.residents[static_cast<std::size_t>(i)].until)
                continue;
            const std::optional<Intent> asked = decide ? decide(def, state, i, user) : std::nullopt;
            if (!asked || asked->resident != i || Apply(def, state, *asked) != Refusal::None)
                Apply(def, state, Choose(def, state, i));
        }
        // 2. Needs grow, and what each resident is doing eases them.
        for (int i = 0; i < count; ++i)
        {
            ResidentState &r = state.residents[static_cast<std::size_t>(i)];
            const ActivityDef &a = def.activities[static_cast<std::size_t>(r.activity)];
            for (std::size_t n = 0; n < def.needs.size(); ++n)
            {
                // Stored times 144: a day's rise is added in 144 whole steps; an hour's relief in 6.
                r.needs[n] = std::clamp(r.needs[n] + def.needs[n].risePerDay - a.relief[n] * (ticksPerDay / ticksPerHour), 0, needLimit);
                const std::uint16_t bit = static_cast<std::uint16_t>(1U << n);
                if (r.needs[n] == needLimit && !(r.crisis & bit))
                {
                    r.crisis = static_cast<std::uint16_t>(r.crisis | bit);
                    Log(state, EventKind::Crisis, i, -1, static_cast<int>(n));
                }
                else if (r.needs[n] < def.urgent * ticksPerDay)
                    r.crisis = static_cast<std::uint16_t>(r.crisis & ~bit);
            }
        }
        // 3. On the hour, everyone doing something social talks to one other person doing the same there.
        if (state.tick % ticksPerHour == ticksPerHour - 1)
            for (int i = 0; i < count; ++i)
            {
                const ResidentState &r = state.residents[static_cast<std::size_t>(i)];
                if (!def.activities[static_cast<std::size_t>(r.activity)].social)
                    continue;
                int partner = -1, seen = 0;
                for (int other = 0; other < count; ++other)
                {
                    const ResidentState &o = state.residents[static_cast<std::size_t>(other)];
                    if (other != i && o.place == r.place && def.activities[static_cast<std::size_t>(o.activity)].social && Below(state, ++seen) == 0)
                        partner = other;
                }
                if (partner < 0)
                    continue;
                // Mostly they get on; one talk in six goes badly.
                const int change = Below(state, 6) == 0 ? -12 : 8;
                ChangeBond(def, state, i, partner, change);
                ChangeBond(def, state, partner, i, change / 2);
            }
        ++state.tick;
    }

    void Run(const TownDef &def, TownState &state, int ticks)
    {
        for (int i = 0; i < ticks; ++i)
            Tick(def, state);
    }

    std::uint64_t Fingerprint(const TownState &state)
    {
        std::uint64_t hash = 14695981039346656037ULL;
        const auto mix = [&](std::uint64_t value) {
            for (int byte = 0; byte < 8; ++byte)
                hash = (hash ^ ((value >> (byte * 8)) & 0xFF)) * 1099511628211ULL;
        };
        const auto number = [&](long long value) { mix(static_cast<std::uint64_t>(value)); };
        mix(state.seed), mix(state.random), number(state.tick), number(state.minted);
        for (const ResidentState &r : state.residents)
        {
            number(r.place), number(r.activity), number(r.until), number(r.money), number(r.xp), number(r.level), number(r.crisis);
            for (const std::int32_t need : r.needs)
                number(need);
        }
        for (const std::int16_t bond : state.bonds)
            number(bond);
        for (const TownEvent &e : state.events)
            number(e.tick), number(static_cast<int>(e.kind)), number(e.who), number(e.other), number(e.what);
        return hash;
    }
}
