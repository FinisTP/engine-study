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

namespace
{
    campaign::Scene TestScene()
    {
        campaign::Scene scene;
        std::vector<core::Diagnostic> d;
        REQUIRE(campaign::LoadScene(tests / "scenes" / "test.json", TheCast(), scene, d));
        return scene;
    }

    // The number of code points in UTF-8 text.
    std::size_t Letters(const std::string &text)
    {
        std::size_t n = 0;
        for (const char ch : text)
            n += (static_cast<unsigned char>(ch) & 0xC0) != 0x80;
        return n;
    }
}

TEST_CASE("The typewriter shows whole letters, then Advance completes the line and moves on")
{
    const campaign::Scene scene = TestScene();
    const campaign::Cast cast = TheCast();
    campaign::Facts facts;
    campaign::ScenePlayer p(scene, cast, {"Lan", "female"}, facts);
    // 1. "Đêm ấy, ..." three letters at a time never splits a letter.
    CHECK(p.Shown().empty());
    p.Update(0.1F, 30.0F);
    CHECK(p.Shown() == "\xc4\x90\xc3\xaam"); // "Đêm"
    for (int i = 0; i < 5; ++i)
    {
        p.Update(0.1F, 30.0F);
        CHECK(Letters(p.Shown()) % 3 == 0);
    }
    // 2. Advance completes it; again moves to line b.
    p.Advance();
    CHECK(p.Shown() == scene.lines[0].text);
    p.Advance();
    CHECK(p.Shown().empty());
    CHECK(p.SpeakerName() == "L\xe1\xbb\x87 Thanh");
}

TEST_CASE("Portraits come and go; the speaker is lit, narration lights nobody")
{
    const campaign::Scene scene = TestScene();
    const campaign::Cast cast = TheCast();
    campaign::Facts facts;
    campaign::ScenePlayer p(scene, cast, {"Lan", "female"}, facts);
    // 1. Narration: Le Thanh stands left, unlit; no name.
    CHECK(p.SpeakerName().empty());
    CHECK(p.Left()[0].who == "lethanh");
    CHECK_FALSE(p.Left()[0].lit);
    // 2. Line b: Ti comes in on the right; Le Thanh speaks (lit, face talk) and is on the left.
    p.Advance();
    p.Advance();
    CHECK(p.Right()[0].who == "ti");
    CHECK(p.Right()[0].face == "afraid");
    CHECK(p.Left()[0].lit);
    CHECK(p.Left()[0].face == "talk");
    CHECK_FALSE(p.Right()[0].lit);
    CHECK(p.SpeakerSide() == campaign::Side::Left);
    // 3. Line c: Ti speaks from the right.
    p.Advance();
    p.Advance();
    CHECK(p.SpeakerSide() == campaign::Side::Right);
    CHECK(p.Right()[0].lit);
    CHECK_FALSE(p.Left()[0].lit);
}

TEST_CASE("Skip stops at a choice; Choose sets the fact and jumps; the log keeps the lines")
{
    const campaign::Scene scene = TestScene();
    const campaign::Cast cast = TheCast();
    campaign::Facts facts;
    campaign::ScenePlayer p(scene, cast, {"Lan", "female"}, facts);
    // 1. Skip runs to line c and stops there, the choices shown.
    p.Skip();
    CHECK_FALSE(p.Done());
    CHECK(p.Shown() == scene.lines[2].text);
    REQUIRE(p.Choices().size() == 2);
    p.Advance(); // not past a choice
    CHECK(p.Shown() == scene.lines[2].text);
    // 2. The second choice: the fact set, the jump to e.
    CHECK(p.Choose(1));
    CHECK(facts["ti.promise"] == "none");
    CHECK(p.Shown().empty());
    p.Advance();
    CHECK(p.Shown() == "...");
    // 3. Skip to the end; the log has a, b, c, e, f; Choose now refused.
    p.Skip();
    CHECK(p.Done());
    CHECK_FALSE(p.Choose(0));
    const auto log = p.Log();
    REQUIRE(log.size() == 5);
    CHECK(log[0].first.empty());
    CHECK(log[3].second == "...");
}

TEST_CASE("A choice on the last line: Skip still stops at it")
{
    campaign::Scene scene = TestScene();
    scene.lines.resize(3);
    scene.lines[2].choices[0].jump = scene.lines[2].choices[1].jump = "";
    const campaign::Cast cast = TheCast();
    campaign::Facts facts;
    campaign::ScenePlayer p(scene, cast, {"Lan", "female"}, facts);
    p.Skip();
    CHECK_FALSE(p.Done());
    CHECK(p.Choices().size() == 2);
    CHECK(p.Choose(0));
    CHECK(p.Done());
    CHECK(facts["ti.promise"] == "kept");
}

TEST_CASE("Tokens: the name and the form of address; a nameless player gets the default name")
{
    const campaign::Cast cast = TheCast();
    CHECK(campaign::FillTokens("Ch\xc3\xa0o {anh/ch\xe1\xbb\x8b} {player}.", {"Nam", "male"}, cast) == "Ch\xc3\xa0o anh Nam.");
    CHECK(campaign::FillTokens("{Anh/Ch\xe1\xbb\x8b} \xc6\xa1i!", {"Lan", "female"}, cast) == "Ch\xe1\xbb\x8b \xc6\xa1i!");
    CHECK(campaign::FillTokens("{player}", {"", "female"}, cast) == "Linh");
    // A scene line with tokens, through the player.
    const campaign::Scene scene = TestScene();
    campaign::Facts facts;
    campaign::ScenePlayer p(scene, cast, {"Nam", "male"}, facts);
    p.Advance();
    p.Advance();
    p.Advance();
    CHECK(p.Shown() == "Ch\xc3\xa0o anh Nam.");
    CHECK(p.TakeSound().empty());
}
