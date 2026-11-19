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
            const int there = static_cast<int>(std::ranges::count_if(s.units, [&](const Unit &u) { return Covers(s.board, u, cell); }));
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
                if (const int n = Neighbour(s.board, m.cell, d); n >= 0 && score(n) > bestScore &&
                    (m.size > 1 ? Fits(s, m.size, n, static_cast<int>(&m - s.units.data())) : Free(def, s, n)))
                    best = n, bestScore = score(n);
            if (best < 0)
                return false;
            m.cell = static_cast<std::int16_t>(best);
            return true;
        }

        bool AllCentre(const Board &b, int size, int anchor)
        {
            const int x0 = anchor % b.width, y0 = anchor / b.width;
            for (int y = y0; y < y0 + size; ++y)
                for (int x = x0; x < x0 + size; ++x)
                    if (x >= b.width || y >= b.height || b.cells[U(y * b.width + x)].kind != CellKind::Centre)
                        return false;
            return true;
        }

        // The nearest character within sight: walk toward it inside the centre, then battle it when beside.
        void Hunt(const MatchDef &def, MatchState &s, int i)
        {
            Unit &m = s.units[U(i)];
            int target = -1, best = def.monsters[U(m.def)].sight + 1;
            for (int v = 0; v < static_cast<int>(s.units.size()); ++v)
                if (const Unit &c = s.units[U(v)]; c.kind == 0 && c.cell >= 0 && c.hp > 0)
                    if (const int d = StepsTo(s.board, c.cell, m); d >= 0 && d < best)
                        target = v, best = d;
            if (target < 0)
                return;
            const int goal = s.units[U(target)].cell;
            auto score = [&](int anchor) {
                Unit probe = m;
                probe.cell = static_cast<std::int16_t>(anchor);
                return AllCentre(s.board, m.size, anchor) ? -StepsTo(s.board, goal, probe) : -1000;
            };
            for (int k = 0; k < m.speed && !Beside(s.board, m, s.units[U(target)]) && StepBy(def, s, m, score); ++k) {}
            if (Beside(s.board, m, s.units[U(target)]))
                StartBattle(def, s, i, target);
        }

        void Act(const MatchDef &def, MatchState &s, int i)
        {
            Unit &m = s.units[U(i)];
            const MonsterDef &d = def.monsters[U(m.def)];
            switch (d.behaviour)
            {
            case Behaviour::Stay: break;
            case Behaviour::Hunt: Hunt(def, s, i); break;
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
                    if (t.kind == 0 && t.cell >= 0 && t.faction != m.faction && Beside(s.board, m, t) && m.abilities[0] >= 0)
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
        s.monsterTurn = 0;
        MonsterPhase(def, s);
    }

    namespace inner
    {
        bool MonsterPhase(const MatchDef &def, MatchState &s)
        {
            // 1. Every monster on the board, in unit order; stop while a battle waits on a character.
            for (; s.monsterTurn < static_cast<int>(s.units.size()); ++s.monsterTurn)
            {
                const Unit &m = s.units[U(s.monsterTurn)];
                if (m.kind == 1 && m.cell >= 0 && m.hp > 0)
                    Act(def, s, s.monsterTurn);
                if (s.battle.on)
                    return false;
            }
            s.monsterTurn = -1;
            return true;
        }

        bool Fits(const MatchState &s, int size, int anchor, int self)
        {
            const Board &b = s.board;
            const int x0 = anchor % b.width, y0 = anchor / b.width;
            if (anchor < 0 || x0 + size > b.width || y0 + size > b.height)
                return false;
            for (int y = y0; y < y0 + size; ++y)
                for (int x = x0; x < x0 + size; ++x)
                {
                    const int c = y * b.width + x;
                    if (b.cells[U(c)].kind == CellKind::Wall || c == s.stage)
                        return false;
                    for (int o = 0; o < static_cast<int>(s.units.size()); ++o)
                        if (o != self && Covers(b, s.units[U(o)], c))
                            return false;
                }
            return true;
        }

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
                // 2. Due and room for it (centred on the square): a new monster replaces the downed one in the slot.
                const int size = def.monsters[U(w.monster)].size, anchor = w.cell - (size - 1) / 2 * (s.board.width + 1);
                if (w.round > s.round || w.cell % s.board.width < (size - 1) / 2 || !Fits(s, size, anchor))
                {
                    ++i;
                    continue;
                }
                for (int slot = 0; slot < static_cast<int>(s.units.size()); ++slot)
                    if (s.units[U(slot)].kind == 1 && s.units[U(slot)].home == w.cell)
                    {
                        s.units[U(slot)] = FromMonster(def, w.monster, -1, anchor);
                        s.units[U(slot)].home = w.cell;
                        Log(s, LogKind::Spawned, w.cell, slot);
                        break;
                    }
                s.spawns.erase(s.spawns.begin() + static_cast<std::ptrdiff_t>(i));
            }
        }
    }
}
