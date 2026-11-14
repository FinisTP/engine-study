#include "NightTestData.hpp"

#include <algorithm>

using namespace umbra::night;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }
    std::int8_t I8(int i) { return static_cast<std::int8_t>(i); }

    NightDef Quiet()
    {
        NightDef def = Data();
        def.rules.monsters.chancePercent = 0;
        return def;
    }

    void SetGoal(const NightDef &def, NightState &state, int guest, const char *id)
    {
        const int goal = NightDef::Find(def.goals, id);
        REQUIRE(goal >= 0);
        state.guests[U(guest)].goal = I8(goal);
    }

    // Every guest passes until the hunt begins.
    void PassToMidnight(const NightDef &def, NightState &state)
    {
        while (Current(state) >= 0 && state.round <= def.rules.clock.turnAfterRound)
        {
            const auto legal = LegalActions(def, state);
            REQUIRE(Apply(def, state, legal[0].kind == ActionKind::LevelUp ? legal[0] : Action{ActionKind::EndTurn}));
        }
    }

    // Gives every guest a goal it has already done (an item it carries).
    void AllDone(const NightDef &def, NightState &state)
    {
        const int item = NightDef::Find(def.items, "crown");
        for (int g = 0; g < state.count; ++g)
        {
            SetGoal(def, state, g, "thanh-crown");
            state.guests[U(g)].items[0] = I8(item);
        }
    }
}

TEST_CASE("Each guest draws one of its own goals, the same for the same seed (T117)", "[night][goal]")
{
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed <= 50; ++seed)
    {
        const NightState a = NewNight(def, seed), b = NewNight(def, seed);
        for (int g = 0; g < a.count; ++g)
        {
            const int goal = a.guests[U(g)].goal;
            REQUIRE(goal >= 0);
            CHECK(def.goals[U(goal)].guest == g);
            CHECK(b.guests[U(g)].goal == goal);
            CHECK(a.guests[U(g)].knows == (1u << g)); // only its own
        }
    }
}

TEST_CASE("Each goal kind is checked (T117)", "[night][goal]")
{
    const NightDef def = Quiet();
    NightState state = NewNight(def, 3);
    SECTION("room: entered once is enough")
    {
        SetGoal(def, state, 0, "nhi-box");
        CHECK_FALSE(GoalDone(def, state, 0));
        state.guests[0].goalRoomSeen = true;
        CHECK(GoalDone(def, state, 0));
    }
    SECTION("item: only while carried")
    {
        SetGoal(def, state, 3, "khang-ledger");
        CHECK_FALSE(GoalDone(def, state, 3));
        state.guests[3].items[1] = I8(NightDef::Find(def.items, "ledger"));
        CHECK(GoalDone(def, state, 3));
        state.guests[3].items[1] = -1;
        CHECK_FALSE(GoalDone(def, state, 3));
    }
    SECTION("ally: only while in the same group")
    {
        SetGoal(def, state, 2, "bao-nhi");
        CHECK_FALSE(GoalDone(def, state, 2));
        state.guests[0].group = state.guests[2].group = 3;
        CHECK(GoalDone(def, state, 2));
    }
    SECTION("events and monsters: the count")
    {
        SetGoal(def, state, 5, "huy-events");
        const int need = def.goals[U(state.guests[5].goal)].count;
        state.guests[5].goalCount = I8(need - 1);
        CHECK_FALSE(GoalDone(def, state, 5));
        state.guests[5].goalCount = I8(need);
        CHECK(GoalDone(def, state, 5));
    }
}

TEST_CASE("Entering the goal room marks it seen (T117)", "[night][goal]")
{
    NightDef def = Quiet();
    // Every deck room is the Patron's box with four doors: the first new room is it.
    const int box = NightDef::Find(def.rooms, "patronbox");
    def.rooms[U(box)].doors = 15;
    NightState state = FoyerNight(def, 2, 0);
    for (std::int8_t &r : state.deck)
        r = I8(box);
    const int g = Current(state);
    SetGoal(def, state, g, "nhi-box");
    for (std::int8_t d = 0; d < 4; ++d)
        if (Doorway(def, state, state.guests[U(g)].cell, d))
        {
            REQUIRE(Apply(def, state, {ActionKind::Move, d}));
            break;
        }
    CHECK(state.guests[U(g)].goalRoomSeen);
    CHECK(GoalDone(def, state, g));
}

TEST_CASE("A passed event roll counts; a trap does not (T117)", "[night][goal]")
{
    NightDef def = Quiet();
    for (EventDef &e : def.events)
        e.target = 1; // always passed
    for (RoomDef &room : def.rooms)
        if (room.kind != RoomKind::Foyer && room.kind != RoomKind::UnderStage)
            room.kind = RoomKind::Event, room.doors = 15;
    NightState state = FoyerNight(def, 2, 0);
    const int g = Current(state);
    SetGoal(def, state, g, "huy-events");
    for (std::int8_t d = 0; d < 4; ++d)
        if (Doorway(def, state, state.guests[U(g)].cell, d))
        {
            REQUIRE(Apply(def, state, {ActionKind::Move, d}));
            break;
        }
    CHECK(state.guests[U(g)].goalCount == 1);
}

