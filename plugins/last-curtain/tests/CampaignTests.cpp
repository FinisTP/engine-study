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

TEST_CASE("The prologue loads and plays to the end both ways through its choice")
{
    const campaign::Cast cast = TheCast();
    campaign::Scene scene;
    std::vector<core::Diagnostic> d;
    REQUIRE(campaign::LoadScene(data / "scenes" / "prologue.json", cast, scene, d));
    for (int pick = 0; pick < 2; ++pick)
    {
        campaign::Facts facts;
        campaign::ScenePlayer p(scene, cast, {"Lan", "female"}, facts);
        int steps = 0;
        while (!p.Done() && ++steps < 200)
        {
            p.Skip();
            if (!p.Done())
                REQUIRE(p.Choose(pick));
        }
        CHECK(p.Done());
        CHECK(facts["lethanh.first"] == (pick == 0 ? "kind" : "wary"));
        // No token is left unfilled.
        for (const auto &[name, text] : p.Log())
            CHECK(text.find('{') == std::string::npos);
    }
}

namespace
{
    match::MatchDef MatchData()
    {
        match::MatchDef def;
        std::vector<core::Diagnostic> d;
        REQUIRE(match::LoadMatch(UMBRA_MATCH_DATA, def, d));
        return def;
    }
}

TEST_CASE("Class lines by level, and units made from the cast with their adjustments and the enemy scaling (campaign plan 4)")
{
    const match::MatchDef def = MatchData();
    const campaign::Cast cast = TheCast();
    campaign::Growth growth;
    std::vector<core::Diagnostic> d;
    REQUIRE(campaign::LoadGrowth(data / "growth.csv", growth, d));
    // 1. Level 1 is the match's class line; HP grows 2 a level.
    const match::Unit p = campaign::MakeUnit(def, cast, growth, "player", 1, 0, false);
    CHECK(p.maxHp == 24);
    CHECK(p.attack == 4);
    CHECK(p.move == 8);
    // 2. Ti at level 3: the Speedster line, HP -2 and Move +2.
    const match::Unit ti = campaign::MakeUnit(def, cast, growth, "ti", 3, 0, false);
    CHECK(ti.maxHp == 20);
    CHECK(ti.attack == 4);
    CHECK(ti.defence == 2);
    CHECK(ti.speed == 6);
    CHECK(ti.move == 12);
    // 3. Enemies: a zodiac's attack and HP x1.33, an ordinary ghost's x0.6 (rounded).
    const match::Unit rat = campaign::MakeUnit(def, cast, growth, "rat", 3, 1, true);
    CHECK(rat.attack == 8);
    CHECK(rat.maxHp == 32);
    CHECK(rat.faction == 1);
    const match::Unit ghost = campaign::MakeUnit(def, cast, growth, "ghost_attacker", 3, 1, true);
    CHECK(ghost.attack == 4);
    CHECK(ghost.maxHp == 14);
    // 4. Allies are never scaled; abilities are the class's up to the level.
    CHECK(campaign::MakeUnit(def, cast, growth, "bao", 3, 0, false).attack == 6);
    CHECK(p.abilities[0] >= 0);
    // 5. A broken line is refused.
    campaign::Growth bad;
    CHECK_FALSE(campaign::LoadGrowth(Write("growth.csv", "role,level,hp,attack,defence,speed,move\nwizard,1,1,1,1,1,1\n"), bad, d));
}

namespace
{
    // Loads the test stage changed by edit; returns the first problem's field ("" when it loads).
    std::string StageProblem(const std::string &name, const std::function<void(json &)> &edit)
    {
        json j = json::parse(std::ifstream(tests / "stages" / "test.json"));
        edit(j);
        campaign::StageDef stage;
        std::vector<core::Diagnostic> d;
        const bool ok = campaign::LoadStage(Write(name + ".json", j.dump()), TheCast(), MatchData(), tests / "scenes", stage, d);
        if (ok)
            return "";
        REQUIRE_FALSE(d.empty());
        CHECK_FALSE(d.front().message.empty());
        return d.front().field;
    }
}

