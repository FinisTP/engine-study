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
            return there < (cell == s.stage ? 1 : def.rules.board.squareCapacity);
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
        const int next = s.round + 1; // the round about to begin
        for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
        {
            Unit &m = s.units[U(i)];
            if (m.kind != 1)
                continue;
            const MonsterDef &d = def.monsters[U(m.def)];
            // 1. The great monster wakes in a stage room that is not the stage; a weak one returns to its free den.
            if (m.cell < 0 && d.tier == Tier::Great && next == def.rules.great.wakeRound)
            {
                std::vector<int> rooms;
                for (int r : s.board.stageRooms)
                    if (r != s.stage && Free(def, s, r))
                        rooms.push_back(r);
                if (!rooms.empty())
                    m.cell = m.home = static_cast<std::int16_t>(rooms[U(Roll(s, static_cast<int>(rooms.size())))]);
            }
            else if (m.cell < 0 && m.respawnRound > 0 && next >= m.respawnRound && Free(def, s, m.home))
            {
                m.cell = m.home;
                m.hp = m.maxHp;
                m.essence = d.essence;
                m.respawnRound = 0;
            }
            else if (m.cell >= 0 && m.hp > 0)
                Act(def, s, i);
        }
    }
}
