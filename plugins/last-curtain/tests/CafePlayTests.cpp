#include "MatchTestData.hpp"

#include <CafePlay.hpp>

#include <fstream>

using namespace umbra::cafe;
using umbra::night::CafePlay;

namespace
{
    CafeDef Cafe()
    {
        std::vector<umbra::core::Diagnostic> diagnostics;
        CafeDef def;
        REQUIRE(LoadCafe(UMBRA_CAFE_DATA, def, diagnostics));
        return def;
    }
    nlohmann::json Json(const std::filesystem::path &file)
    {
        std::ifstream in(file);
        return nlohmann::json::parse(in);
    }
    nlohmann::json Room() { return Json(std::filesystem::path(UMBRA_CAFE_DATA) / "room.json"); }
    nlohmann::json Words() { return Json(std::filesystem::path(UMBRA_LASTCURTAIN_ASSETS) / "cafe-words.json"); }

    // Walks straight to the first spot of a kind (and station or seat), by WalkTo and Update.
    int GoTo(CafePlay &p, const char *kind, const char *key = nullptr, int value = -1, const char *station = nullptr)
    {
        const nlohmann::json &spots = p.Room()["spots"];
        for (int i = 0; i < static_cast<int>(spots.size()); ++i)
        {
            const nlohmann::json &s = spots[static_cast<std::size_t>(i)];
            if (s["kind"] != kind || (key && s[key] != value) || (station && s["station"] != station))
                continue;
            p.WalkTo({s["at"][0].get<float>(), s["at"][1].get<float>()});
            for (int k = 0; k < 400 && p.Near() != i; ++k)
                p.Update(0.05F);
            return p.Near() == i ? i : -1;
        }
        return -1;
    }
}

TEST_CASE("The walker moves over the floor, never through the counter or a wall; E needs a spot in reach (T365)", "[cafe][play]")
{
    const CafeDef def = Cafe();
    const umbra::match::MatchDef match = Data();
    Career career;
    CafePlay p(def, match, Room(), Words(), career, "mai", 1);
    const glm::vec2 start = p.Position();
    p.Move({1.0F, 0.0F}, 0.5F);
    CHECK(p.Position().x > start.x);
    // 1. Toward the back: stopped at the counter's front, never inside it.
    for (int i = 0; i < 200; ++i)
        p.Move({0.0F, -1.0F}, 0.1F);
    CHECK(p.Position().y >= 1.0F);
    // 2. Toward the right: stopped at the wall.
    for (int i = 0; i < 200; ++i)
        p.Move({1.0F, 0.0F}, 0.1F);
    CHECK(p.Position().x <= p.Room()["size"][0].get<float>());
    // 3. Far from everything, E does nothing.
    p.WalkTo({6.0F, 6.5F});
    for (int i = 0; i < 200; ++i)
        p.Update(0.05F);
    CHECK(p.Near() < 0);
    CHECK_FALSE(p.Interact());
}

TEST_CASE("The sign opens the shop; stations fill the cup; a table serves; the day's takings go to the career once (T366)", "[cafe][play]")
{
    const CafeDef def = Cafe();
    const umbra::match::MatchDef match = Data();
    Career career;
    career.money = 5;
    CafePlay p(def, match, Room(), Words(), career, "mai", 3);
    // 1. The sign: open.
    REQUIRE(GoTo(p, "sign") >= 0);
    REQUIRE(p.Interact());
    REQUIRE(p.Open());
    // 2. Someone sits; make their drink and serve it.
    int seat = -1;
    for (int i = 0; i < 400 && seat < 0; ++i)
    {
        p.Update(0.1F);
        for (int k = 0; k < static_cast<int>(p.Shift().seats.size()); ++k)
            if (p.Shift().seats[static_cast<std::size_t>(k)].guest >= 0)
                seat = k;
    }
    REQUIRE(seat >= 0);
    const Recipe &r = def.recipes[static_cast<std::size_t>(p.Shift().seats[static_cast<std::size_t>(seat)].recipe)];
    for (int step : r.steps)
    {
        REQUIRE(GoTo(p, "station", nullptr, -1, def.stations[static_cast<std::size_t>(step)].id.c_str()) >= 0);
        REQUIRE(p.Interact());
    }
    CHECK(p.UiData()["cup"].size() == r.steps.size());
    REQUIRE(GoTo(p, "seat", "seat", seat) >= 0);
    REQUIRE(p.Interact());
    CHECK(p.Shift().served == 1);
    // 3. Closing: the sign again; the takings screen; the money lands once.
    REQUIRE(GoTo(p, "sign") >= 0);
    REQUIRE(p.Interact());
    CHECK(p.Screen() == "takings");
    const int day = p.Shift().takings + p.Shift().tips;
    CHECK(career.money == 5 + day);
    REQUIRE(p.HandleUi("takings", "ok", ""));
    CHECK(p.Screen().empty());
    CHECK(career.money == 5 + day);
    // 4. Once a day: the sign does not open again; the door leads to the next match.
    REQUIRE(p.Interact());
    CHECK_FALSE(p.Open());
    REQUIRE(GoTo(p, "door") >= 0);
    REQUIRE(p.Interact());
    CHECK(p.Leaving());
}

TEST_CASE("A served ghost talks with the walker, each talk once; the board shows the characters and changes the walker (T367)", "[cafe][play]")
{
    const CafeDef def = Cafe();
    const umbra::match::MatchDef match = Data();
    Career career;
    CafePlay p(def, match, Room(), Words(), career, "mai", 3);
    REQUIRE(GoTo(p, "sign") >= 0);
    REQUIRE(p.Interact());
    int seat = -1;
    for (int i = 0; i < 400 && seat < 0; ++i)
    {
        p.Update(0.1F);
        for (int k = 0; k < static_cast<int>(p.Shift().seats.size()); ++k)
            if (p.Shift().seats[static_cast<std::size_t>(k)].guest >= 0)
                seat = k;
    }
    REQUIRE(seat >= 0);
    for (int step : def.recipes[static_cast<std::size_t>(p.Shift().seats[static_cast<std::size_t>(seat)].recipe)].steps)
    {
        REQUIRE(GoTo(p, "station", nullptr, -1, def.stations[static_cast<std::size_t>(step)].id.c_str()) >= 0);
        REQUIRE(p.Interact());
    }
    REQUIRE(GoTo(p, "seat", "seat", seat) >= 0);
    REQUIRE(p.Interact());
    // 1. A talk begins: a line in a bubble; the career remembers it.
    REQUIRE_FALSE(p.UiData()["bubbles"].empty());
    CHECK(career.heard.size() == 1);
    // 2. The board: every character with level and abilities; choosing one makes it the walker.
    REQUIRE(GoTo(p, "board") >= 0);
    REQUIRE(p.Interact());
    CHECK(p.Screen() == "board");
    const nlohmann::json board = p.UiData()["board"];
    CHECK(board.size() == match.characters.size());
    CHECK_FALSE(board[0]["abilities"].get<std::string>().empty());
    REQUIRE(p.HandleUi("board", "walk", "bao"));
    CHECK(p.Walker() == "bao");
    REQUIRE(p.HandleUi("board", "close", ""));
    CHECK(p.Screen().empty());
}
