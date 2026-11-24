#include <Localization.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <regex>
#include <set>

using umbra::night::Localization;

namespace
{
    const std::filesystem::path assets{UMBRA_LASTCURTAIN_ASSETS};
}

TEST_CASE("localization.csv: one row per text, Vietnamese and English; nested words by prefix; lists by number (the user, 2026-11-24 19:40)", "[localization]")
{
    const std::filesystem::path file = std::filesystem::temp_directory_path() / "umbra-loc-test.csv";
    std::ofstream(file) << "key,vi,en\n"
                           "match.log.Blow,\"{a} đánh {b}, mất {c}\",\"{a} hits {b}, for {c}\"\n"
                           "match.lines.Down.0,Ngã rồi,Down\n"
                           "match.lines.Down.1,Tối quá,So dark\n"
                           "campaign.close,Đóng,\n";
    Localization loc;
    std::string error;
    REQUIRE(loc.Load(file, error));
    CHECK(loc.Get("match.log.Blow", false) == "{a} đánh {b}, mất {c}");
    CHECK(loc.Get("match.log.Blow", true) == "{a} hits {b}, for {c}");
    CHECK(loc.Get("campaign.close", true) == "Đóng");   // no English yet: the Vietnamese
    CHECK(loc.Get("nowhere", false) == "nowhere");      // a missing key shows itself
    const nlohmann::json words = loc.Words("match", false);
    CHECK(words["log"]["Blow"] == "{a} đánh {b}, mất {c}");
    REQUIRE(words["lines"]["Down"].is_array());
    CHECK(words["lines"]["Down"][1] == "Tối quá");
    CHECK(loc.Missing() == std::vector<std::string>{"campaign.close (en)"});
}

TEST_CASE("The shipped table has both languages everywhere; the campaign's screens show no text that is not in it (the user, 2026-11-24 19:40)", "[localization]")
{
    Localization loc;
    std::string error;
    REQUIRE(loc.Load(assets / "localization.csv", error));
    CHECK(loc.Missing().empty());
    // Every screen the campaign and its fights open: text comes from {words...} or data, never written in.
    const std::set<std::string> screens{"campaign-hud", "campaign-pause", "match-labels", "match-battle", "match-banner", "match-level", "match-end",
                                        "cafe-hub", "notebook", "brew", "worldmap", "prestage", "result", "gameover", "settings", "dialog", "dialog-log",
                                        "title", "newgame", "retreat", "keyboard"};
    const std::regex literal(R"re("(text|tip)": "([^"]*)")re");
    const std::regex token(R"(\{[^}]*\})");
    for (const std::string &name : screens)
    {
        std::ifstream in(assets / "ui" / (name + ".ui.json"));
        if (!in)
            continue;
        const std::string text((std::istreambuf_iterator<char>(in)), {});
        for (std::sregex_iterator m(text.begin(), text.end(), literal), end; m != end; ++m)
        {
            // What is left once tokens, numbers and signs are taken out must be nothing (or HP, XP, Lv).
            std::string rest = std::regex_replace((*m)[2].str(), token, "");
            rest = std::regex_replace(rest, std::regex(R"(HP|XP|Lv|»|_|[0-9 ×/:·+\-()%.\[\]])"), "");
            INFO(name << ": \"" << (*m)[2].str() << "\"");
            CHECK(rest.empty());
        }
    }
}
