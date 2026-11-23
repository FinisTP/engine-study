#include <Match.hpp>

#include "MatchInner.hpp"

#include <algorithm>
#include <deque>

// A new match: sides, the rigged pick, turn order and placement (Match.md "A new match").
namespace umbra::match
{
    using namespace inner;

    Unit FromCharacter(const MatchDef &def, int c, int faction, int home)
    {
        const CharacterDef &d = def.characters[U(c)];
        Unit u;
        u.kind = 0;
        u.def = static_cast<std::int8_t>(c);
        u.faction = static_cast<std::int8_t>(faction);
        u.home = static_cast<std::int16_t>(home);
        u.hp = u.maxHp = static_cast<std::int16_t>(d.hp);
        u.attack = static_cast<std::int16_t>(d.attack);
        u.defence = static_cast<std::int16_t>(d.defence);
        u.speed = static_cast<std::int16_t>(d.speed);
        u.move = static_cast<std::int16_t>(d.move);
        // 1. The job's level-1 abilities, in jobs.csv order.
        std::size_t slot = 0;
        for (const JobLevel &j : def.jobs)
            if (j.role == d.role && j.level == 1 && slot < u.abilities.size())
                u.abilities[slot++] = static_cast<std::int8_t>(j.ability);
        return u;
    }

    Unit FromMonster(const MatchDef &def, int m, int faction, int cell)
    {
        const MonsterDef &d = def.monsters[U(m)];
        Unit u;
        u.kind = 1;
        u.def = static_cast<std::int8_t>(m);
        u.faction = static_cast<std::int8_t>(faction);
        u.cell = u.home = static_cast<std::int16_t>(cell);
        u.hp = u.maxHp = static_cast<std::int16_t>(d.hp);
        u.attack = static_cast<std::int16_t>(d.attack);
        u.defence = static_cast<std::int16_t>(d.defence);
        u.speed = static_cast<std::int16_t>(d.speed);
        u.size = static_cast<std::int8_t>(d.size);
        u.essence = d.essence;
        for (std::size_t i = 0; i < d.abilities.size() && i < u.abilities.size(); ++i)
            u.abilities[i] = static_cast<std::int8_t>(d.abilities[i]);
        return u;
    }

    namespace
    {

        // Squares nearest the hub, never the hub (it stays free to step through) nor the guardian's square.
        std::vector<int> StartSquares(const Board &b, int count)
        {
            std::vector<int> out, seen(b.cells.size(), 0);
            std::deque<int> open{b.hub[0]};
            seen[U(b.hub[0])] = 1;
            while (!open.empty() && static_cast<int>(out.size()) < count)
            {
                const int c = open.front();
                open.pop_front();
                if (c != b.door[0] && c != b.hub[0])
                    out.push_back(c);
                for (int dir = 0; dir < 4; ++dir)
                    if (const int n = Neighbour(b, c, dir); n >= 0 && !seen[U(n)] && b.cells[U(n)].kind != CellKind::Room)
                        seen[U(n)] = 1, open.push_back(n);
            }
            return out;
        }

        // A monster of the tier, picked from the seed.
        int OfTier(const MatchDef &def, Rng &rng, Tier tier)
        {
            std::vector<int> pool;
            for (int m = 0; m < static_cast<int>(def.monsters.size()); ++m)
                if (def.monsters[U(m)].tier == tier)
                    pool.push_back(m);
            return pool.empty() ? -1 : pool[U(rng.Below(static_cast<int>(pool.size())))];
        }
    }

    std::vector<int> RiggedPick(const MatchDef &def, std::uint64_t seed, int count)
    {
        Rng rng{seed ^ 0xB0757EEDULL};
        // 1. Roles in a shuffled order, the four balanced ones before the rest.
        std::vector<Role> first{Role::Defender, Role::Supporter, Role::AllRounder, Role::Attacker}, rest{Role::Speedster};
        for (auto *list : {&first, &rest})
            for (std::size_t i = list->size(); i > 1; --i)
                std::swap((*list)[i - 1], (*list)[U(rng.Below(static_cast<int>(i)))]);
        first.insert(first.end(), rest.begin(), rest.end());
        // 2. One character of each role, until there are enough.
        std::vector<int> out;
        for (Role r : first)
        {
            std::vector<int> pool;
            for (int c = 0; c < static_cast<int>(def.characters.size()); ++c)
                if (def.characters[U(c)].role == r)
                    pool.push_back(c);
            if (!pool.empty() && static_cast<int>(out.size()) < count)
                out.push_back(pool[U(rng.Below(static_cast<int>(pool.size())))]);
        }
        return out;
    }

