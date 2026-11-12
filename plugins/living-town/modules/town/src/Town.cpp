#include "TownInner.hpp"

#include <algorithm>

namespace umbra::town
{
    namespace
    {
        constexpr int needLimit = needFull * ticksPerDay; // needs are stored times ticksPerDay
        constexpr int xpPerLevel = 100;

        // xorshift64*: three shifts and a multiply, the same on every compiler.
        std::uint64_t Next(TownState &state)
        {
            state.random ^= state.random >> 12;
            state.random ^= state.random << 25;
            state.random ^= state.random >> 27;
            return state.random * 2685821657736338717ULL;
        }
        int Below(TownState &state, int count) { return static_cast<int>((Next(state) >> 33) % static_cast<std::uint64_t>(count)); }

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
    }

    namespace inner
    {
        void Log(TownState &state, EventKind kind, int who, int other, int what)
        {
            if (state.events.size() == TownState::maxEvents)
                state.events.erase(state.events.begin());
            state.events.push_back({state.tick, kind, static_cast<std::int16_t>(who), static_cast<std::int16_t>(other), what});
        }

        constexpr int keptRegard = 80, skippedRegard = -120;

        // News about the player changes what they think of the player: fully for the one who
        // asked, half for everybody else. Someone who is gone learns nothing.
        void Learn(TownState &state, Fact &fact, int resident)
        {
            if ((fact.knows & Bit(resident)) || state.residents[static_cast<std::size_t>(resident)].gone != Gone::No)
                return;
            fact.knows |= Bit(resident);
            if (fact.kind != FactKind::FavourKept && fact.kind != FactKind::FavourSkipped)
                return;
            const int change = (fact.kind == FactKind::FavourKept ? keptRegard : skippedRegard) / (resident == fact.who ? 1 : 2);
            std::int16_t &regard = state.regard[static_cast<std::size_t>(resident)];
            regard = static_cast<std::int16_t>(std::clamp(regard + change, -1000, 1000));
        }

        void Remember(TownState &state, FactKind kind, int who, int other, int what, bool seen)
        {
            if (state.facts.size() == TownState::maxFacts)
                state.facts.erase(state.facts.begin());
            Fact fact{state.tick, kind, static_cast<std::int16_t>(who), static_cast<std::int16_t>(other), what, 0, 0, 0};
            const int count = static_cast<int>(state.residents.size());
            for (int i = 0; i < count; ++i)
                if (i == who || (seen && state.residents[static_cast<std::size_t>(i)].place == state.residents[static_cast<std::size_t>(who)].place))
                    fact.saw |= Bit(i);
            state.facts.push_back(fact);
            for (int i = 0; i < count; ++i)
                if (fact.saw & Bit(i))
                    Learn(state, state.facts.back(), i);
        }

        // `teller` passes on the newest thing `listener` does not know yet (one per talk).
        void Tell(TownState &state, int teller, int listener)
        {
            for (std::size_t i = state.facts.size(); i-- > 0;)
                if ((state.facts[i].knows & Bit(teller)) && !(state.facts[i].knows & Bit(listener)))
                    return Learn(state, state.facts[i], listener);
        }

        void ChangeBond(const TownDef &def, TownState &state, int a, int b, int amount)
        {
            std::int16_t &bond = state.bonds[static_cast<std::size_t>(a) * def.residents.size() + static_cast<std::size_t>(b)];
            const int before = bond;
            bond = static_cast<std::int16_t>(std::clamp(before + amount, -1000, 1000));
            if (before < friendsAt && bond >= friendsAt)
            {
                Log(state, EventKind::Friends, a, b, bond);
                Remember(state, FactKind::Friends, a, b, bond, true);
            }
            if (before > rivalsAt && bond <= rivalsAt)
            {
                Log(state, EventKind::Rivals, a, b, bond);
                Remember(state, FactKind::Rivals, a, b, bond, true);
            }
        }
    }
    using namespace inner;

