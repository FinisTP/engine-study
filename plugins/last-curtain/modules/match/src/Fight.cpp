#include <Match.hpp>

#include "MatchInner.hpp"

#include <algorithm>

// The campaign's falls, reviving and removal (Match.md "Falls").
namespace umbra::match
{
    using namespace inner;

    namespace inner
    {
        int RevivePlace(const MatchDef &def, const MatchState &s, int unit, int reviver)
        {
            const Unit &u = s.units[U(unit)];
            if (Room(def, s, u.cell, u.faction))
                return u.cell;
            int best = -1, bestSteps = 1 << 20, bestNear = 1 << 20;
            const int from = s.units[U(reviver)].cell;
            for (int c = 0; c < static_cast<int>(s.board.cells.size()); ++c)
            {
                const int d = Steps(s.board, u.cell, c);
                if (d <= 0 || !Room(def, s, c, u.faction))
                    continue;
                const int near = Steps(s.board, from, c);
                if (d < bestSteps || (d == bestSteps && near < bestNear))
                    best = c, bestSteps = d, bestNear = near;
            }
            return best;
        }

        void Revive(const MatchDef &def, MatchState &s, int unit, int reviver, int percent)
        {
            const int cell = RevivePlace(def, s, unit, reviver);
            Unit &u = s.units[U(unit)];
            u.cell = static_cast<std::int16_t>(cell);
            u.hp = static_cast<std::int16_t>(std::max(1, u.maxHp * percent / 100));
            u.ap = 0;
            u.from = static_cast<std::int16_t>(s.round + 1); // joins the order next round
            Log(s, LogKind::Healed, reviver, unit, u.hp);
        }

        void Remove(MatchState &s, int unit, int by)
        {
            Unit &u = s.units[U(unit)];
            Log(s, LogKind::Down, unit, by, -1);
            u.cell = -1;
            u.flags |= flag::Removed;
        }
    }
}
