#include <Campaign.hpp>

namespace umbra::campaign
{
    std::vector<std::string> StageMoments(const StageDef &def, const BuiltStage &stage, std::size_t told, Moments &m)
    {
        std::vector<std::string> out;
        const auto reach = [&](const std::string &moment) {
            const auto it = def.scenes.find(moment);
            if (it != def.scenes.end() && m.done.insert(moment).second)
                out.push_back(it->second);
        };
        const match::MatchState &s = stage.state;
        const auto who = [&](int unit) { return unit >= 0 && static_cast<std::size_t>(unit) < stage.who.size() ? stage.who[static_cast<std::size_t>(unit)] : std::string(); };
        // 1. The start and every round reached.
        reach("start");
        for (int r = 1; r <= s.round; ++r)
            reach("round:" + std::to_string(r));
        // 2. Told log lines: a first blow on a unit, a fall, a freeing.
        for (; m.read < told && m.read < s.log.size(); ++m.read)
        {
            const match::LogEntry &e = s.log[m.read];
            if (e.kind == match::LogKind::Blow)
                reach("firstAttacked:" + who(e.b));
            else if (e.kind == match::LogKind::Down)
                reach("falls:" + who(e.a));
            else if (e.kind == match::LogKind::Freed)
                reach("freed:" + who(e.b));
        }
        // 3. The end (won) or lost, once everything is told.
        if (s.over && m.read == s.log.size())
            reach(s.winner == 0 ? "end" : "lost");
        return out;
    }
}
