#include <Cafe.hpp>
#include <Career.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>

using namespace umbra::cafe;

namespace
{
    CafeDef Data()
    {
        std::vector<umbra::core::Diagnostic> diagnostics;
        CafeDef def;
        REQUIRE(LoadCafe(UMBRA_CAFE_DATA, def, diagnostics));
        return def;
    }

    int StationOf(const CafeDef &def, const char *id)
    {
        for (int i = 0; i < static_cast<int>(def.stations.size()); ++i)
            if (def.stations[static_cast<std::size_t>(i)].id == id)
                return i;
        return -1;
    }

    // Ticks until a guest sits, returns its seat.
    int FirstSeated(const CafeDef &def, Shift &s)
    {
        for (int i = 0; i < 400; ++i)
        {
            for (int seat = 0; seat < static_cast<int>(s.seats.size()); ++seat)
                if (s.seats[static_cast<std::size_t>(seat)].guest >= 0)
                    return seat;
            Tick(def, s, 0.25F);
        }
        return -1;
    }

    // Makes the seat's order at the stations.
    void Make(const CafeDef &def, Shift &s, int seat)
    {
        for (int step : def.recipes[static_cast<std::size_t>(s.seats[static_cast<std::size_t>(seat)].recipe)].steps)
            REQUIRE(UseStation(def, s, step));
    }
}

TEST_CASE("The cafe's data loads: ten ghosts, recipes of one to three steps (T360)", "[cafe]")
{
    const CafeDef def = Data();
    CHECK(def.guests.size() >= 10);
    CHECK(def.stations.size() >= 3);
    for (const Recipe &r : def.recipes)
    {
        CHECK(!r.steps.empty());
        CHECK(static_cast<int>(r.steps.size()) <= def.rules.maxSteps);
        CHECK(r.price > 0);
    }
    for (const Guest &g : def.guests)
        CHECK_FALSE(g.likes.empty());
    // A broken file is refused with its place.
    const auto dir = std::filesystem::temp_directory_path() / "cafe-broken";
    std::filesystem::remove_all(dir);
    std::filesystem::copy(UMBRA_CAFE_DATA, dir, std::filesystem::copy_options::recursive);
    std::ofstream(dir / "recipes.csv", std::ios::app) << "bad,Bad,nothing;else,3\n";
    CafeDef broken;
    std::vector<umbra::core::Diagnostic> diagnostics;
    CHECK_FALSE(LoadCafe(dir, broken, diagnostics));
    REQUIRE_FALSE(diagnostics.empty());
    CHECK(diagnostics[0].field.starts_with("recipes.csv:"));
}

TEST_CASE("A shift: ghosts sit and order; the right drink pays and tips, a wrong one loses the guest (T361)", "[cafe]")
{
    const CafeDef def = Data();
    Shift s = OpenShop(def, 7);
    CHECK(s.seats.size() == static_cast<std::size_t>(def.rules.seats));
    // 1. Someone comes and orders.
    const int seat = FirstSeated(def, s);
    REQUIRE(seat >= 0);
    CHECK(s.seats[static_cast<std::size_t>(seat)].recipe >= 0);
    CHECK(s.log.front().kind == CafeEvent::Arrived);
    // 2. Made right and served at once: price plus the full tip.
    Make(def, s, seat);
    const int price = def.recipes[static_cast<std::size_t>(s.seats[static_cast<std::size_t>(seat)].recipe)].price;
    REQUIRE(Serve(def, s, seat));
    CHECK(s.takings == price);
    CHECK(s.tips > 0);
    CHECK(s.tips <= def.rules.tipMax);
    CHECK(s.served == 1);
    CHECK(s.cup.empty());
    CHECK(s.seats[static_cast<std::size_t>(seat)].guest < 0);
    // 3. The next one gets the wrong drink: it leaves, unpaid.
    const int next = FirstSeated(def, s);
    REQUIRE(next >= 0);
    const int wrongStep = def.recipes[static_cast<std::size_t>(s.seats[static_cast<std::size_t>(next)].recipe)].steps[0] == StationOf(def, "tea") ? StationOf(def, "phin") : StationOf(def, "tea");
    REQUIRE(UseStation(def, s, wrongStep));
    REQUIRE(UseStation(def, s, wrongStep));
    REQUIRE(Serve(def, s, next));
    CHECK(s.log.back().kind == CafeEvent::Wrong);
    CHECK(s.takings == price);
    CHECK(s.lost == 1);
    CHECK(s.seats[static_cast<std::size_t>(next)].guest < 0);
}

TEST_CASE("Patience runs out, the cup holds at most its steps, the shift closes at its time (T362)", "[cafe]")
{
    const CafeDef def = Data();
    Shift s = OpenShop(def, 3);
    const int seat = FirstSeated(def, s);
    REQUIRE(seat >= 0);
    const int guest = s.seats[static_cast<std::size_t>(seat)].guest;
    // 1. Waiting longer than its patience: it leaves, counted lost.
    for (float t = 0.0F; t < def.guests[static_cast<std::size_t>(guest)].patience + 1.0F && s.seats[static_cast<std::size_t>(seat)].guest == guest; t += 0.5F)
        Tick(def, s, 0.5F);
    CHECK(s.seats[static_cast<std::size_t>(seat)].guest != guest);
    CHECK(s.lost >= 1);
    // 2. The cup is full at maxSteps; Trash empties it; serving an empty seat does nothing.
    for (int i = 0; i < def.rules.maxSteps; ++i)
        REQUIRE(UseStation(def, s, 0));
    CHECK_FALSE(UseStation(def, s, 0));
    Trash(s);
    CHECK(s.cup.empty());
    // 3. Past closing time no one new comes; when the seats empty the shift is over.
    for (int i = 0; i < 4000 && !s.over; ++i)
        Tick(def, s, 0.25F);
    CHECK(s.over);
    CHECK(s.clock >= def.rules.shiftSeconds);
    CHECK(s.log.back().kind == CafeEvent::Closed);
    CHECK_FALSE(UseStation(def, s, 0));
}

TEST_CASE("A career keeps money and each character's progress; a save of another version is refused (T363)", "[cafe]")
{
    Career c;
    c.money = 42, c.day = 3;
    CharacterProgress p;
    p.id = "mai", p.level = 3, p.xp = 9, p.maxHp = 26, p.attack = 3, p.abilities = {"tap", "tend", "bloom"};
    p.boost[1] = 2;
    c.characters.push_back(p);
    c.heard = {"usher:mai:0"};
    const auto file = std::filesystem::temp_directory_path() / "career-test.json";
    std::string error;
    REQUIRE(SaveCareer(file, c, error));
    Career back;
    REQUIRE(LoadCareer(file, back, error));
    CHECK(back.money == 42);
    CHECK(back.day == 3);
    REQUIRE(back.characters.size() == 1);
    CHECK(back.characters[0].abilities == p.abilities);
    CHECK(back.characters[0].boost[1] == 2);
    CHECK(back.Find("mai") != nullptr);
    CHECK(back.Find("bao") == nullptr);
    CHECK(back.heard == c.heard);
    // Another version, or a broken file, is refused and the career untouched.
    std::ofstream(file, std::ios::trunc) << R"({"version": 99, "money": 5})";
    Career other;
    other.money = 7;
    CHECK_FALSE(LoadCareer(file, other, error));
    CHECK(other.money == 7);
    CHECK(error.find("version") != std::string::npos);
}
