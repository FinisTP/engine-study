#include <Match.hpp>

#include "MatchInner.hpp"

#include <algorithm>
#include <utility>

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

        void Remove(const MatchDef &def, MatchState &s, int unit, int by)
        {
            Unit &u = s.units[U(unit)];
            Log(s, LogKind::Down, unit, by, -1);
            u.cell = -1;
            u.flags |= flag::Removed;
            CheckEnd(def, s);
        }

        void RunArrivals(const MatchDef &def, MatchState &s)
        {
            for (std::size_t i = 0; i < s.arrivals.size();)
            {
                const Arrival a = s.arrivals[i];
                Unit &u = s.units[U(a.unit)];
                // 1. Next round's arrival: a mark on its square now.
                if (a.round == s.round + 1)
                    Log(s, LogKind::Arriving, a.unit, a.cell);
                if (a.round > s.round)
                {
                    ++i;
                    continue;
                }
                // 2. Its round: on its square, or the nearest one with room; it acts from the next round.
                u.cell = a.cell;
                if (!Room(def, s, a.cell, u.faction))
                {
                    u.cell = -1;
                    int best = -1, bestSteps = 1 << 20;
                    for (int c = 0; c < static_cast<int>(s.board.cells.size()); ++c)
                        if (const int d = Steps(s.board, a.cell, c); d > 0 && d < bestSteps && Room(def, s, c, u.faction))
                            best = c, bestSteps = d;
                    u.cell = static_cast<std::int16_t>(best);
                }
                u.from = static_cast<std::int16_t>(s.round + 1);
                Log(s, LogKind::Arrived, a.unit, u.cell);
                s.arrivals.erase(s.arrivals.begin() + static_cast<std::ptrdiff_t>(i));
            }
        }

        void RunGimmicks(const MatchDef &def, MatchState &s)
        {
            Gimmicks &g = s.gimmicks;
            const auto hurt = [&](int unit, int cell, int damage) {
                Unit &u = s.units[U(unit)];
                u.hp = static_cast<std::int16_t>(std::max(0, u.hp - damage));
                Log(s, LogKind::Hazard, unit, cell, damage);
                if (u.hp <= 0)
                    Down(def, s, unit, -1);
            };
            // 1. Last round's marks: hazards fall on anyone there (not one with sea wind), the breath strikes the
            //    other side on its row, open windows let out a spawn while fewer than spawnMax are alive.
            const std::vector<Mark> due = std::exchange(g.marks, {});
            for (const Mark &m : due)
                for (int i = 0; i < static_cast<int>(s.units.size()) && !s.over; ++i)
                {
                    const Unit &u = s.units[U(i)];
                    if (!Up(u) || u.cell != m.cell)
                        continue;
                    if (m.kind == Mark::Hazard && !std::ranges::any_of(u.abilities, [&](std::int8_t a) { return a >= 0 && def.abilities[U(a)].effect == AbilityEffect::SeaWind; }))
                        hurt(i, m.cell, g.hazardDamage);
                    if (m.kind == Mark::Breath && g.breathUnit >= 0 && u.faction != s.units[U(g.breathUnit)].faction)
                        hurt(i, m.cell, g.breathDamage);
                }
            for (const Mark &m : due)
            {
                const auto alive = std::ranges::count_if(s.units, [](const Unit &u) { return Up(u) && (u.flags & flag::Spawned); });
                if (m.kind != Mark::Window || s.over || alive >= g.spawnMax || !Room(def, s, m.cell, g.spawn.faction))
                    continue;
                Unit u = g.spawn;
                u.cell = m.cell;
                u.hp = u.maxHp;
                u.flags = static_cast<std::uint8_t>(u.flags | flag::Spawned);
                u.from = static_cast<std::int16_t>(s.round + 1);
                s.units.push_back(u);
                const auto index = static_cast<std::int8_t>(s.units.size() - 1);
                for (Drop d : g.spawnDrops)
                    d.unit = index, s.drops.push_back(d);
                s.order.push_back(index); // after this round's places; the next round sorts it in
                Log(s, LogKind::Arrived, index, m.cell);
            }
            if (s.over)
                return;
            // 2. The coming round's marks.
            std::vector<int> open;
            for (int c = 0; c < static_cast<int>(s.board.cells.size()); ++c)
                if (s.board.cells[U(c)].kind != CellKind::Wall)
                    open.push_back(c);
            for (int k = 0; k < g.hazards && !open.empty(); ++k)
            {
                const int pick = Roll(s, static_cast<int>(open.size()));
                g.marks.push_back({Mark::Hazard, static_cast<std::int16_t>(open[U(pick)])});
                open.erase(open.begin() + pick);
            }
            if (g.breathUnit >= 0 && Up(s.units[U(g.breathUnit)]) && g.breathEvery > 0 && s.round >= g.breathFirst && (s.round - g.breathFirst) % g.breathEvery == 0)
            {
                const Unit &b = s.units[U(g.breathUnit)];
                const int row = b.cell / s.board.width;
                for (int x = 0; x < s.board.width; ++x)
                    if (const int c = row * s.board.width + x; c != b.cell && s.board.cells[U(c)].kind != CellKind::Wall)
                        g.marks.push_back({Mark::Breath, static_cast<std::int16_t>(c)});
                Log(s, LogKind::Breath, g.breathUnit, b.cell);
            }
            if (!g.pattern.empty())
                for (const std::int8_t w : g.pattern[U((s.round - 1) % static_cast<int>(g.pattern.size()))])
                    if (w >= 0 && w < static_cast<int>(g.windows.size()))
                        g.marks.push_back({Mark::Window, g.windows[U(w)]});
        }

        int Passive(const MatchDef &def, const Unit &u, AbilityEffect effect)
        {
            for (const std::int8_t a : u.abilities)
                if (a >= 0 && def.abilities[U(a)].use == Use::Passive && def.abilities[U(a)].effect == effect)
                    return def.abilities[U(a)].amount;
            return 0;
        }

        void DropStolen(MatchState &s, int thief)
        {
            for (std::size_t i = 0; i < s.stash.size();)
                if (s.stash[i].thief == thief)
                {
                    s.floor.push_back({s.units[U(thief)].cell, s.stash[i].item, 0, false});
                    s.stash.erase(s.stash.begin() + static_cast<std::ptrdiff_t>(i));
                }
                else
                    ++i;
        }

        void Dart(const MatchDef &def, MatchState &s, int unit)
        {
            // Up to `amount` squares to where the nearest foe is farthest; ties: fewer steps, then the lower square.
            Unit &u = s.units[U(unit)];
            const int reach = Passive(def, u, AbilityEffect::Dart);
            if (reach <= 0 || !Up(u))
                return;
            const auto nearestFoe = [&](int cell) {
                int best = 1 << 20;
                for (const Unit &o : s.units)
                    if (Up(o) && o.faction != u.faction && o.faction >= 0)
                        best = std::min(best, Steps(s.board, cell, o.cell));
                return best;
            };
            int to = u.cell, far = nearestFoe(u.cell), steps = 0;
            for (int c = 0; c < static_cast<int>(s.board.cells.size()); ++c)
            {
                const int d = Steps(s.board, u.cell, c);
                if (d <= 0 || d > reach || !Room(def, s, c, u.faction))
                    continue;
                if (const int f = nearestFoe(c); f > far || (f == far && d < steps))
                    to = c, far = f, steps = d;
            }
            if (to != u.cell)
            {
                u.cell = static_cast<std::int16_t>(to);
                Log(s, LogKind::Moved, unit, to);
            }
        }

        int Player(const MatchState &s)
        {
            for (std::size_t i = 0; i < s.units.size(); ++i)
                if (s.units[i].flags & flag::Player)
                    return static_cast<int>(i);
            return -1;
        }

        bool RiteOffered(const MatchState &s) { return s.over && s.loss == Loss::PlayerFell && s.riteReady && !s.riteUsed && Player(s) >= 0; }

        void CheckEnd(const MatchDef &, MatchState &s)
        {
            if (s.over || s.mode.win != WinBy::Boss)
                return;
            const auto all = [&](auto test) { return std::ranges::all_of(s.units, test); };
            const auto any = [&](auto test) { return std::ranges::any_of(s.units, test); };
            const auto end = [&](int winner, Loss loss) {
                s.over = true;
                s.winner = winner;
                s.loss = loss;
                Log(s, LogKind::Over, winner);
            };
            // 1. The living player fell; 2. nobody of the side stands.
            const int player = Player(s);
            if (s.mode.playerLoss && player >= 0 && !Up(s.units[U(player)]))
                return end(1, Loss::PlayerFell);
            if (all([](const Unit &u) { return u.faction != 0 || !Up(u); }))
                return end(1, Loss::SideDown);
            // 3. A recruit that can no longer be freed: removed, or strung when the bosses are down.
            const auto waiting = [&](const Unit &u) { return std::ranges::any_of(s.arrivals, [&](const Arrival &a) { return &s.units[U(a.unit)] == &u; }); };
            const bool bossesDown = any([](const Unit &u) { return (u.flags & flag::Boss) != 0; }) &&
                                    all([&](const Unit &u) { return (u.flags & flag::Boss) == 0 || (!Up(u) && !waiting(u)); });
            const bool removed = any([](const Unit &u) { return (u.flags & flag::Recruit) && (u.flags & flag::Removed) && !(u.flags & flag::Freed); });
            const bool strung = any([](const Unit &u) { return (u.flags & flag::Recruit) && (u.flags & flag::Strung); });
            if (s.mode.mustFree && (removed || (bossesDown && strung)))
                return end(1, Loss::Trapped);
            // 4. Every boss down (and, if the stage asks, every recruit freed).
            if (bossesDown)
                end(0, Loss::None);
        }

        Why CheckCut(const MatchState &s, int me, int target)
        {
            if (!(s.units[U(me)].flags & flag::Player))
                return Why::OnlyPlayer;
            if (s.now.acted)
                return Why::AlreadyActed;
            if (target < 0 || target >= static_cast<int>(s.units.size()))
                return Why::NothingHere;
            const Unit &u = s.units[U(me)], &t = s.units[U(target)];
            if (!Fallen(t))
                return Why::NotFallen;
            if (!(t.flags & flag::Strung))
                return Why::NotStrung;
            return Beside(s.board, u, t) ? Why::Ok : Why::OutOfRange;
        }

        void Cut(const MatchDef &def, MatchState &s, int me, int target)
        {
            // The strings cut: the recruit is freed and leaves the board (it waits in the cafe).
            Unit &t = s.units[U(target)];
            Log(s, LogKind::Freed, me, target, t.cell);
            t.flags = static_cast<std::uint8_t>((t.flags & ~flag::Strung) | flag::Freed);
            t.cell = -1;
            Acted(def, s);
            CheckEnd(def, s);
        }

        void Rite(const MatchDef &def, MatchState &s)
        {
            // 1. The fight is on again; a battle in progress ends.
            s.over = false;
            s.winner = -1;
            s.loss = Loss::None;
            s.riteReady = false;
            s.riteUsed = true;
            s.battle.on = false;
            // 2. The player first, then the rest of the side: fallen ones stand up, everyone whole.
            const int player = Player(s);
            std::vector<int> side{player};
            for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
                if (i != player && s.units[U(i)].faction == 0 && !(s.units[U(i)].flags & flag::Removed) && s.units[U(i)].cell >= 0)
                    side.push_back(i);
            for (const int i : side)
            {
                if (Fallen(s.units[U(i)]))
                    Revive(def, s, i, player, 100);
                s.units[U(i)].hp = s.units[U(i)].maxHp;
            }
            Log(s, LogKind::Healed, player, player, 0);
            // 3. The order goes on from where it stopped.
            NextTurn(def, s);
        }
    }
}