    TownState NewTown(const TownDef &def, std::uint64_t seed)
    {
        TownState state;
        state.seed = seed;
        state.random = seed * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL; // never 0
        if (state.random == 0)
            state.random = 1;
        state.residents.resize(def.residents.size());
        state.bonds.assign(def.residents.size() * def.residents.size(), 0);
        state.regard.assign(def.residents.size(), 0);
        state.favours.assign(def.favours.size(), {});
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
        if (r.gone != Gone::No)
            return Refusal::Gone;
        if (state.tick < r.until)
            return Refusal::Busy;
        if (state.tick < r.illUntil && a.where != "home")
            return Refusal::Ill; // the ill stay in
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
        int owner = def.places[static_cast<std::size_t>(place)].owner;
        if (owner >= 0 && state.residents[static_cast<std::size_t>(owner)].gone != Gone::No)
            owner = -1; // nobody keeps the place now: the coins leave the town
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
            Remember(state, FactKind::LevelUp, intent.resident, -1, level, true);
        }
        return Refusal::None;
    }

    void Tick(const TownDef &def, TownState &state, Decider decide, void *user)
    {
        const int count = static_cast<int>(state.residents.size());
        // 1. Whoever is free decides what to do next (in resident order: the order is part of the rules).
        for (int i = 0; i < count; ++i)
        {
            if (state.tick < state.residents[static_cast<std::size_t>(i)].until || state.residents[static_cast<std::size_t>(i)].gone != Gone::No)
                continue;
            const std::optional<Intent> asked = decide ? decide(def, state, i, user) : std::nullopt;
            if (!asked || asked->resident != i || Apply(def, state, *asked) != Refusal::None)
                Apply(def, state, Choose(def, state, i));
        }
        // 2. Needs grow, and what each resident is doing eases them.
        for (int i = 0; i < count; ++i)
        {
            ResidentState &r = state.residents[static_cast<std::size_t>(i)];
            if (r.gone != Gone::No)
                continue;
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
                    Remember(state, FactKind::Crisis, i, -1, static_cast<int>(n), true);
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
                if (!def.activities[static_cast<std::size_t>(r.activity)].social || r.gone != Gone::No)
                    continue;
                int partner = -1, seen = 0;
                for (int other = 0; other < count; ++other)
                {
                    const ResidentState &o = state.residents[static_cast<std::size_t>(other)];
                    if (other != i && o.gone == Gone::No && o.place == r.place && def.activities[static_cast<std::size_t>(o.activity)].social && Below(state, ++seen) == 0)
                        partner = other;
                }
                if (partner < 0)
                    continue;
                // Mostly they get on; one talk in six goes badly.
                const int change = Below(state, 6) == 0 ? -12 : 8;
                ChangeBond(def, state, i, partner, change);
                ChangeBond(def, state, partner, i, change / 2);
                // And they pass on news: one thing each way (rumours, Town.md).
                Tell(state, i, partner);
                Tell(state, partner, i);
            }
        // 4. A favour the player was asked and did not do in time: the one who asked knows.
        for (std::size_t f = 0; f < state.favours.size(); ++f)
            if (state.favours[f].status == FavourStatus::Asked && state.tick >= state.favours[f].due)
            {
                state.favours[f].status = FavourStatus::Skipped;
                Remember(state, FactKind::FavourSkipped, def.favours[f].resident, def.favours[f].target, static_cast<int>(f), false);
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
            number(r.place), number(r.activity), number(r.until), number(r.money), number(r.xp), number(r.level), number(r.crisis), number(r.illUntil), number(static_cast<int>(r.gone));
            for (const std::int32_t need : r.needs)
                number(need);
        }
        for (const std::int16_t bond : state.bonds)
            number(bond);
        for (const TownEvent &e : state.events)
            number(e.tick), number(static_cast<int>(e.kind)), number(e.who), number(e.other), number(e.what);
        for (const Fact &f : state.facts)
            number(f.tick), number(static_cast<int>(f.kind)), number(f.who), number(f.other), number(f.what), mix(f.saw), mix(f.knows), mix(f.told);
        for (const std::int16_t regard : state.regard)
            number(regard);
        for (const SaidLine &line : state.said)
        {
            number(line.tick), number(line.speaker);
            for (const char c : line.text)
                number(static_cast<unsigned char>(c));
        }
        for (const FavourState &f : state.favours)
            number(static_cast<int>(f.status)), number(f.due);
        return hash;
    }

    std::string Sentence(const TownDef &def, const Fact &fact)
    {
        const auto name = [&](int resident) { return resident >= 0 ? def.residents[static_cast<std::size_t>(resident)].name : std::string("someone"); };
        // "his", "her" or "their", from the resident's pronoun in the town file.
        const std::string &pronoun = def.residents[static_cast<std::size_t>(std::max<int>(fact.who, 0))].pronoun;
        const std::string their = pronoun == "he" ? "his" : pronoun == "she" ? "her" : "their";
        switch (fact.kind)
        {
        case FactKind::Ill: return name(fact.who) + " has taken ill";
        case FactKind::Theft: return name(fact.who) + " stole coins from " + name(fact.other);
        case FactKind::Feud: return name(fact.who) + " and " + name(fact.other) + " had a row in front of everyone";
        case FactKind::Left: return name(fact.who) + " has left town for good";
        case FactKind::Died: return name(fact.who) + " has died";
        case FactKind::LevelUp: return name(fact.who) + " has grown better at " + their + " work (level " + std::to_string(fact.what) + ")";
        case FactKind::Crisis: return name(fact.who) + " was in a bad way: " + def.needs[static_cast<std::size_t>(fact.what)].name;
        case FactKind::Friends: return name(fact.who) + " now counts " + name(fact.other) + " as a friend";
        case FactKind::Rivals: return name(fact.who) + " has fallen out with " + name(fact.other);
        case FactKind::FavourKept: return "You kept your word to " + name(fact.who) + " and went to see " + name(fact.other);
        case FactKind::FavourSkipped: return "You let " + name(fact.who) + " down: you never went to see " + name(fact.other);
        }
        return {};
    }

    std::string Says(const TownDef &def, const Fact &fact, int resident)
    {
        const auto name = [&](int other) { return other >= 0 ? def.residents[static_cast<std::size_t>(other)].name : std::string("someone"); };
        // 1. About themselves, in their own words.
        if (fact.who == resident)
            switch (fact.kind)
            {
            case FactKind::LevelUp: return "I'm getting better at my work, if I say so myself.";
            case FactKind::Crisis: return "I was in a bad way the other day. " + def.needs[static_cast<std::size_t>(fact.what)].name + ".";
            case FactKind::Friends: return name(fact.other) + " and I get on well these days.";
            case FactKind::Rivals: return "I've had enough of " + name(fact.other) + ".";
            case FactKind::FavourKept: return "You went to see " + name(fact.other) + ", as I asked. Thank you.";
            case FactKind::FavourSkipped: return "I asked you one thing: to look in on " + name(fact.other) + ". You never went.";
            case FactKind::Ill: return "I've been unwell. Don't come too close.";
            case FactKind::Theft: return "I took what I needed from " + name(fact.other) + ". Leave it be.";
            case FactKind::Feud: return name(fact.other) + " and I had words. I meant every one.";
            case FactKind::Left:
            case FactKind::Died: break; // they are not here to say it
            }
        if (fact.other == resident && fact.kind == FactKind::Theft)
            return name(fact.who) + " robbed me. My own neighbour.";
        if (fact.other == resident && fact.kind == FactKind::Feud)
            return name(fact.who) + " and I had words. I meant every one.";
        // 2. About others: seen with their own eyes, or heard.
        std::string text = Sentence(def, fact);
        if (fact.kind == FactKind::FavourKept || fact.kind == FactKind::FavourSkipped)
            text[0] = 'y'; // "you kept..." after the lead-in
        return ((fact.saw & Bit(resident)) ? "I saw it myself: " : "They say ") + text + ".";
    }

    namespace
    {
        bool AboutPlayer(const Fact &fact) { return fact.kind == FactKind::FavourKept || fact.kind == FactKind::FavourSkipped; }

        // The news a resident tells next: the newest they know and have not told; about the player first.
        int News(const TownState &state, int resident)
        {
            int news = -1;
            for (std::size_t i = 0; i < state.facts.size(); ++i)
                if ((state.facts[i].knows & Bit(resident)) && !(state.facts[i].told & Bit(resident)) &&
                    (news < 0 || AboutPlayer(state.facts[i]) || !AboutPlayer(state.facts[static_cast<std::size_t>(news)])))
                    news = static_cast<int>(i);
            return news;
        }
    }

    int NextNews(const TownDef &def, const TownState &state, int resident)
    {
        if (resident < 0 || static_cast<std::size_t>(resident) >= state.residents.size() || state.residents[static_cast<std::size_t>(resident)].gone != Gone::No)
            return -1;
        for (std::size_t f = 0; f < state.favours.size(); ++f)
            if ((state.favours[f].status == FavourStatus::Asked && def.favours[f].target == resident) ||
                (state.favours[f].status == FavourStatus::Open && def.favours[f].resident == resident && state.regard[static_cast<std::size_t>(resident)] > -200 &&
                 state.residents[static_cast<std::size_t>(def.favours[f].target)].gone == Gone::No))
                return -1;
        return News(state, resident);
    }

    std::string TalkTo(const TownDef &def, TownState &state, int resident, const std::string &worded)
    {
        if (resident < 0 || static_cast<std::size_t>(resident) >= state.residents.size() || state.residents[static_cast<std::size_t>(resident)].gone != Gone::No)
            return {};
        const ResidentDef &who = def.residents[static_cast<std::size_t>(resident)];
        // 1. The player was sent here: the favour is kept. Those here see it; the asker hears in time.
        for (std::size_t f = 0; f < state.favours.size(); ++f)
            if (state.favours[f].status == FavourStatus::Asked && def.favours[f].target == resident)
            {
                state.favours[f].status = FavourStatus::Kept;
                Remember(state, FactKind::FavourKept, def.favours[f].resident, resident, static_cast<int>(f), false);
                Fact &fact = state.facts.back();
                fact.saw |= Bit(resident);
                Learn(state, fact, resident);
                fact.told |= Bit(resident); // the player was there: it is in their chronicle
                return who.name + ": " + def.favours[f].thanks;
            }
        // 2. This resident has something to ask (not of someone they think badly of).
        for (std::size_t f = 0; f < state.favours.size(); ++f)
            if (state.favours[f].status == FavourStatus::Open && def.favours[f].resident == resident && state.regard[static_cast<std::size_t>(resident)] > -200 &&
                state.residents[static_cast<std::size_t>(def.favours[f].target)].gone == Gone::No)
            {
                state.favours[f] = {FavourStatus::Asked, state.tick + def.favours[f].hours * ticksPerHour};
                return who.name + ": " + def.favours[f].ask;
            }
        // 3. The newest thing they know and have not told the player; news about the player first.
        if (const int news = News(state, resident); news >= 0)
        {
            Fact &fact = state.facts[static_cast<std::size_t>(news)];
            fact.told |= Bit(resident);
            // Kept as it was said, for the chronicle.
            if (state.said.size() == TownState::maxSaid)
                state.said.erase(state.said.begin());
            state.said.push_back({state.tick, static_cast<std::int16_t>(resident), worded.empty() ? Says(def, fact, resident) : worded});
            return who.name + ": " + state.said.back().text;
        }
        // 4. Nothing new: a greeting that shows what they think of the player, and what they are doing.
        const int regard = state.regard[static_cast<std::size_t>(resident)];
        const std::string doing = def.activities[static_cast<std::size_t>(state.residents[static_cast<std::size_t>(resident)].activity)].name;
        return who.name + ": " + (regard >= 60 ? "Good to see you! " : regard <= -60 ? "Oh. It's you. " : "Hello. ") + "I'm " + doing + ".";
    }

    std::vector<std::string> Chronicle(const TownDef &def, const TownState &state)
    {
        // Two lists in time order, merged: what the player did, and what people said.
        std::vector<std::pair<std::int32_t, std::string>> entries;
        for (const Fact &fact : state.facts)
            if (fact.told != 0 && AboutPlayer(fact))
                entries.emplace_back(fact.tick, Sentence(def, fact) + ".");
        for (const SaidLine &line : state.said)
            entries.emplace_back(line.tick, def.residents[static_cast<std::size_t>(line.speaker)].name + ": \"" + line.text + "\"");
        std::stable_sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
        std::vector<std::string> lines;
        for (const auto &[tick, text] : entries)
            lines.push_back("Day " + std::to_string(tick / ticksPerDay + 1) + ": " + text);
        return lines;
    }
}