TEST_CASE("A keepsake waits in its room only when a drawn goal needs it (T117)", "[night][goal]")
{
    NightDef def = Quiet();
    const int costumes = NightDef::Find(def.rooms, "costumes"), crown = NightDef::Find(def.items, "crown");
    def.rooms[U(costumes)].doors = 15;
    for (int needed = 0; needed < 2; ++needed)
    {
        NightState state = FoyerNight(def, 5, 0);
        for (std::int8_t &r : state.deck)
            r = I8(costumes);
        for (int g = 0; g < state.count; ++g)
            SetGoal(def, state, g, needed && g == 1 ? "thanh-crown" : "huy-events");
        const int g = Current(state);
        for (std::int8_t d = 0; d < 4; ++d)
            if (Doorway(def, state, state.guests[U(g)].cell, d))
            {
                REQUIRE(Apply(def, state, {ActionKind::Move, d}));
                break;
            }
        const bool onFloor = std::any_of(state.floor.begin(), state.floor.end(), [&](const FloorItem &f) { return f.item == crown; });
        const bool carried = std::find(state.guests[U(g)].items.begin(), state.guests[U(g)].items.end(), I8(crown)) != state.guests[U(g)].items.end();
        CHECK((onFloor || carried) == (needed == 1));
    }
}

TEST_CASE("A keepsake changes hands and counts for its new holder (T117)", "[night][goal]")
{
    const NightDef def = Quiet();
    NightState state = NewNight(def, 3);
    const int crown = NightDef::Find(def.items, "crown");
    SetGoal(def, state, 1, "thanh-crown");
    SetGoal(def, state, 6, "lan-crown");
    state.guests[1].items[0] = I8(crown);
    CHECK(GoalDone(def, state, 1));
    CHECK_FALSE(GoalDone(def, state, 6));
    state.guests[1].items[0] = -1;
    state.guests[6].items[0] = I8(crown);
    CHECK_FALSE(GoalDone(def, state, 1));
    CHECK(GoalDone(def, state, 6));
}

TEST_CASE("At midnight the Taken comes from the failed (T117)", "[night][goal]")
{
    const NightDef def = Quiet();
    std::array<int, 2> drawn{};
    for (std::uint64_t seed = 1; seed <= 400; ++seed)
    {
        NightState state = NewNight(def, seed);
        AllDone(def, state);
        const int a = state.order[1], b = state.order[2];
        state.guests[U(a)].items[0] = state.guests[U(b)].items[0] = -1; // a and b fail
        state.guests[U(a)].hp = state.guests[U(b)].hp = 9;
        PassToMidnight(def, state);
        REQUIRE(state.taken.present);
        const int t = state.taken.guest, other = t == a ? b : a;
        REQUIRE((t == a || t == b));
        ++drawn[t == a ? 0 : 1];
        CHECK(state.guests[U(other)].hp == std::min(9, def.rules.goals.failHp));
    }
    CHECK(drawn[0] >= 160);
    CHECK(drawn[1] >= 160);
}

TEST_CASE("A downed failed guest keeps its HP rule unless drawn (T117)", "[night][goal]")
{
    const NightDef def = Quiet();
    NightState state = NewNight(def, 7);
    AllDone(def, state);
    const int a = state.order[1], b = state.order[2];
    state.guests[U(a)].items[0] = state.guests[U(b)].items[0] = -1;
    state.guests[U(b)].down = true, state.guests[U(b)].hp = 0;
    PassToMidnight(def, state);
    REQUIRE(state.taken.present);
    if (state.taken.guest == a)
        CHECK(state.guests[U(b)].hp == def.rules.guests.downedReturnHp); // stands, as every downed guest
}

TEST_CASE("If every living guest finished, the company wins at midnight (T117)", "[night][goal]")
{
    const NightDef def = Quiet();
    NightState state = NewNight(def, 9);
    AllDone(def, state);
    PassToMidnight(def, state);
    CHECK(state.over);
    CHECK(state.outcome == Outcome::Company);
    CHECK_FALSE(state.taken.present);
}

TEST_CASE("A passed chat answer reveals the asker's goal (T117)", "[night][goal]")
{
    NightDef def = Quiet();
    for (QuestionDef &q : def.questions)
        q.physicalTarget = q.magicalTarget = 1; // always passed
    NightState state = NewNight(def, 4, 0);
    while (Current(state) != 0)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    const int other = state.order[0] == 0 ? state.order[1] : state.order[0];
    state.guests[U(other)].cell = state.guests[0].cell;
    REQUIRE(Apply(def, state, {ActionKind::Chat, I8(other)}));
    REQUIRE(Apply(def, state, {ActionKind::Answer, 0}));
    CHECK((state.guests[0].knows >> other) & 1u);
}
