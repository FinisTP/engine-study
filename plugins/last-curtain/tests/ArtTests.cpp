#include "NightTestData.hpp"

#include <Art.hpp>
#include <Play.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

using namespace umbra::night;

namespace
{
    int RoomCell(const NightDef &def, const NightState &state, const char *room)
    {
        const int r = NightDef::Find(def.rooms, room);
        for (int c = 0; c < maxCells; ++c)
            if (state.cells[static_cast<std::size_t>(c)].room == r)
                return c;
        return -1;
    }
}

TEST_CASE("Featured rooms: foyer, under-stage, the night's start rooms and goal rooms (T140)", "[night][art]")
{
    const NightDef def = Data();
    const NightState start = NewNight(def, 21, 0);
    // 1. The start cells are where the guests began.
    const std::vector<int> starts = StartCells(def, start);
    REQUIRE(starts.size() == def.guests.size());
    for (std::size_t g = 0; g < starts.size(); ++g)
        CHECK(starts[g] == start.guests[g].cell);
    // 2. Later in the night (guests moved, saved and restored) they are the same.
    Play play(def, start);
    for (int i = 0; i < 60; ++i)
        play.PlayerTurn() ? play.Key(umbra::platform::InputKey::Enter) : play.Update(10.0F);
    NightState restored;
    REQUIRE(Play::Restore(def, play.Save(), restored));
    CHECK(StartCells(def, restored) == starts);
    // 3. Featured: those cells, the foyer, the under-stage, goal rooms; not a plain room.
    for (const int cell : starts)
        CHECK(Featured(def, restored, cell));
    CHECK(Featured(def, restored, restored.foyer));
    for (int c = 0; c < maxCells; ++c)
    {
        const int room = restored.cells[static_cast<std::size_t>(c)].room;
        if (room < 0 || std::find(starts.begin(), starts.end(), c) != starts.end())
            continue;
        const std::string &id = def.rooms[static_cast<std::size_t>(room)].id;
        const bool goal = std::any_of(def.goals.begin(), def.goals.end(), [&](const GoalDef &g) { return g.kind == GoalKind::Room && g.target == room; });
        const bool expected = id == "foyer" || id == "understage" || goal;
        CHECK(Featured(def, restored, c) == expected);
    }
    CHECK(RoomCell(def, restored, "foyer") == restored.foyer);
}

TEST_CASE("ArtPath names a picture by its data id; MissingArt lists what is not there (T140)", "[night][art]")
{
    CHECK(ArtPath("rooms", "foyer") == std::filesystem::path("art/rooms/foyer.jpg"));
    CHECK(ArtPath("cards", "moths") == std::filesystem::path("art/cards/moths.png"));
    const NightDef def = Data();
    const std::filesystem::path folder = std::filesystem::temp_directory_path() / "umbra-art-test";
    std::filesystem::remove_all(folder);
    const std::vector<std::string> missing = MissingArt(def, folder);
    CHECK(std::find(missing.begin(), missing.end(), "art/rooms/foyer.jpg") != missing.end());
    CHECK(std::find(missing.begin(), missing.end(), "art/events/applause.jpg") != missing.end());
    CHECK(std::find(missing.begin(), missing.end(), "art/portraits/thanh.jpg") != missing.end());
    std::filesystem::create_directories(folder / "art/rooms");
    std::ofstream(folder / "art/rooms/foyer.jpg") << "x";
    const std::vector<std::string> fewer = MissingArt(def, folder);
    CHECK(std::find(fewer.begin(), fewer.end(), "art/rooms/foyer.jpg") == fewer.end());
    CHECK(fewer.size() == missing.size() - 1);
    std::filesystem::remove_all(folder);
}
