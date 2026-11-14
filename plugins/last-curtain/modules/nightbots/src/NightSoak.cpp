#include <NightSoak.hpp>

#include <NightBots.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <climits>
#include <sstream>

namespace umbra::night
{
    namespace
    {
        std::size_t U(int i) { return static_cast<std::size_t>(i); }

        // Guest 0's chooser under a test strategy; the bot otherwise.
        Action Strategy(const NightDef &def, const NightState &state, int g, int strategy)
        {
            const std::vector<Action> legal = LegalActions(def, state);
            auto has = [&](const Action &a) { return std::find(legal.begin(), legal.end(), a) != legal.end(); };
            if (g != 0 || state.battle.active || legal.empty())
                return ChooseAction(def, state, g);
            if (legal.front().kind == ActionKind::TakeOmen && (strategy == 2 || strategy == 3))
                return {strategy == 3 ? ActionKind::TakeOmen : ActionKind::LeaveOmen};
            if (strategy != 1 || state.taken.present || legal.front().kind == ActionKind::LevelUp)
                return ChooseAction(def, state, g);
            // 1: attack anyone in the room, else walk toward the nearest guest.
            const int at = state.guests[0].cell;
            int target = -1, best = INT_MAX;
            for (int o = 1; o < state.count; ++o)
            {
                if (has({ActionKind::Attack, -1, static_cast<std::int8_t>(o)}))
                    return {ActionKind::Attack, -1, static_cast<std::int8_t>(o)};
                const int n = Steps(def, state, at, state.guests[U(o)].cell);
                if (!state.guests[U(o)].down && n > 0 && n < best)
                    target = state.guests[U(o)].cell, best = n;
            }
            for (const Action &a : legal)
                if (a.kind == ActionKind::Move && a.a < 4 && Passage(def, state, at, a.a) && target >= 0 &&
                    Steps(def, state, Neighbour(def, at, a.a), target) < best)
                    return a;
            return ChooseAction(def, state, g);
        }
    }

    bool SoakReport::Holds(const NightDef &def) const
    {
        if (faults > 0)
            return false;
        for (const SoakForm &f : forms)
            if (f.nights >= 50)
            {
                const int share = f.companyWins * 100 / f.nights;
                if (share < def.rules.balance.companyWinMin || share > def.rules.balance.companyWinMax)
                    return false;
            }
        return true;
    }

    SoakReport Soak(const NightDef &def, std::uint64_t firstSeed, int nights, int strategy)
    {
        SoakReport r;
        for (int n = 0; n < nights; ++n)
        {
            NightState state = NewNight(def, firstSeed + static_cast<std::uint64_t>(n), 0);
            bool turned = false;
            int applies = 0;
            ++r.nights;
            // 1. Play the night, counting what happens from the log.
            while (Current(state) >= 0 && applies < 5000)
            {
                const std::size_t since = state.logCount;
                if (!Apply(def, state, Strategy(def, state, Current(state), strategy)))
                    break;
                ++applies;
                for (const LogEntry &e : NewLog(state, since))
                {
                    auto count = [](bool yes) { return yes ? 1 : 0; };
                    r.rooms += count(e.kind == LogKind::Opened);
                    r.monsterKills += count(e.kind == LogKind::Monster && e.other == 1);
                    r.levels += count(e.kind == LogKind::Level);
                    r.downs += count(e.kind == LogKind::Down);
                    r.deaths += count(e.kind == LogKind::Died && !IsTaken(state, e.who));
                    r.guestAttacks += count(e.kind == LogKind::Battle && !turned && e.who >= 0 && e.other >= 0);
                    turned = turned || e.kind == LogKind::Turned;
                }
            }
            // 2. The outcome, by the form that was marked.
            const Outcome outcome = Result(state);
            if (outcome == Outcome::Running || state.marked < 0)
            {
                ++r.faults;
                continue;
            }
            r.omens += state.omensTaken;
            ++r.marked[U(state.marked)];
            SoakForm &form = r.forms[U(state.marked)];
            if (outcome == Outcome::CompanyEarly)
                ++r.earlyWins, ++form.earlyWins;
            else
            {
                ++form.nights;
                if (outcome == Outcome::Company)
                    ++r.companyWins, ++form.companyWins;
                else
                    ++r.takenWins;
            }
            const bool guest0Taken = state.marked == 0 && state.taken.present;
            r.playerWins += (guest0Taken ? outcome == Outcome::Taken : outcome != Outcome::Taken) ? 1 : 0;
        }
        return r;
    }

    std::string Text(const NightDef &def, const SoakReport &r)
    {
        std::ostringstream out;
        auto share = [](long long part, long long whole) { return whole ? part * 100 / whole : 0; };
        auto each = [&](long long total) { // per night, one decimal, whole-number arithmetic
            const long long tenths = total * 10 / std::max(1, r.nights - r.faults);
            return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
        };
        const int done = std::max(1, r.nights - r.faults);
        out << r.nights << " nights, " << r.faults << " faults\n"
            << "won before midnight " << r.earlyWins << " (" << share(r.earlyWins, done) << "%), company won the hunt " << r.companyWins
            << ", the Taken won " << r.takenWins << "; guest 0's side won " << share(r.playerWins, done) << "%\n"
            << "per night: rooms " << each(r.rooms) << ", omens " << each(r.omens) << ", monster kills " << each(r.monsterKills) << ", levels "
            << each(r.levels) << ", downs " << each(r.downs) << ", deaths " << each(r.deaths) << ", guest attacks " << each(r.guestAttacks) << "\n"
            << "the hunt, company wins by form (band " << def.rules.balance.companyWinMin << "-" << def.rules.balance.companyWinMax << "%):\n";
        for (std::size_t g = 0; g < def.guests.size(); ++g)
        {
            const SoakForm &f = r.forms[g];
            const PowerDef &p = def.powers[U(def.guests[g].power)];
            out << "  " << def.guests[g].taken << " (" << p.id << "): " << share(f.companyWins, f.nights) << "% of " << f.nights << " hunts; marked "
                << r.marked[g] << ", brought down early " << f.earlyWins << "\n";
        }
        return out.str();
    }

    nlohmann::json Json(const NightDef &def, const SoakReport &r)
    {
        nlohmann::json forms = nlohmann::json::array();
        for (std::size_t g = 0; g < def.guests.size(); ++g)
            forms.push_back({{"guest", def.guests[g].id}, {"form", def.guests[g].taken}, {"hunts", r.forms[g].nights},
                             {"companyWins", r.forms[g].companyWins}, {"earlyWins", r.forms[g].earlyWins}, {"marked", r.marked[g]}});
        return {{"nights", r.nights}, {"faults", r.faults}, {"earlyWins", r.earlyWins}, {"companyWins", r.companyWins}, {"takenWins", r.takenWins},
                {"playerWins", r.playerWins}, {"rooms", r.rooms}, {"omens", r.omens}, {"monsterKills", r.monsterKills}, {"levels", r.levels},
                {"downs", r.downs}, {"deaths", r.deaths}, {"guestAttacks", r.guestAttacks}, {"forms", forms}, {"holds", r.Holds(def)}};
    }
}
