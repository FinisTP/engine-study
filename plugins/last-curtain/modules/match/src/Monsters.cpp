#include <Match.hpp>

#include "MatchInner.hpp"

#include <algorithm>

// Monsters act once a round, after the last character (Match.md "Monsters").
namespace umbra::match
{
    using namespace inner;

    namespace
    {
        bool Free(const MatchDef &def, const MatchState &s, int cell)
        {
            const int there = static_cast<int>(std::ranges::count_if(s.units, [&](const Unit &u) { return u.cell == cell && u.hp > 0; }));
            return cell != s.stage && there < def.rules.board.squareCapacity; // monsters never take the stage
        }

        int NearestCharacter(const MatchState &s, int cell)
        {
            int best = 1 << 20;
            for (const Unit &u : s.units)
                if (u.kind == 0 && u.cell >= 0)
                    if (const int d = Steps(s.board, cell, u.cell); d >= 0)
                        best = std::min(best, d);
            return best;
        }

        // One step to the free neighbour with the best score (higher wins), if it beats staying.
        template <class F> bool StepBy(const MatchDef &def, MatchState &s, Unit &m, F score)
        {
            int best = -1, bestScore = score(m.cell);
            for (int d = 0; d < 4; ++d)
                if (const int n = Neighbour(s.board, m.cell, d); n >= 0 && Free(def, s, n) && score(n) > bestScore)
                    best = n, bestScore = score(n);
            if (best < 0)
                return false;
            m.cell = static_cast<std::int16_t>(best);
            return true;
        }

        void Act(const MatchDef &def, MatchState &s, int i)
        {
            Unit &m = s.units[U(i)];
            const MonsterDef &d = def.monsters[U(m.def)];
            switch (d.behaviour)
            {
            case Behaviour::Stay:
            case Behaviour::Hunt: break; // hunters act in the monster phase (MonsterMind.cpp)
            case Behaviour::Flee: // 1. up to `speed` steps away from the nearest character
                for (int k = 0; k < m.speed && StepBy(def, s, m, [&](int c) { return NearestCharacter(s, c); }); ++k) {}
                break;
            case Behaviour::Stage: // 2. up to `speed` steps toward the stage, onto it when free
                for (int k = 0; k < m.speed && m.cell != s.stage && StepBy(def, s, m, [&](int c) { return -Steps(s.board, c, s.stage); }); ++k) {}
                break;
            case Behaviour::Guard: // 3. a blow to every enemy character next to it, no battle
                for (int v = 0; v < static_cast<int>(s.units.size()); ++v)
                {
                    Unit &t = s.units[U(v)];
                    if (t.kind == 0 && t.cell >= 0 && t.faction != m.faction && Adjacent(s.board, m.cell, t.cell) && m.abilities[0] >= 0)
                    {
                        const int dmg = Damage(def, s, m.abilities[0], i, v, 0);
                        t.hp = static_cast<std::int16_t>(std::max(0, t.hp - dmg));
                        Log(s, LogKind::Blow, i, v, dmg);
                        if (t.hp == 0)
                            Down(def, s, v, i);
                    }
                }
                break;
            }
        }
    }

    void RunMonsters(const MatchDef &def, MatchState &s)
    {
        for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
        {
            Unit &m = s.units[U(i)];
            if (m.kind != 1)
                continue;
            if (m.cell >= 0 && m.hp > 0)
                Act(def, s, i);
        }
    }

    namespace inner
    {
        int ZoneOf(const Board &b, int cell)
        {
            int zone = 0;
            for (const auto *list : {&b.weakDens, &b.strongDens, &b.centreSpawns})
            {
                if (std::ranges::find(*list, static_cast<std::int16_t>(cell)) != list->end())
                    return zone;
                ++zone;
            }
            return -1;
        }

        int SpawnKind(const MatchDef &def, int zone, int round)
        {
            int kind = -1;
            for (const SpawnRow &w : def.spawns)
                if (w.zone == zone && w.from <= round)
                    kind = w.monster;
            return kind;
        }

        void ScheduleSpawn(const MatchDef &def, MatchState &s, int cell)
        {
            // 1. Due `every` rounds on (the row in force now), or with its mirror if that one is already due.
            const int zone = ZoneOf(s.board, cell);
            const SpawnRow *now = nullptr;
            for (const SpawnRow &w : def.spawns)
                if (w.zone == zone && w.from <= s.round)
                    now = &w;
            if (!now)
                return;
            const int mirror = Mirror(s.board, cell);
            const auto partner = std::ranges::find(s.spawns, static_cast<std::int16_t>(mirror), &Spawn::cell);
            const int round = partner != s.spawns.end() ? partner->round : s.round + now->every;
            s.spawns.push_back({static_cast<std::int16_t>(cell), static_cast<std::int8_t>(SpawnKind(def, zone, round)), round});
            // 2. Announced at once when the warning time has already come.
            if (round - def.rules.spawn.warn <= s.round)
                Log(s, LogKind::SpawnSoon, cell, s.spawns.back().monster, round);
        }

        void RunSpawns(const MatchDef &def, MatchState &s, bool roundStart)
        {
            for (std::size_t i = 0; i < s.spawns.size();)
            {
                const Spawn w = s.spawns[i];
                // 1. At a round's start: announce what is due spawn.warn rounds from now.
                if (roundStart && w.round - def.rules.spawn.warn == s.round && w.round > s.round)
                    Log(s, LogKind::SpawnSoon, w.cell, w.monster, w.round);
                // 2. Due and free: a new monster replaces the downed one in the square's slot.
                const bool taken = std::ranges::any_of(s.units, [&](const Unit &u) { return u.cell == w.cell && u.hp > 0; });
                if (w.round > s.round || taken)
                {
                    ++i;
                    continue;
                }
                for (int slot = 0; slot < static_cast<int>(s.units.size()); ++slot)
                    if (s.units[U(slot)].kind == 1 && s.units[U(slot)].home == w.cell)
                    {
                        s.units[U(slot)] = FromMonster(def, w.monster, -1, w.cell);
                        Log(s, LogKind::Spawned, w.cell, slot);
                        break;
                    }
                s.spawns.erase(s.spawns.begin() + static_cast<std::ptrdiff_t>(i));
            }
        }
    }
}
