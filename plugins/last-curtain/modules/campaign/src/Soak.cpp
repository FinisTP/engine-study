#include <CampaignSoak.hpp>

#include <algorithm>
#include <sstream>

namespace umbra::campaign
{
    StageReport SoakStage(const match::MatchDef &def, const Cast &cast, const Growth &growth, const StageDef &stage, int refill, std::uint64_t seed, int runs)
    {
        std::vector<PartyMember> party;
        for (const char *who : {"player", "mai", "bao"}) // in the order they join
            if (static_cast<int>(party.size()) < stage.partyCap)
                party.push_back({who, stage.level + refill, {}}); // a refill comes after a story stage: the party has grown
        StageReport r;
        for (int run = 0; run < runs; ++run)
        {
            // 1. One fight, both sides played by bots.
            BuiltStage b = BuildStage(def, cast, growth, stage, party, refill, {}, seed + static_cast<std::uint64_t>(run));
            for (int actions = 0; actions < 6000 && b.state.round <= 40 && !b.state.over && match::Current(b.state) >= 0; ++actions)
            {
                const int me = match::Current(b.state);
                const bool ours = b.state.units[static_cast<std::size_t>(me)].faction == 0;
                const match::Action a = ours ? PartyAction(def, b, b.state, match::BotLevel::Medium) : EnemyAction(def, b, b.state, def.options.bots);
                if (!match::Apply(def, b.state, a) && !match::Apply(def, b.state, {match::ActionKind::EndTurn}))
                    break;
                Sync(b);
            }
            // 2. What happened.
            const match::MatchState &s = b.state;
            ++r.runs;
            if (s.over && s.winner == 0)
                ++r.wins;
            else if (s.over && s.loss == match::Loss::Trapped)
                ++r.trapped;
            else if (s.over && s.winner == 1)
                ++r.otherLosses;
            else
                ++r.draws;
            bool playerFell = false;
            for (const match::LogEntry &e : s.log)
                if (e.kind == match::LogKind::Down && s.units[static_cast<std::size_t>(e.a)].faction == 0)
                {
                    ++r.falls[b.who[static_cast<std::size_t>(e.a)]];
                    playerFell |= (s.units[static_cast<std::size_t>(e.a)].flags & match::flag::Player) != 0;
                }
            r.playerFalls += playerFell;
            for (const std::int8_t item : s.found)
                ++r.found[def.items[static_cast<std::size_t>(item)].id];
            // 3. Minutes: about 6 s per party turn, 1 s per told line (Campaign.md "The soak").
            r.rounds += s.round;
            r.minutes += (static_cast<double>(s.round) * stage.partyCap * 6.0 + static_cast<double>(s.log.size())) / 60.0;
        }
        if (r.runs > 0)
            r.rounds /= r.runs, r.minutes /= r.runs, r.playerFalls /= r.runs;
        return r;
    }

    std::vector<std::string> Missed(const StageReport &r, const nlohmann::json &bands)
    {
        const double n = std::max(1, r.runs);
        const std::map<std::string, double> value{{"wins", r.wins / n}, {"trapped", r.trapped / n}, {"playerFalls", r.playerFalls}, {"minutes", r.minutes}};
        std::vector<std::string> out;
        for (const auto &[key, band] : bands.items())
            if (value.contains(key) && band.is_array() && band.size() == 2 && (value.at(key) < band[0].get<double>() || value.at(key) > band[1].get<double>()))
                out.push_back(key + " " + std::to_string(value.at(key)) + " is outside [" + band[0].dump() + ", " + band[1].dump() + "]");
        return out;
    }

    std::string Text(const StageDef &stage, const StageReport &r)
    {
        std::ostringstream t;
        const double n = std::max(1, r.runs);
        t << stage.id << ": " << r.runs << " runs, wins " << 100.0 * r.wins / n << "%, trapped " << 100.0 * r.trapped / n << "%, other losses "
          << 100.0 * r.otherLosses / n << "%, unfinished " << 100.0 * r.draws / n << "%\n  rounds " << r.rounds << ", about " << r.minutes
          << " min, player fell in " << 100.0 * r.playerFalls << "% of runs\n  falls:";
        for (const auto &[who, count] : r.falls)
            t << " " << who << " " << count;
        t << "\n  found:";
        for (const auto &[item, count] : r.found)
            t << " " << item << " " << count;
        t << "\n";
        return t.str();
    }
}