TEST_CASE("A stage file loads: map, units, items, scenes, twists (campaign plan 4)")
{
    campaign::StageDef stage;
    std::vector<core::Diagnostic> d;
    REQUIRE(campaign::LoadStage(tests / "stages" / "test.json", TheCast(), MatchData(), tests / "scenes", stage, d));
    CHECK(stage.id == "test");
    CHECK(stage.level == 3);
    CHECK(stage.partyCap == 2);
    CHECK(stage.width == 9);
    CHECK(stage.height == 6);
    CHECK(stage.At(1, 1) == 'S');
    CHECK(stage.At(4, 3) == 'D');
    REQUIRE(stage.units.size() == 3);
    CHECK(stage.units[0].who == "rat");
    CHECK(stage.units[0].boss);
    CHECK(stage.units[0].behaviour == campaign::Behaviour::Aggressive);
    CHECK(stage.units[0].after == 2);
    REQUIRE(stage.units[0].drops.size() == 1);
    CHECK(stage.units[0].drops[0].second == 30);
    CHECK(stage.units[1].recruit);
    CHECK(stage.units[1].strung);
    CHECK(stage.units[2].behaviour == campaign::Behaviour::Reinforcement);
    CHECK(stage.units[2].round == 2);
    REQUIRE(stage.items.size() == 1);
    CHECK(stage.items[0].id == "test-bandage");
    CHECK(stage.mustFree);
    CHECK(stage.noRevive == std::vector<int>{1});
    CHECK(stage.scenes.at("firstAttacked:rat") == "test");
    REQUIRE(stage.twists.size() == 1);
    CHECK(stage.twists[0].units.size() == 1);
}

TEST_CASE("A broken stage names the file, the field and the fix (campaign plan 4)")
{
    CHECK(StageProblem("ok", [](json &) {}).empty());
    CHECK(StageProblem("format", [](json &j) { j["format"] = "x"; }) == "format");
    CHECK(StageProblem("rows", [](json &j) { j["map"][2] = "#S"; }) == "map[2]");
    CHECK(StageProblem("char", [](json &j) { j["map"][2] = "#S..#..?#"; }) == "map[2]");
    CHECK(StageProblem("starts", [](json &j) { j["partyCap"] = 3; }) == "partyCap");
    CHECK(StageProblem("who", [](json &j) { j["units"][0]["who"] = "nobody"; }) == "units[0].who");
    CHECK(StageProblem("wall", [](json &j) { j["units"][0]["at"] = json::array({0, 0}); }) == "units[0].at");
    CHECK(StageProblem("outside", [](json &j) { j["units"][0]["at"] = json::array({40, 1}); }) == "units[0].at");
    CHECK(StageProblem("flag", [](json &j) { j["units"][0]["flags"] = json::array({"king"}); }) == "units[0].flags");
    CHECK(StageProblem("behaviour", [](json &j) { j["units"][0]["behaviour"]["kind"] = "dance"; }) == "units[0].behaviour.kind");
    CHECK(StageProblem("drop", [](json &j) { j["units"][0]["drops"][0]["item"] = "cake"; }) == "units[0].drops[0].item");
    CHECK(StageProblem("item", [](json &j) { j["items"][0]["item"] = "cake"; }) == "items[0].item");
    CHECK(StageProblem("itemid", [](json &j) { j["items"][0].erase("id"); }) == "items[0].id");
    CHECK(StageProblem("scene", [](json &j) { j["scenes"]["start"] = "missing"; }) == "scenes.start");
    CHECK(StageProblem("moment", [](json &j) { j["scenes"]["whenever"] = "test"; }) == "scenes.whenever");
    CHECK(StageProblem("twist", [](json &j) { j["twists"][0]["units"][0]["who"] = "nobody"; }) == "twists[0].units[0].who");
    CHECK(StageProblem("boss", [](json &j) { j["units"][0]["flags"] = json::array(); }) == "units");
}
