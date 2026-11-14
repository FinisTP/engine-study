#include "NightTestData.hpp"

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
    const Beat beat = Step(def, state, {ActionKind::Move, 0}, 0, captions);
    CHECK(beat.seen);
    CHECK(beat.guest == bot);
    CHECK(beat.camera == state.guests[static_cast<std::size_t>(bot)].cell);
    CHECK(beat.path == std::vector<int>{state.foyer, state.guests[static_cast<std::size_t>(bot)].cell});
    CHECK(beat.caption.find(def.guests[static_cast<std::size_t>(bot)].name) != std::string::npos);
    CHECK(beat.caption.find("opens") != std::string::npos);
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
    const Beat beat = Step(def, state, {ActionKind::Move, 0}, 0, Lines());
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
    REQUIRE(Apply(def, state, {ActionKind::Move, 0})); // into the dark
    const Beat beat = Step(def, state, {ActionKind::Move, 0}, 0, Lines());
    CHECK_FALSE(beat.seen);
    CHECK(beat.caption.empty());
    CHECK(beat.path.empty());
    CHECK(beat.camera == -1);
    (void)bot;
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
