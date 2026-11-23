#include <Campaign.hpp>

namespace umbra::campaign
{
    namespace
    {
        constexpr int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0}; // N E S W, as the match's board

        int ItemRow(const match::MatchDef &def, const std::string &id)
        {
            for (std::size_t i = 0; i < def.items.size(); ++i)
                if (def.items[i].id == id)
                    return static_cast<int>(i);
            return -1;
        }

        // 1. The map as the match's board: walls, open squares, doors between open neighbours.
        match::Board MapBoard(const StageDef &stage)
        {
            match::Board b;
            b.width = stage.width, b.height = stage.height;
            b.cells.resize(static_cast<std::size_t>(b.width * b.height));
            for (int y = 0; y < b.height; ++y)
                for (int x = 0; x < b.width; ++x)
                {
                    match::Cell &c = b.cells[static_cast<std::size_t>(y * b.width + x)];
                    if (stage.At(x, y) == '#')
                        continue;
                    c.kind = match::CellKind::Hall;
                    for (int dir = 0; dir < 4; ++dir)
                        if (stage.At(x + dx[dir], y + dy[dir]) != '#')
                            c.doors = static_cast<std::uint8_t>(c.doors | (1 << dir));
                }
            b.hub = {-1, -1}, b.door = {-1, -1};
            return b;
        }
    }

    BuiltStage BuildStage(const match::MatchDef &def, const Cast &cast, const Growth &growth, const StageDef &stage,
                          const std::vector<PartyMember> &party, int refill, const std::set<std::string> &taken, std::uint64_t seed)
    {
        BuiltStage out;
        match::MatchState &s = out.state;
        s.board = MapBoard(stage);
        s.mode = def.modes.at("campaign");
        s.mode.mustFree = stage.mustFree;
        for (const int side : stage.noRevive)
            s.noRevive = static_cast<std::uint8_t>(s.noRevive | (1 << side));
        const auto add = [&](match::Unit u, const std::string &who, float height) {
            s.units.push_back(u);
            out.who.push_back(who);
            out.height.push_back(height);
            return static_cast<int>(s.units.size()) - 1;
        };
        // 2. The party, up to the cap, on the starts in reading order; the first is the player.
        std::vector<int> starts;
        for (int c = 0; c < static_cast<int>(s.board.cells.size()); ++c)
            if (stage.At(c % stage.width, c / stage.width) == 'S')
                starts.push_back(c);
        for (std::size_t i = 0; i < party.size() && static_cast<int>(i) < stage.partyCap && i < starts.size(); ++i)
        {
            match::Unit u = MakeUnit(def, cast, growth, party[i].id, party[i].level, 0, false);
            u.cell = static_cast<std::int16_t>(starts[i]);
            u.flags = i == 0 ? match::flag::Player : 0;
            for (std::size_t k = 0; k < party[i].items.size() && k < u.items.size(); ++k)
                u.items[k] = static_cast<std::int8_t>(ItemRow(def, party[i].items[k]));
            add(u, party[i].id, 1.0F);
        }
        // 3. Enemies and others: the stage's, then the refill's twist (the last twist repeats).
        std::vector<StageUnit> units = stage.units;
        if (refill > 0 && !stage.twists.empty())
        {
            const Twist &t = stage.twists[static_cast<std::size_t>(std::min<int>(refill, static_cast<int>(stage.twists.size())) - 1)];
            units.insert(units.end(), t.units.begin(), t.units.end());
        }
        for (const StageUnit &su : units)
        {
            match::Unit u = MakeUnit(def, cast, growth, su.who, su.level, su.side, su.side != 0);
            u.flags = static_cast<std::uint8_t>((su.boss ? match::flag::Boss : 0) | (su.recruit ? match::flag::Recruit : 0) | (su.strung ? match::flag::Strung : 0));
            const int cell = su.y * stage.width + su.x;
            u.cell = static_cast<std::int16_t>(su.behaviour == Behaviour::Reinforcement ? -1 : cell);
            const int index = add(u, su.who, su.height);
            if (su.behaviour == Behaviour::Reinforcement)
                s.arrivals.push_back({static_cast<std::int8_t>(index), static_cast<std::int16_t>(cell), static_cast<std::int16_t>(su.round)});
            for (const auto &[item, percent] : su.drops)
                s.drops.push_back({static_cast<std::int8_t>(index), static_cast<std::int8_t>(ItemRow(def, item)), static_cast<std::int8_t>(percent)});
        }
        // 4. Items not taken yet, on their squares.
        for (const StageItem &it : stage.items)
            if (!taken.contains(it.id))
                s.floor.push_back({static_cast<std::int16_t>(it.y * stage.width + it.x), static_cast<std::int8_t>(ItemRow(def, it.item)), 0, false});
        match::BeginMatch(def, s, seed);
        return out;
    }
}
