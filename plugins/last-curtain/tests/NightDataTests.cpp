#include <NightDef.hpp>

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>

using namespace umbra::night;
namespace fs = std::filesystem;

namespace
{
    std::string ReadText(const fs::path &file)
    {
        std::ifstream in(file, std::ios::binary);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    void WriteText(const fs::path &file, const std::string &text)
    {
        std::ofstream(file, std::ios::binary) << text;
    }

    // A fresh copy of the shipped data to break.
    fs::path CopyData(const std::string &name)
    {
        const fs::path folder = fs::temp_directory_path() / ("umbra-night-" + name);
        fs::remove_all(folder);
        fs::copy(UMBRA_NIGHT_DATA, folder);
        return folder;
    }

    void Replace(const fs::path &file, const std::string &from, const std::string &to)
    {
        std::string text = ReadText(file);
        const std::size_t at = text.find(from);
        REQUIRE(at != std::string::npos);
        text.replace(at, from.size(), to);
        WriteText(file, text);
    }

    // Loads the folder, expects a refusal, returns the first diagnostic.
    umbra::core::Diagnostic Refused(const fs::path &folder)
    {
        std::vector<umbra::core::Diagnostic> diagnostics;
        NightDef def;
        CHECK_FALSE(LoadNight(folder, def, diagnostics));
        REQUIRE_FALSE(diagnostics.empty());
        return diagnostics.front();
    }
}

TEST_CASE("The shipped data loads and is complete (T94)", "[night][data]")
{
    std::vector<umbra::core::Diagnostic> diagnostics;
    NightDef def;
    REQUIRE(LoadNight(UMBRA_NIGHT_DATA, def, diagnostics));
    CHECK(diagnostics.empty());
    CHECK(def.guests.size() == 7);
    CHECK(def.powers.size() == 7);
    CHECK(def.omens.size() == 5);
    CHECK(def.rules.clock.rounds == 20);
    CHECK(def.rules.carry == 4);
    CHECK(def.guests[static_cast<std::size_t>(NightDef::Find(def.guests, "khang"))].might == 7);
    int rooms = 0;
    for (const RoomDef &room : def.rooms)
        rooms += room.count;
    CHECK(rooms >= def.rules.board.rooms);
}

TEST_CASE("Bad data is refused with file, row and column (T94)", "[night][data]")
{
    SECTION("a missing file")
    {
        const fs::path folder = CopyData("missing");
        fs::remove(folder / "items.csv");
        const auto d = Refused(folder);
        CHECK(d.code == "night.items");
        CHECK(d.field == "items.csv");
    }
    SECTION("text where a number goes")
    {
        const fs::path folder = CopyData("text");
        Replace(folder / "guests.csv", "thanh,Le Thanh,she,magical,20,2,", "thanh,Le Thanh,she,magical,20,x,");
        const auto d = Refused(folder);
        CHECK(d.code == "night.guests");
        CHECK(d.field == "guests.csv:2:might");
    }
    SECTION("a blank cell")
    {
        const fs::path folder = CopyData("blank");
        Replace(folder / "guests.csv", "nhi,Truong Uyen Nhi,she,physical,16,", "nhi,Truong Uyen Nhi,she,physical,,");
        CHECK(Refused(folder).field == "guests.csv:1:hp");
    }
    SECTION("an unknown id")
    {
        const fs::path folder = CopyData("unknown");
        Replace(folder / "guests.csv", ",line\n", ",nosuch\n");
        CHECK(Refused(folder).field == "guests.csv:1:power");
    }
    SECTION("no under-stage")
    {
        const fs::path folder = CopyData("understage");
        Replace(folder / "rooms.csv", "understage,Under-stage,understage,", "understage,Under-stage,plain,");
        CHECK(Refused(folder).code == "night.rooms");
    }
    SECTION("more carried items than a guest has slots")
    {
        const fs::path folder = CopyData("carry");
        Replace(folder / "rules.json", "\"carry\": 4", "\"carry\": 9");
        const auto d = Refused(folder);
        CHECK(d.code == "night.rules");
        CHECK(d.field == "rules.json:carry");
    }
    SECTION("a row with too few cells")
    {
        const fs::path folder = CopyData("short");
        Replace(folder / "omens.csv", "mask,The cracked mask,4,0,0,0", "mask,The cracked mask,4");
        CHECK(Refused(folder).field == "omens.csv:1");
    }
    SECTION("a guest without five questions")
    {
        const fs::path folder = CopyData("questions");
        std::string text = ReadText(folder / "questions.csv");
        text.erase(text.rfind("\nlan,"));
        WriteText(folder / "questions.csv", text + "\n");
        CHECK(Refused(folder).code == "night.questions");
    }
    SECTION("a quoted field with a comma loads")
    {
        const fs::path folder = CopyData("quoted");
        Replace(folder / "omens.csv", "mask,The cracked mask,", "mask,\"The mask, cracked\",");
        std::vector<umbra::core::Diagnostic> diagnostics;
        NightDef def;
        REQUIRE(LoadNight(folder, def, diagnostics));
        CHECK(def.omens[0].name == "The mask, cracked");
    }
}
