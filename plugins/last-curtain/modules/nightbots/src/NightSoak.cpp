#include <NightSoak.hpp>

#include <NightBots.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <sstream>

namespace umbra::night
{
    namespace
    {
        std::size_t U(int i) { return static_cast<std::size_t>(i); }
    }

    bool SoakReport::Holds(const NightDef &def) const
    {
        const auto &b = def.rules.balance;
        const int done = runs - faults;
        if (faults > 0 || done <= 0)
            return false;
        // Whole-number bands: nights per run, any one winner's share, the house's share, deaths per night.
        const bool nightsOk = nights >= static_cast<long long>(b.nightsMin) * done && nights <= static_cast<long long>(b.nightsMax) * done;
        const bool spread = std::all_of(wins.begin(), wins.end(), [&](int w) { return w * 100 <= b.winnerMax * done; });
        return nightsOk && spread && houseWins * 100 <= b.houseMax * done && deaths >= static_cast<long long>(b.deathsPerNightMin) * nights;
    }

    SoakReport Soak(const NightDef &def, std::uint64_t firstSeed, int runs, int strategy)
    {
        SoakReport r;
        for (int n = 0; n < runs; ++n)
        {
            NightState state = NewNight(def, firstSeed + static_cast<std::uint64_t>(n), 0);
            int applies = 0;
            ++r.runs;
            // 1. Play the run, counting what happens from the log.
            while (Current(state) >= 0 && applies < 40000)
            {
                const std::size_t since = state.logCount;
                if (!Apply(def, state, ChooseAction(def, state, Current(state), strategy != 1)))
                    break;
                ++applies;
                for (const LogEntry &e : NewLog(state, since))
                {
                    const bool living = e.kind == LogKind::Died && e.a == 1;
                    r.deaths += living ? 1 : 0;
                    if (living)
                        (e.other < 0 ? r.byHouse : state.guests[U(e.other)].ghost ? r.byGhosts : r.byGuests) += 1;
                    r.monsterKills += e.kind == LogKind::Monster && e.other == 1 ? 1 : 0;
                    r.levels += e.kind == LogKind::Level ? 1 : 0;
                }
            }
            // 2. The outcome.
            if (!state.over)
            {
                ++r.faults;
                continue;
            }
            r.nights += state.night;
            if (state.outcome == Outcome::Escaped && state.winner >= 0)
                ++r.escapes, ++r.wins[U(state.winner)];
            else
                ++r.houseWins;
        }
        return r;
    }

    std::string Text(const NightDef &def, const SoakReport &r)
    {
        std::ostringstream out;
        const long long done = std::max(1, r.runs - r.faults), nights = std::max(1LL, r.nights);
        auto share = [](long long part, long long whole) { return whole ? part * 100 / whole : 0; };
        auto tenths = [](long long total, long long by) { const long long t = total * 10 / by; return std::to_string(t / 10) + "." + std::to_string(t % 10); };
        out << r.runs << " runs, " << r.faults << " faults; nights per run " << tenths(r.nights, done) << " (band " << def.rules.balance.nightsMin << "-"
            << def.rules.balance.nightsMax << ")\n"
            << "escaped " << share(r.escapes, done) << "%, the house won " << share(r.houseWins, done) << "% (at most " << def.rules.balance.houseMax << "%)\n"
            << "per night: deaths " << tenths(r.deaths, nights) << " (by guests " << tenths(r.byGuests, nights) << ", ghosts " << tenths(r.byGhosts, nights)
            << ", the house " << tenths(r.byHouse, nights) << "), monster kills " << tenths(r.monsterKills, nights) << ", levels " << tenths(r.levels, nights) << "\n"
            << "winners (at most " << def.rules.balance.winnerMax << "% each):\n";
        for (std::size_t g = 0; g < def.guests.size(); ++g)
            out << "  " << def.guests[g].id << ": " << share(r.wins[g], done) << "%\n";
        return out.str();
    }

    nlohmann::json Json(const NightDef &, const SoakReport &r)
    {
        return {{"runs", r.runs}, {"faults", r.faults}, {"escapes", r.escapes}, {"houseWins", r.houseWins}, {"wins", r.wins}, {"nights", r.nights},
                {"deaths", r.deaths}, {"byGuests", r.byGuests}, {"byGhosts", r.byGhosts}, {"byHouse", r.byHouse}, {"monsterKills", r.monsterKills},
                {"levels", r.levels}};
    }
}
