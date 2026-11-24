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

        // 2. Rooms: flood fill over open squares, stopping at walls and doorways.
        std::vector<int> Rooms(const StageDef &stage)
        {
            std::vector<int> room(static_cast<std::size_t>(stage.width * stage.height), -1);
            int next = 0;
            for (int start = 0; start < static_cast<int>(room.size()); ++start)
            {
                const char c = stage.At(start % stage.width, start / stage.width);
                if (c == '#' || c == 'D' || room[static_cast<std::size_t>(start)] >= 0)
                    continue;
                std::vector<int> todo{start};
                room[static_cast<std::size_t>(start)] = next;
                while (!todo.empty())
                {
                    const int at = todo.back();
                    todo.pop_back();
                    for (int dir = 0; dir < 4; ++dir)
                    {
                        const int x = at % stage.width + dx[dir], y = at / stage.width + dy[dir], n = y * stage.width + x;
                        const char k = stage.At(x, y);
                        if (k != '#' && k != 'D' && room[static_cast<std::size_t>(n)] < 0)
                            room[static_cast<std::size_t>(n)] = next, todo.push_back(n);
                    }
                }
                ++next;
            }
            return room;
        }
    }

    BuiltStage BuildStage(const match::MatchDef &def, const Cast &cast, const Growth &growth, const StageDef &stage,
                          const std::vector<PartyMember> &party, int refill, const std::set<std::string> &taken, std::uint64_t seed)
    {
        BuiltStage out;
        out.rooms = Rooms(stage);
        match::MatchState &s = out.state;
        s.board = MapBoard(stage);
        s.mode = def.modes.at("campaign");
        s.mode.mustFree = stage.mustFree;
        s.noRevive = 1 << 1; // the enemy side never revives: more come as reinforcements (note 7)
        for (const int side : stage.noRevive)
            s.noRevive = static_cast<std::uint8_t>(s.noRevive | (1 << side));
        const auto add = [&](match::Unit u, const std::string &who, float height, UnitBehaviour behaviour = {}) {
            s.units.push_back(u);
            out.who.push_back(who);
            out.height.push_back(height);
            out.behaviour.push_back(behaviour);
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
            if (const auto hurt = stage.startHp.find(party[i].id); hurt != stage.startHp.end())
                u.hp = static_cast<std::int16_t>(std::clamp<int>(hurt->second, 1, u.maxHp));
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
            if (stage.meleeEnemies && su.side != 0) // a tutorial's enemies fight up close (no Snipe)
                for (auto &a : u.abilities)
                    if (a >= 0 && def.abilities[static_cast<std::size_t>(a)].range > 1)
                        a = -1;
            if (stage.meleeEnemies && su.side != 0)
                (void)std::ranges::stable_partition(u.abilities, [](auto a) { return a >= 0; });
            u.flags = static_cast<std::uint8_t>((su.boss ? match::flag::Boss : 0) | (su.recruit ? match::flag::Recruit : 0) | (su.strung ? match::flag::Strung : 0));
            const int cell = su.y * stage.width + su.x;
            u.cell = static_cast<std::int16_t>(su.behaviour == Behaviour::Reinforcement ? -1 : cell);
            const int index = add(u, su.who, su.height, {su.behaviour, su.after});
            if (su.behaviour == Behaviour::Reinforcement)
                s.arrivals.push_back({static_cast<std::int8_t>(index), static_cast<std::int16_t>(cell), static_cast<std::int16_t>(su.round)});
            for (const auto &[item, percent] : su.drops)
                s.drops.push_back({static_cast<std::int8_t>(index), static_cast<std::int8_t>(ItemRow(def, item)), static_cast<std::int8_t>(percent)});
        }
        // 4. Gimmicks: the stage's, with the refill's twist replacing what it names (Campaign.md "Gimmicks").
        nlohmann::json g = stage.gimmicks;
        if (refill > 0 && !stage.twists.empty())
            for (const auto &[key, v] : stage.twists[static_cast<std::size_t>(std::min<int>(refill, static_cast<int>(stage.twists.size())) - 1)].gimmicks.items())
                g[key] = v;
        match::Gimmicks &mg = s.gimmicks;
        if (g.contains("windows"))
        {
            const nlohmann::json &w = g["windows"];
            for (int c = 0; c < stage.width * stage.height; ++c)
                if (stage.At(c % stage.width, c / stage.width) == 'W')
                    mg.windows.push_back(static_cast<std::int16_t>(c));
            for (const nlohmann::json &round : w.value("pattern", nlohmann::json::array()))
                mg.pattern.push_back(round.get<std::vector<std::int8_t>>());
            out.spawnWho = w.value("spawn", "");
            mg.spawn = MakeUnit(def, cast, growth, out.spawnWho, w.value("level", 1), 1, true);
            mg.spawnMax = static_cast<std::int8_t>(w.value("max", 1));
            for (const nlohmann::json &d : w.value("drops", nlohmann::json::array()))
                mg.spawnDrops.push_back({-1, static_cast<std::int8_t>(ItemRow(def, d.value("item", ""))), static_cast<std::int8_t>(d.value("percent", 0))});
        }
        if (g.contains("hazards"))
            mg.hazards = static_cast<std::int8_t>(g["hazards"].value("count", 0)), mg.hazardDamage = static_cast<std::int8_t>(g["hazards"].value("damage", 0));
        if (g.contains("breath"))
        {
            const std::string who = g["breath"].value("unit", "");
            for (std::size_t i = 0; i < out.who.size(); ++i)
                if (out.who[i] == who && s.units[i].faction != 0)
                    mg.breathUnit = static_cast<std::int8_t>(i);
            mg.breathDamage = static_cast<std::int8_t>(g["breath"].value("damage", 0));
            mg.breathEvery = static_cast<std::int8_t>(g["breath"].value("every", 2));
            mg.breathFirst = static_cast<std::int8_t>(g["breath"].value("first", 1));
        }
        if (g.contains("dark"))
            out.sight = g["dark"].value("sight", 4);
        // 5. Items not taken yet, on their squares.
        for (const StageItem &it : stage.items)
            if (!taken.contains(it.id))
                s.floor.push_back({static_cast<std::int16_t>(it.y * stage.width + it.x), static_cast<std::int8_t>(ItemRow(def, it.item)), 0, false});
        for (const std::string &l : stage.leaders) // a tutorial's turn order (the user, notes 14 and 20)
            for (std::size_t i = 0; i < out.who.size(); ++i)
                if (out.who[i] == l)
                    s.mode.leaders.push_back(static_cast<std::int8_t>(i));
        s.mode.leadRounds = stage.leadRounds;
        s.mode.boardUses = 2;
        out.press = true; // campaign enemies come to you: board abilities are counted, so nobody can wait a fight out // the campaign: a board ability is free, twice a fight (the user, note 16)
        match::BeginMatch(def, s, seed);
        Sync(out);
        return out;
    }

    void Sync(BuiltStage &stage)
    {
        while (stage.who.size() < stage.state.units.size())
        {
            stage.who.push_back(stage.spawnWho);
            stage.height.push_back(0.8F); // an ordinary ghost's height
            stage.behaviour.push_back({});
        }
    }
}
