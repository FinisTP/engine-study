#include "NightTestData.hpp"

#include <BoardView.hpp>
#include <Narrate.hpp>

#include <fstream>

using namespace umbra::night;

namespace
{
    Captions Lines()
    {
        std::vector<umbra::core::Diagnostic> diagnostics;
        Captions captions;
        REQUIRE(LoadCaptions(std::filesystem::path(UMBRA_NIGHT_DATA) / "captions.json", captions, diagnostics));
        return captions;
    }

    // A legal move of the current guest into a room still holding its event (the map is whole).
    Action Fresh(const NightDef &def, const NightState &state)
    {
        const int at = state.guests[static_cast<std::size_t>(Current(state))].cell;
        for (const Action &a : LegalActions(def, state))
            if (a.kind == ActionKind::Move && a.a < transportMove && !state.cells[static_cast<std::size_t>(Neighbour(def, at, a.a))].resolved)
                return a;
        FAIL("no fresh room");
        return {};
    }

    // Applies `action` for the current guest and narrates it for `viewer`.
    Beat Step(const NightDef &def, NightState &state, const Action &action, int viewer, const Captions &captions)
    {
        const NightState before = state;
        const int actor = Current(state);
        const std::size_t since = state.logCount;
        REQUIRE(Apply(def, state, action));
        return Narrate(def, before, state, NewLog(state, since), action, actor, viewer, captions);
    }
}

TEST_CASE("A bot's turn in light has a caption, a path and a result (T109)", "[night][narrate]")
{
    NightDef def = Data();
    for (RoomDef &room : def.rooms)
        if (room.kind != RoomKind::Foyer && room.kind != RoomKind::UnderStage)
            room.kind = RoomKind::Plain, room.doors = 15;
    NightState state = FoyerNight(def, 1, 0);
    while (Current(state) == 0)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    const int bot = Current(state);
    const Captions captions = Lines();
    const Beat beat = Step(def, state, Fresh(def, state), 0, captions);
    CHECK(beat.seen);
    CHECK(beat.guest == bot);
    CHECK(beat.camera == state.guests[static_cast<std::size_t>(bot)].cell);
    CHECK(beat.path == std::vector<int>{state.foyer, state.guests[static_cast<std::size_t>(bot)].cell});
    CHECK(beat.caption.find(def.guests[static_cast<std::size_t>(bot)].name) != std::string::npos);
    CHECK_FALSE(beat.caption.empty());
}

TEST_CASE("A roll is told with its die, stat and target (T109)", "[night][narrate]")
{
    NightDef def = Data();
    for (RoomDef &room : def.rooms)
        if (room.kind != RoomKind::Foyer && room.kind != RoomKind::UnderStage)
            room.kind = RoomKind::Event, room.doors = 15;
    NightState state = FoyerNight(def, 2, 0);
    while (Current(state) == 0)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    const Beat beat = Step(def, state, Fresh(def, state), 0, Lines());
    CHECK(beat.result.find("rolled") != std::string::npos);
    CHECK(beat.result.find("against") != std::string::npos);
}

TEST_CASE("A turn in the dark shows nothing but the opened room (T109)", "[night][narrate]")
{
    NightDef def = Data();
    def.rules.light.steps = 0;
    for (RoomDef &room : def.rooms)
        if (room.kind != RoomKind::Foyer && room.kind != RoomKind::UnderStage)
            room.kind = RoomKind::Plain, room.doors = 15;
    NightState state = FoyerNight(def, 1, 0);
    while (Current(state) == 0)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    const int bot = Current(state);
    REQUIRE(Apply(def, state, Fresh(def, state))); // into the dark
    const Beat beat = Step(def, state, Fresh(def, state), 0, Lines());
    CHECK_FALSE(beat.seen);
    CHECK(beat.caption.empty());
    CHECK(beat.path.empty());
    CHECK(beat.camera == -1);
    (void)bot;
}

TEST_CASE("A bot that walks out of the light into a dark room: the walk shows, the room's deed does not (T217)", "[night][narrate]")
{
    NightDef def = Data();
    def.rules.light.steps = 0; // the viewer lights only their own room
    for (RoomDef &room : def.rooms)
        if (room.kind != RoomKind::Foyer && room.kind != RoomKind::UnderStage)
            room.kind = RoomKind::Event, room.doors = 15;
    NightState state = FoyerNight(def, 2, 0);
    while (Current(state) == 0)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    const Beat beat = Step(def, state, Fresh(def, state), 0, Lines());
    CHECK(beat.seen);            // it left the viewer's room
    CHECK(beat.dark);            // ... into the dark
    CHECK(beat.result.empty());  // what the room did is not told
}

TEST_CASE("With the light off everyone sees everything (T218)", "[night][narrate]")
{
    NightDef def = Data();
    def.rules.light.steps = 0;
    for (RoomDef &room : def.rooms)
        if (room.kind != RoomKind::Foyer && room.kind != RoomKind::UnderStage)
            room.kind = RoomKind::Event, room.doors = 15;
    NightState state = FoyerNight(def, 2, 0);
    while (Current(state) == 0)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    const NightState before = state;
    const int actor = Current(state);
    const Action move = Fresh(def, state);
    REQUIRE(Apply(def, state, move));
    const Beat beat = Narrate(def, before, state, NewLog(state, before.logCount), move, actor, 0, Lines(), false);
    CHECK(beat.seen);
    CHECK_FALSE(beat.dark);
    CHECK(beat.result.find("rolled") != std::string::npos);
    // The board shows every opened room lit and every guest.
    const BoardView view = MakeView(def, state, 0, false);
    for (int c = 0; c < maxCells; ++c)
        CHECK(view.cells[static_cast<std::size_t>(c)].lit == state.cells[static_cast<std::size_t>(c)].opened);
    CHECK(static_cast<int>(view.pawns.size()) == state.count);
    CHECK(MakeView(def, state, 0, false).cells[static_cast<std::size_t>(state.foyer)].step == MakeView(def, state, 0).cells[static_cast<std::size_t>(state.foyer)].step);
}

TEST_CASE("Every action kind has a caption line (T109)", "[night][narrate]")
{
    Lines();
    const std::filesystem::path file = std::filesystem::temp_directory_path() / "umbra-captions.json";
    std::ofstream(file) << R"({"move": "{guest} moves."})";
    std::vector<umbra::core::Diagnostic> diagnostics;
    Captions captions;
    CHECK_FALSE(LoadCaptions(file, captions, diagnostics));
    REQUIRE_FALSE(diagnostics.empty());
    CHECK(diagnostics.front().code == "night.captions");
}

TEST_CASE("A passed answer tells the goal (T129)", "[night][narrate]")
{
    const NightDef def = Data();
    const NightState state = FoyerNight(def, 1);
    LogEntry passed{};
    passed.kind = LogKind::Persuade, passed.who = 1, passed.other = 2, passed.a = 1, passed.b = 1;
    const Beat beat = Narrate(def, state, state, {passed}, {ActionKind::Answer, 0}, 1, -1, Lines());
    CHECK(beat.result.find(def.guests[2].name + " tells " + def.guests[1].name) != std::string::npos);
    LogEntry failed = passed;
    failed.a = 0;
    CHECK(Narrate(def, state, state, {failed}, {ActionKind::Answer, 0}, 1, -1, Lines()).result.find("tells") == std::string::npos);
}
