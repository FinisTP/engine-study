#include "MatchTestData.hpp"

#include <Match.hpp>

#include <algorithm>
#include <set>

using namespace umbra::match;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }
    int Char(const MatchDef &def, const char *id) { return MatchDef::Find(def.characters, id); }
}

TEST_CASE("A match starts with both sides at home (T280)", "[match][setup]")
{
    const MatchDef def = Data();
    const std::vector<int> picks{Char(def, "bao"), Char(def, "khang"), Char(def, "mai")};
    const MatchState s = NewMatch(def, 7, picks);
    int chars[2] = {}, guards[2] = {};
    std::set<int> cells;
    for (const Unit &u : s.units)
    {
        if (u.cell >= 0)
            CHECK(cells.insert(u.cell).second); // one unit per square
        if (u.kind == 0)
        {
            ++chars[u.faction];
            CHECK(u.hp == u.maxHp);
            CHECK(u.home == s.board.hub[U(u.faction)]);
            CHECK(Steps(s.board, u.cell, s.board.hub[U(u.faction)]) <= 2);
            CHECK(u.abilities[0] >= 0);
        }
        else if (def.monsters[U(u.def)].tier == Tier::Guardian)
        {
            ++guards[u.faction];
            CHECK(u.cell == s.board.door[U(u.faction)]);
        }
        else // a den's monster is its half's side's (side 0: the bottom); the centre's is no one's
            CHECK(u.faction == (u.home / s.board.width == s.board.height / 2 ? -1 : u.home / s.board.width > s.board.height / 2 ? 0 : 1));
    }
    CHECK(chars[0] == 3);
    CHECK(chars[1] == 3);
    CHECK(guards[0] == 1);
    CHECK(guards[1] == 1);
    for (const auto *dens : {&s.board.weakDens, &s.board.strongDens})
        for (int d : *dens)
            CHECK(std::ranges::any_of(s.units, [&](const Unit &u) { return u.cell == d && u.kind == 1; }));
    CHECK(s.stage == (s.board.height / 2) * s.board.width + s.board.width / 2);
    CHECK(TurnsLeft(def, s) == 30);
    CHECK(s.floor.size() == s.board.itemRooms.size());
    // Side 0 is what was picked, in order.
    for (std::size_t i = 0; i < picks.size(); ++i)
        CHECK(s.units[i].def == picks[i]);
}

TEST_CASE("The bot side never repeats a role (T281)", "[match][setup]")
{
    const MatchDef def = Data();
    std::set<int> seen;
    for (std::uint64_t seed = 1; seed <= 100; ++seed)
    {
        const std::vector<int> pick = RiggedPick(def, seed, 3);
        REQUIRE(pick.size() == 3);
        std::set<Role> roles;
        for (int c : pick)
        {
            roles.insert(def.characters[U(c)].role);
            CHECK(def.characters[U(c)].role != Role::Speedster);
            seen.insert(c);
        }
        CHECK(roles.size() == 3);
    }
    CHECK(seen.size() == 4); // every allowed character turns up
}

TEST_CASE("Turn order follows Speed, ties by the seed (T282)", "[match][setup]")
{
    const MatchDef def = Data();
    const std::vector<int> picks{Char(def, "bao"), Char(def, "huy"), Char(def, "mai")}; // bao and huy share speed 4
    bool baoFirst = false, huyFirst = false;
    for (std::uint64_t seed = 1; seed <= 50; ++seed)
    {
        const MatchState s = NewMatch(def, seed, picks);
        REQUIRE(s.order.size() == s.units.size()); // every unit, monsters included (campaign plan 2)
        for (std::size_t i = 1; i < s.order.size(); ++i)
            CHECK(s.units[U(s.order[i - 1])].speed >= s.units[U(s.order[i])].speed);
        const auto at = [&](int unit) { return std::ranges::find(s.order, unit) - s.order.begin(); };
        (at(0) < at(1) ? baoFirst : huyFirst) = true;
        CHECK(NewMatch(def, seed, picks).order == s.order);
    }
    CHECK(baoFirst);
    CHECK(huyFirst);
}
