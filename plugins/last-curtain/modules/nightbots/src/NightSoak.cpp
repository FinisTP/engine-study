#include <NightSoak.hpp>

#include <NightBots.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <bit>
#include <climits>
#include <sstream>

namespace umbra::night
{
    namespace
    {
        std::size_t U(int i) { return static_cast<std::size_t>(i); }

        // The chooser under a test strategy: 1 the company leaves minions alone.
        Action Strategy(const NightDef &def, const NightState &state, int g, int strategy)
        {
            return ChooseAction(def, state, g, strategy != 1);
        }
    }

    bool SoakReport::Holds(const NightDef &def) const
    {
        const auto &b = def.rules.balance;
        auto in = [](long long part, long long whole, int min, int max) { return part * 100 >= min * whole && part * 100 <= max * whole; };
        if (faults > 0 || nights == 0 || allFinished * 100 > b.allFinishedMax * nights)
            return false;
        for (const SoakForm &f : forms)
            if (f.nights >= 50 && !in(f.companyWins, f.nights, b.companyWinMin, b.companyWinMax))
                return false;
        for (std::size_t g = 0; g < def.guests.size(); ++g)
            if (!in(marked[g], nights, b.takenMin, b.takenMax))
                return false;
        for (std::size_t i = 0; i < goalsDrawn.size(); ++i) // the player's seat (guest 0) is not a bot's: not banded
            if (def.goals[i].guest != 0 && goalsDrawn[i] >= 50 && !in(goalsDone[i], goalsDrawn[i], b.goalMin, b.goalMax))
                return false;
        return in(roomsByMidnight, nights - faults, b.roomsMin * 100, b.roomsMax * 100) &&
               roundsWithBotChat * 100 >= b.botChatRoundsMin * eveningRounds;
    }