    MatchState NewMatch(const MatchDef &def, std::uint64_t seed, const std::vector<int> &picks)
    {
        MatchState s;
        s.mode = def.modes.at("faction");
        s.board = BuildBoard(def, seed);
        Rng rng{seed};
        const Board &b = s.board;

        // 1. Characters: side 0 as picked, side 1 rigged; side 1 stands on the mirror of side 0's squares.
        const std::vector<int> sides[2] = {picks, RiggedPick(def, seed, static_cast<int>(picks.size()))};
        const std::vector<int> start = StartSquares(b, static_cast<int>(picks.size()));
        for (int f = 0; f < 2; ++f)
            for (std::size_t i = 0; i < sides[f].size() && i < start.size(); ++i)
            {
                Unit u = FromCharacter(def, sides[f][i], f, b.hub[U(f)]);
                u.cell = static_cast<std::int16_t>(f == 0 ? start[i] : Mirror(b, start[i]));
                s.units.push_back(u);
            }

        // 3. Guardians on their squares.
        for (int f = 0; f < 2; ++f)
            if (const int g = OfTier(def, rng, Tier::Guardian); g >= 0)
                s.units.push_back(FromMonster(def, g, f, b.door[U(f)]));
        // 4. Every spawn square owns a slot and is due its zone's first row (round 1 fills at once, if it fits).
        for (const auto *list : {&b.weakDens, &b.strongDens, &b.centreSpawns})
            for (int c : *list)
            {
                const int zone = ZoneOf(b, c), kind = SpawnKind(def, zone, 1);
                const auto first = std::ranges::find(def.spawns, zone, &SpawnRow::zone);
                if (first == def.spawns.end())
                    continue;
                Unit u = FromMonster(def, kind >= 0 ? kind : first->monster, -1, c);
                u.hp = 0, u.cell = -1;
                s.units.push_back(u);
                s.spawns.push_back({static_cast<std::int16_t>(c), static_cast<std::int8_t>(kind >= 0 ? kind : first->monster), kind >= 0 ? 1 : first->from});
            }

        // 5. A treasure in every item room (Plan 3: face-down cards when opened).
        for (const std::int16_t cell : b.itemRooms)
            s.floor.push_back({cell, -1, 0, true});

        // 6. The stage is the centre of the middle row; the first turn begins.
        s.stage = static_cast<std::int16_t>((b.height / 2) * b.width + b.width / 2);
        RunSpawns(def, s, false);
        s.rng = rng.Next();
        BeginMatch(def, s, rng.Next());
        return s;
    }

    void BeginMatch(const MatchDef &def, MatchState &s, std::uint64_t seed)
    {
        for (Unit &u : s.units)
            u.from = 0, u.ap = 0;
        s.orderSeed = seed;
        if (s.rng == 0) // a campaign fight: its own random numbers from the seed (NewMatch seeds the faction match's)
            s.rng = Rng{seed ^ 0xC0FFEEULL}.Next();
        s.round = 1;
        s.turn = -1;
        SortOrder(s);
        NextTurn(def, s);
    }

    namespace inner
    {
        void SortOrder(MatchState &s)
        {
            // Speed, highest first; ties by a key drawn for each unit from orderSeed (the same every round).
            s.order.clear();
            std::vector<std::uint64_t> key;
            Rng rng{s.orderSeed};
            for (std::size_t i = 0; i < s.units.size(); ++i)
            {
                s.order.push_back(static_cast<std::int8_t>(i));
                key.push_back(rng.Next());
            }
            std::ranges::stable_sort(s.order, [&](int a, int c) {
                return s.units[U(a)].speed != s.units[U(c)].speed ? s.units[U(a)].speed > s.units[U(c)].speed : key[U(a)] < key[U(c)];
            });
        }
    }

    int TurnsLeft(const MatchDef &, const MatchState &s) { return s.mode.roundLimit > 0 ? std::max(0, s.mode.roundLimit - s.round + 1) : -1; }
}
