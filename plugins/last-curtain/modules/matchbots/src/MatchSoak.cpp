#include <MatchSoak.hpp>

#include <algorithm>
#include <cstdio>

// The soak (Bots.md "The soak").
namespace umbra::match
{
    namespace
    {
        std::size_t U(int i) { return static_cast<std::size_t>(i); }
        double Percent(long long part, long long whole) { return whole > 0 ? 100.0 * static_cast<double>(part) / static_cast<double>(whole) : 0; }
        int Lead(const MatchState &s) { return s.score[0] == s.score[1] ? -1 : (s.score[0] > s.score[1] ? 0 : 1); }
    }

    MatchReport SoakMatches(const MatchDef &def, std::uint64_t firstSeed, int runs, BotLevel side0, BotLevel side1)
    {
        MatchReport r;
        double decided = 0;
        for (int run = 0; run < runs; ++run)
        {
            // 1. Both sides picked by the rigged pick, then played by bots to the end.
            const std::uint64_t seed = firstSeed + static_cast<std::uint64_t>(run);
            MatchState s = NewMatch(def, seed, RiggedPick(def, seed * 7919, def.options.side));
            int applies = 0, lead = -1, lastChange = 1, round = s.round, leadAt = -1;
            const int twoThirds = (def.options.rounds * 2 + 2) / 3;
            while (Current(s) >= 0 && applies++ < 40000)
            {
                const BotLevel level = s.units[U(Current(s))].faction == 0 ? side0 : side1;
                if (!Apply(def, s, ChooseAction(def, s, level)))
                    break;
                // 2. Who leads at each new round, and at two thirds.
                if (s.round != round)
                {
                    round = s.round;
                    if (const int now = Lead(s); now >= 0 && now != lead)
                        lead = now, lastChange = round;
                    if (round == twoThirds)
                        leadAt = Lead(s);
                }
            }
            ++r.runs;
            if (!s.over)
            {
                ++r.faults;
                continue;
            }
            // 3. Essence by source, from the log of downs; guardians that fell.
            for (const LogEntry &e : s.log)
            {
                if (e.kind == LogKind::Performed)
                    r.performed += e.b;
                if (e.kind != LogKind::Down || e.b < 0 || s.units[U(e.b)].kind != 0)
                    continue;
                const Unit &v = s.units[U(e.a)];
                if (v.kind == 0)
                    r.fromKills += e.c;
                else if (def.monsters[U(v.def)].tier == Tier::Guardian)
                    r.fromGuardians += e.c, ++r.guardianFalls;
                else
                    r.fromMonsters += e.c;
            }
            if (s.winner < 0)
                ++r.draws;
            else
            {
                ++r.wins[U(s.winner)];
                if (s.winner == s.units[U(s.order[0])].faction)
                    ++r.firstMoverWins;
            }
            if (leadAt >= 0)
            {
                ++r.leads;
                if (s.winner >= 0 && s.winner != leadAt)
                    ++r.comebacks;
            }
            decided += lastChange;
        }
        r.decidedRound = r.runs > r.faults ? decided / (r.runs - r.faults) : 0;
        return r;
    }

    bool MatchReport::Holds(const MatchDef &def) const
    {
        const auto &b = def.rules.balance;
        const long long played = wins[0] + wins[1], gained = fromMonsters + fromKills + fromGuardians;
        const double side0 = Percent(wins[0], played), first = Percent(firstMoverWins, played);
        const double top = Percent(std::max({fromMonsters, fromKills, fromGuardians}), gained);
        return faults == 0 && side0 >= b.sideMin && side0 <= b.sideMax && first - 50 <= b.firstMoverMax && top <= b.sourceMax &&
               Percent(comebacks, leads) >= b.comebackMin;
    }

    std::string Text(const MatchDef &def, const MatchReport &r)
    {
        const long long played = r.wins[0] + r.wins[1], gained = r.fromMonsters + r.fromKills + r.fromGuardians;
        const auto &b = def.rules.balance;
        char buffer[1024];
        std::snprintf(buffer, sizeof buffer,
                      "matches %d, faults %d, draws %d\n"
                      "side 0 wins %.1f%% (band %d-%d)\n"
                      "the side moving first wins %.1f%% (at most %d over 50)\n"
                      "essence gained: monsters %.1f%%, kills %.1f%%, guardians %.1f%% (none over %d%%); performed %.1f a match\n"
                      "comebacks after two thirds %.1f%% of %d led matches (at least %d%%)\n"
                      "guardians fell %d times; the lead last changed on round %.1f on average\n",
                      r.runs, r.faults, r.draws, Percent(r.wins[0], played), b.sideMin, b.sideMax, Percent(r.firstMoverWins, played),
                      b.firstMoverMax, Percent(r.fromMonsters, gained), Percent(r.fromKills, gained), Percent(r.fromGuardians, gained),
                      b.sourceMax, r.runs > 0 ? static_cast<double>(r.performed) / r.runs : 0, Percent(r.comebacks, r.leads), r.leads,
                      b.comebackMin, r.guardianFalls, r.decidedRound);
        return buffer;
    }
}