    SoakReport Soak(const NightDef &def, std::uint64_t firstSeed, int nights, int strategy)
    {
        SoakReport r;
        r.goalsDrawn.assign(def.goals.size(), 0);
        r.goalsDone.assign(def.goals.size(), 0);
        for (int n = 0; n < nights; ++n)
        {
            NightState state = NewNight(def, firstSeed + static_cast<std::uint64_t>(n), 0);
            bool turned = false;
            std::uint64_t rounds = 0, chatRounds = 0; // bits: evening rounds seen, those with a chat between bots
            int applies = 0;
            ++r.nights;
            // 1. Play the night, counting what happens from the log.
            while (Current(state) >= 0 && applies < 5000)
            {
                const std::size_t since = state.logCount;
                if (!state.taken.present && state.round < 64)
                    rounds |= 1ull << state.round;
                if (!Apply(def, state, Strategy(def, state, Current(state), strategy)))
                    break;
                ++applies;
                for (const LogEntry &e : NewLog(state, since))
                {
                    auto count = [](bool yes) { return yes ? 1 : 0; };
                    r.rooms += count(e.kind == LogKind::Opened);
                    if (e.kind == LogKind::Persuade && e.who != state.player && e.other != state.player && state.round < 64 && !turned)
                        chatRounds |= 1ull << state.round;
                    if (e.kind == LogKind::Goal && state.guests[U(e.who)].goal >= 0)
                        ++r.goalsDrawn[U(state.guests[U(e.who)].goal)], r.goalsDone[U(state.guests[U(e.who)].goal)] += count(e.a != 0);
                    const bool kill = e.kind == LogKind::Monster && e.other == 1;
                    r.monsterKills += count(kill && def.monsters[U(e.a)].taken < 0);
                    r.minionKills += count(kill && def.monsters[U(e.a)].taken >= 0);
                    r.levels += count(e.kind == LogKind::Level);
                    r.downs += count(e.kind == LogKind::Down);
                    r.deaths += count(e.kind == LogKind::Died && !IsTaken(state, e.who));
                    if (e.kind == LogKind::Turned && !turned)
                        r.roomsByMidnight += state.placed; // the foyer and the start rooms too
                    turned = turned || e.kind == LogKind::Turned;
                }
            }
            r.eveningRounds += std::popcount(rounds);
            r.roundsWithBotChat += std::popcount(chatRounds);
            // 2. The outcome, by the form that was marked.
            const Outcome outcome = Result(state);
            if (outcome == Outcome::Company && !turned)
            {
                r.roomsByMidnight += state.placed;
                ++r.allFinished;
                continue;
            }
            if (outcome == Outcome::Running || !turned)
            {
                ++r.faults;
                continue;
            }
            ++r.marked[U(state.marked)];
            SoakForm &form = r.forms[U(state.marked)];
            ++form.nights;
            if (outcome == Outcome::Company)
                ++r.companyWins, ++form.companyWins;
            else
                ++r.takenWins;
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
        out << r.nights << " nights, " << r.faults << " faults, " << r.allFinished << " all finished (no hunt)\n"
            << "company won the hunt " << r.companyWins << " (" << share(r.companyWins, done) << "%), the Taken won " << r.takenWins
            << "; guest 0's side won " << share(r.playerWins, done) << "%\n"
            << "per night: rooms " << each(r.rooms) << ", monster kills " << each(r.monsterKills) << ", minion kills "
            << each(r.minionKills) << ", levels " << each(r.levels) << ", downs " << each(r.downs) << ", deaths " << each(r.deaths) << "\n"
            << "the hunt, company wins by form (band " << def.rules.balance.companyWinMin << "-" << def.rules.balance.companyWinMax << "%):\n";
        for (std::size_t g = 0; g < def.guests.size(); ++g)
        {
            const SoakForm &f = r.forms[g];
            const PowerDef &p = def.powers[U(def.guests[g].power)];
            out << "  " << def.guests[g].taken << " (" << p.id << "): " << share(f.companyWins, f.nights) << "% of " << f.nights << " hunts; "
                << def.guests[g].id << " the Taken in " << share(r.marked[g], r.nights) << "% of nights\n";
        }
        out << "all finished (no hunt): " << share(r.allFinished, r.nights) << "% (at most " << def.rules.balance.allFinishedMax << "%)\n"
            << "rooms open by midnight: " << each(r.roomsByMidnight) << " (band " << def.rules.balance.roomsMin << "-" << def.rules.balance.roomsMax << ")\n"
            << "evening rounds with a chat between bots: " << share(r.roundsWithBotChat, r.eveningRounds) << "% (at least "
            << def.rules.balance.botChatRoundsMin << "%)\n"
            << "goals finished (band " << def.rules.balance.goalMin << "-" << def.rules.balance.goalMax << "%):\n";
        for (std::size_t i = 0; i < r.goalsDrawn.size(); ++i)
            out << "  " << def.goals[i].id << ": " << share(r.goalsDone[i], r.goalsDrawn[i]) << "% of " << r.goalsDrawn[i]
                << (def.goals[i].guest == 0 ? " (the player's seat, not banded)\n" : "\n");
        return out.str();
    }

    nlohmann::json Json(const NightDef &def, const SoakReport &r)
    {
        nlohmann::json forms = nlohmann::json::array();
        for (std::size_t g = 0; g < def.guests.size(); ++g)
            forms.push_back({{"guest", def.guests[g].id}, {"form", def.guests[g].taken}, {"hunts", r.forms[g].nights},
                             {"companyWins", r.forms[g].companyWins}});
        return {{"nights", r.nights}, {"faults", r.faults}, {"companyWins", r.companyWins}, {"takenWins", r.takenWins}, {"playerWins", r.playerWins},
                {"rooms", r.rooms}, {"monsterKills", r.monsterKills},
                {"minionKills", r.minionKills}, {"levels", r.levels}, {"downs", r.downs}, {"deaths", r.deaths}, {"forms", forms},
                {"allFinished", r.allFinished}, {"taken", r.marked}, {"roomsByMidnight", r.roomsByMidnight}, {"eveningRounds", r.eveningRounds},
                {"roundsWithBotChat", r.roundsWithBotChat}, {"goalsDrawn", r.goalsDrawn}, {"goalsDone", r.goalsDone}, {"holds", r.Holds(def)}};
    }
}
