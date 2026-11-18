#include "MatchTestData.hpp"

#include <Match.hpp>

#include <algorithm>

using namespace umbra::match;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }
    bool Has(const std::vector<std::int16_t> &list, int cell) { return std::find(list.begin(), list.end(), cell) != list.end(); }
    bool Bottom(const Board &b, int cell) { return cell / b.width > b.height / 2; }
}

TEST_CASE("Boards are mirrored and fair on every seed (T277)", "[match][board]")
{
    const MatchDef def = Data();
    for (std::uint64_t seed = 1; seed <= 200; ++seed)
    {
        const Board b = BuildBoard(def, seed);
        REQUIRE(Fair(b));
        CHECK(b.cells[U(b.hub[0])].kind == CellKind::Hub);
        CHECK(b.cells[U(b.hub[1])].kind == CellKind::Hub);
        CHECK(Mirror(b, b.hub[0]) == b.hub[1]);
        CHECK(Mirror(b, b.door[0]) == b.door[1]);
        CHECK(Steps(b, b.hub[0], b.door[0]) == 1);
        CHECK(Steps(b, b.hub[0], b.hub[1]) > 0);
    }
    // Different seeds give different boards.
    CHECK_FALSE(std::ranges::equal(BuildBoard(def, 1).cells, BuildBoard(def, 2).cells,
                                   [](const Cell &a, const Cell &c) { return a.kind == c.kind && a.doors == c.doors && a.event == c.event; }));
}

TEST_CASE("Dens sit in their step bands from their own hub (T278)", "[match][board]")
{
    const MatchDef def = Data();
    const auto &r = def.rules.board;
    for (std::uint64_t seed = 1; seed <= 100; ++seed)
    {
        const Board b = BuildBoard(def, seed);
        int weak = 0, strong = 0;
        for (int d : b.weakDens)
            if (Bottom(b, d))
            {
                ++weak;
                CHECK(Steps(b, b.hub[0], d) >= r.weakMin);
                CHECK(Steps(b, b.hub[0], d) <= r.weakMax);
            }
        for (int d : b.strongDens)
            if (Bottom(b, d))
            {
                ++strong;
                CHECK(Steps(b, b.hub[0], d) >= r.strongMin);
                CHECK(Steps(b, b.hub[0], d) <= r.strongMax);
            }
        CHECK(weak == r.weakPerHalf);
        CHECK(strong == r.strongPerHalf);
        // Events: the count per half, eventGap apart, never on a den.
        std::vector<int> events;
        for (int c = 0; c < static_cast<int>(b.cells.size()); ++c)
            if (b.cells[U(c)].event >= 0 && Bottom(b, c))
                events.push_back(c);
        CHECK(static_cast<int>(events.size()) == r.eventsPerHalf);
        for (std::size_t i = 0; i < events.size(); ++i)
        {
            CHECK_FALSE(Has(b.weakDens, events[i]));
            CHECK_FALSE(Has(b.strongDens, events[i]));
            for (std::size_t j = i + 1; j < events.size(); ++j)
                CHECK(Steps(b, events[i], events[j]) >= r.eventGap);
        }
        CHECK(static_cast<int>(b.itemRooms.size()) == 2 * r.itemsPerHalf);
    }
}

TEST_CASE("Every board has at least two open stage rooms (T279)", "[match][board]")
{
    const MatchDef def = Data();
    for (std::uint64_t seed = 1; seed <= 200; ++seed)
    {
        const Board b = BuildBoard(def, seed);
        CHECK(b.stageRooms.size() >= 2);
        for (int s : b.stageRooms)
        {
            CHECK(b.cells[U(s)].kind == CellKind::Centre);
            CHECK(b.cells[U(s)].doors == 0xF);
            CHECK(Steps(b, b.hub[0], s) == Steps(b, b.hub[1], s));
        }
    }
}
