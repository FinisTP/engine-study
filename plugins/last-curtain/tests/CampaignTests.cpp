#include <Campaign.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <fstream>
#include <functional>

using namespace umbra;
using json = nlohmann::json;

namespace
{
    const std::filesystem::path data = UMBRA_CAMPAIGN_DATA;
    const std::filesystem::path tests = UMBRA_CAMPAIGN_TEST_DATA;

    campaign::Cast TheCast()
    {
        campaign::Cast cast;
        std::vector<core::Diagnostic> d;
        REQUIRE(campaign::LoadCast(data / "cast.csv", cast, d));
        return cast;
    }

    std::filesystem::path Write(const std::string &name, const std::string &text)
    {
        const auto file = std::filesystem::temp_directory_path() / ("umbra-campaign-" + name);
        std::ofstream(file, std::ios::binary) << text;
        return file;
    }

    // Loads the test scene changed by edit; returns the diagnostics' fields.
    std::vector<core::Diagnostic> Broken(const std::string &name, const std::function<void(json &)> &edit)
    {
        json j = json::parse(std::ifstream(tests / "scenes" / "test.json"));
        edit(j);
        campaign::Scene scene;
        std::vector<core::Diagnostic> d;
        CHECK_FALSE(campaign::LoadScene(Write(name + ".json", j.dump()), TheCast(), scene, d));
        REQUIRE_FALSE(d.empty());
        CHECK(d.front().file.find(name) != std::string::npos);
        CHECK_FALSE(d.front().message.empty());
        return d;
    }
}

TEST_CASE("The cast sheet loads; a bad form is refused")
{
    const campaign::Cast cast = TheCast();
    REQUIRE(cast.Find("ti"));
    CHECK(cast.Find("ti")->name == "Bé Tí");
    CHECK(cast.Find("ti")->bound == "ti_bound");
    CHECK(cast.Find("ti")->form == campaign::Form::Strung);
    CHECK(cast.Find("rat")->form == campaign::Form::Zodiac);
    CHECK(cast.Find("lethanh")->form == campaign::Form::Host);
    CHECK(cast.Find("nobody") == nullptr);

    campaign::Cast bad;
    std::vector<core::Diagnostic> d;
    CHECK_FALSE(campaign::LoadCast(Write("cast.csv", "id,name,sprite,bound,form,voice\nx,X,x,,wizard,\n"), bad, d));
    REQUIRE_FALSE(d.empty());
    CHECK(d.front().field == "rows[0].form");
}

TEST_CASE("A scene loads")
{
    campaign::Scene scene;
    std::vector<core::Diagnostic> d;
    REQUIRE(campaign::LoadScene(tests / "scenes" / "test.json", TheCast(), scene, d));
    CHECK(scene.id == "test");
    REQUIRE(scene.lines.size() == 6);
    CHECK(scene.lines[0].speaker.empty());
    CHECK(scene.lines[0].enter.front().who == "lethanh");
    CHECK(scene.lines[1].enter.front().side == campaign::Side::Right);
    REQUIRE(scene.lines[2].choices.size() == 2);
    CHECK(scene.lines[2].choices[0].fact == "ti.promise");
    CHECK(scene.lines[2].choices[0].jump == "d");
    CHECK(scene.lines[3].jump == "f");
    CHECK(scene.lines[5].leave == std::vector<std::string>{"ti"});
}

TEST_CASE("A broken scene names the file, the line and the fix")
{
    CHECK(Broken("speaker", [](json &j) { j["lines"][1]["speaker"] = "nobody"; }).front().field == "lines[1].speaker");
    CHECK(Broken("who", [](json &j) { j["lines"][0]["enter"][0]["who"] = "nobody"; }).front().field == "lines[0].enter[0].who");
    CHECK(Broken("face", [](json &j) { j["lines"][1]["face"] = "sleepy"; }).front().field == "lines[1].face");
    CHECK(Broken("crowd", [](json &j) {
              j["lines"][1]["enter"] = json::array({{{"who", "ti"}, {"side", "left"}, {"slot", 1}}, {{"who", "bao"}, {"side", "left"}, {"slot", 1}}});
          }).front().field == "lines[1].enter");
    CHECK(Broken("offstage", [](json &j) { j["lines"][1]["speaker"] = "mai"; }).front().field == "lines[1].speaker");
    CHECK(Broken("onechoice", [](json &j) { j["lines"][2]["choices"].erase(1); }).front().field == "lines[2].choices");
    CHECK(Broken("jump", [](json &j) { j["lines"][3]["jump"] = "zz"; }).front().field == "lines[3].jump");
    CHECK(Broken("dup", [](json &j) { j["lines"][3]["id"] = "c"; }).front().field == "lines[3].id");
    CHECK(Broken("loop", [](json &j) {
              j["lines"][3]["jump"] = "e";
              j["lines"][4]["jump"] = "d";
          }).front().field == "lines[3].jump");
}
