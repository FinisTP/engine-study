#include <CampaignText.hpp>
#include <Localization.hpp>
#include <Campaign.hpp>
#include <MatchSave.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <functional>

namespace
{
    // The display text in Vietnamese, as the game fills it (Localization.md "Data").
    const CampaignText &Text()
    {
        static const CampaignText text = [] {
            CampaignText t;
            std::string error;
            (void)t.table.Load(std::filesystem::path(UMBRA_LASTCURTAIN_APP_ASSETS) / "localization.csv", error);
            return t;
        }();
        return text;
    }
}

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
    CHECK(p.move == 4); // moves are 2-4 squares (feedback 2026-11-24 15:10, note 3)
    // 2. Ti at level 3: the Speedster line, HP -2 (his Move bonus is 0 since note 3).
    const match::Unit ti = campaign::MakeUnit(def, cast, growth, "ti", 3, 0, false);
    CHECK(ti.maxHp == 20);
    CHECK(ti.attack == 4);
    CHECK(ti.defence == 2);
    CHECK(ti.speed == 6);
    CHECK(ti.move == 4);
    // 3. Enemies: a zodiac's attack and HP x1.33, an ordinary ghost's x0.6 (rounded).
    const match::Unit rat = campaign::MakeUnit(def, cast, growth, "rat", 3, 1, true);
    CHECK(rat.attack == 7); // (6 - 1 of its own) x 1.33
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

namespace
{
    campaign::StageDef TestStage()
    {
        campaign::StageDef stage;
        std::vector<core::Diagnostic> d;
        REQUIRE(campaign::LoadStage(tests / "stages" / "test.json", TheCast(), MatchData(), tests / "scenes", stage, d));
        return stage;
    }

    campaign::Growth TheGrowth()
    {
        campaign::Growth g;
        std::vector<core::Diagnostic> d;
        REQUIRE(campaign::LoadGrowth(data / "growth.csv", g, d));
        return g;
    }

    int Cell(const match::MatchState &s, int x, int y) { return y * s.board.width + x; }
}

TEST_CASE("A stage builds a fight: the map, the party on its starts, enemies, items not taken, reinforcements waiting (campaign plan 4)")
{
    const match::MatchDef def = MatchData();
    const campaign::StageDef stage = TestStage();
    const std::vector<campaign::PartyMember> party{{"player", 3, {"bandage", "", "", ""}}, {"bao", 3, {}}, {"mai", 3, {}}};
    const campaign::BuiltStage b = campaign::BuildStage(def, TheCast(), TheGrowth(), stage, party, 0, {}, 7);
    const match::MatchState &s = b.state;
    // 1. The map: walls and open squares, doors between open neighbours.
    CHECK(s.board.width == 9);
    CHECK(s.board.cells[0].kind == match::CellKind::Wall);
    CHECK(match::Neighbour(s.board, Cell(s, 1, 1), 1) == Cell(s, 2, 1));
    CHECK(match::Neighbour(s.board, Cell(s, 1, 1), 0) == -1);
    // 2. The party up to its cap, on the starts; the first is the player and carries its items.
    REQUIRE(b.who.size() == 5); // 2 of the party, the rat, Ti, the reinforcement
    CHECK(b.who[0] == "player");
    CHECK(b.who[1] == "bao");
    CHECK(s.units[0].cell == Cell(s, 1, 1));
    CHECK(s.units[1].cell == Cell(s, 1, 2));
    CHECK((s.units[0].flags & match::flag::Player) != 0);
    CHECK(s.units[0].items[0] >= 0);
    CHECK(s.units[0].faction == 0);
    // 3. Enemies with their flags; the reinforcement off the board until its round.
    CHECK(b.who[2] == "rat");
    CHECK((s.units[2].flags & match::flag::Boss) != 0);
    CHECK((s.units[3].flags & match::flag::Strung) != 0);
    CHECK(s.units[3].faction == 1);
    CHECK(s.units[4].cell == -1);
    REQUIRE(s.arrivals.size() == 1);
    CHECK(s.arrivals[0].unit == 4);
    CHECK(s.arrivals[0].cell == Cell(s, 7, 3));
    CHECK(s.arrivals[0].round == 2);
    // 4. The rules: the campaign mode, the stage's overrides, drops, the item on its square.
    CHECK(s.mode.mustFree);
    CHECK(s.noRevive == 2); // side 1
    REQUIRE(s.drops.size() == 1);
    CHECK(s.drops[0].unit == 2);
    CHECK(s.drops[0].percent == 30);
    REQUIRE(s.floor.size() == 1);
    CHECK(s.floor[0].cell == Cell(s, 3, 4));
    CHECK(match::Current(s) >= 0);
}

TEST_CASE("A taken item stays taken; a refill brings its twist (campaign plan 4)")
{
    const match::MatchDef def = MatchData();
    const campaign::StageDef stage = TestStage();
    const std::vector<campaign::PartyMember> party{{"player", 3, {}}};
    const campaign::BuiltStage taken = campaign::BuildStage(def, TheCast(), TheGrowth(), stage, party, 0, {"test-bandage"}, 7);
    CHECK(taken.state.floor.empty());
    const campaign::BuiltStage refill = campaign::BuildStage(def, TheCast(), TheGrowth(), stage, party, 1, {}, 7);
    CHECK(refill.who.size() == taken.who.size() + 1);
    CHECK(refill.who.back() == "ghost_attacker");
}

TEST_CASE("Rooms: open squares joined without passing a doorway (campaign plan 4)")
{
    const campaign::BuiltStage b = campaign::BuildStage(MatchData(), TheCast(), TheGrowth(), TestStage(), {{"player", 3, {}}}, 0, {}, 7);
    const match::MatchState &s = b.state;
    CHECK(b.rooms[static_cast<std::size_t>(Cell(s, 1, 1))] == b.rooms[static_cast<std::size_t>(Cell(s, 3, 4))]);
    CHECK(b.rooms[static_cast<std::size_t>(Cell(s, 1, 1))] != b.rooms[static_cast<std::size_t>(Cell(s, 6, 1))]);
    CHECK(b.rooms[static_cast<std::size_t>(Cell(s, 4, 3))] == -1); // the doorway belongs to no room
}

TEST_CASE("Reinforcements are marked a round ahead, stand on their square at their round, act from the next (campaign plan 4)")
{
    const match::MatchDef def = MatchData();
    campaign::BuiltStage b = campaign::BuildStage(def, TheCast(), TheGrowth(), TestStage(), {{"player", 3, {}}}, 0, {}, 7);
    match::MatchState &s = b.state;
    // 1. Round 1: marked, not there.
    CHECK(std::ranges::any_of(s.log, [](const match::LogEntry &e) { return e.kind == match::LogKind::Arriving && e.a == 3; }));
    CHECK(s.units[3].cell == -1);
    // 2. Round 2: on its square; it doesn't act until round 3.
    for (int i = 0; i < 40 && s.round < 2; ++i)
        REQUIRE(match::Apply(def, s, {match::ActionKind::EndTurn}));
    CHECK(s.units[3].cell == Cell(s, 7, 3));
    CHECK(s.units[3].from == 3);
    CHECK(s.arrivals.empty());
}

TEST_CASE("A reinforcement whose square is taken stands on the nearest free one (campaign plan 4)")
{
    const match::MatchDef def = MatchData();
    campaign::BuiltStage b = campaign::BuildStage(def, TheCast(), TheGrowth(), TestStage(), {{"player", 3, {}}}, 0, {}, 7);
    match::MatchState &s = b.state;
    s.units[1].cell = static_cast<std::int16_t>(Cell(s, 7, 3)); // the rat stands there
    s.units[2].cell = static_cast<std::int16_t>(Cell(s, 7, 3)); // and Ti: the square is full
    for (int i = 0; i < 40 && s.round < 2; ++i)
        REQUIRE(match::Apply(def, s, {match::ActionKind::EndTurn}));
    CHECK(s.units[3].cell >= 0);
    CHECK(s.units[3].cell != Cell(s, 7, 3));
    CHECK(match::Steps(s.board, s.units[3].cell, Cell(s, 7, 3)) == 1);
}

TEST_CASE("Behaviours steer the enemy brain: aggressive waits for its round, passive for a party unit in its room (campaign plan 4)")
{
    const match::MatchDef def = MatchData();
    campaign::BuiltStage b = campaign::BuildStage(def, TheCast(), TheGrowth(), TestStage(), {{"player", 3, {}}}, 0, {}, 7);
    match::MatchState &s = b.state;
    const auto turnOf = [&](int unit) {
        for (int i = 0; i < 60 && match::Current(s) != unit; ++i)
            REQUIRE(match::Apply(def, s, {match::ActionKind::EndTurn}));
        REQUIRE(match::Current(s) == unit);
    };
    // 1. The rat is aggressive after round 2: in round 1 it holds.
    turnOf(1);
    REQUIRE(s.round == 1);
    CHECK(campaign::EnemyAction(def, b, s, match::BotLevel::Medium).kind == match::ActionKind::EndTurn);
    // 2. Made passive: it holds while the player is in the other room, and acts once the player is beside it.
    b.behaviour[1] = {campaign::Behaviour::Passive, 0};
    CHECK(campaign::EnemyAction(def, b, s, match::BotLevel::Medium).kind == match::ActionKind::EndTurn);
    s.units[0].cell = static_cast<std::int16_t>(Cell(s, 5, 1));
    CHECK(campaign::EnemyAction(def, b, s, match::BotLevel::Medium).kind != match::ActionKind::EndTurn);
    // 3. Hurt, a passive unit wakes even with nobody near.
    s.units[0].cell = static_cast<std::int16_t>(Cell(s, 1, 1));
    s.units[1].hp = static_cast<std::int16_t>(s.units[1].maxHp - 1);
    CHECK(campaign::EnemyAction(def, b, s, match::BotLevel::Medium).kind != match::ActionKind::EndTurn);
}

TEST_CASE("The slice's kits: Bao, Ba Mai, the Rat and Ti carry their abilities; every version exists (campaign plan 4)")
{
    const match::MatchDef def = MatchData();
    const campaign::Cast cast = TheCast();
    const campaign::Growth growth = TheGrowth();
    const auto has = [&](const match::Unit &u, const char *id) {
        return std::ranges::any_of(u.abilities, [&](std::int8_t a) { return a >= 0 && def.abilities[static_cast<std::size_t>(a)].id == id; });
    };
    const match::Unit ti = campaign::MakeUnit(def, cast, growth, "ti", 3, 0, false);
    CHECK(has(ti, "na_cao_su"));
    CHECK(has(ti, "dua_tin"));
    CHECK(has(ti, "chan_sao"));
    const match::Unit rat = campaign::MakeUnit(def, cast, growth, "rat", 3, 1, true);
    CHECK(has(rat, "moc_tui"));
    CHECK(has(rat, "chay_lang"));
    CHECK(has(rat, "speedkill"));
    CHECK(has(campaign::MakeUnit(def, cast, growth, "bao", 3, 0, false), "chan_doan"));
    CHECK(has(campaign::MakeUnit(def, cast, growth, "mai", 3, 0, false), "com_nguoi"));
    // Every upgrade version is an ability row, with its Vietnamese name in localization.csv.
    const nlohmann::json words = umbra::night::LoadWords(std::filesystem::path(UMBRA_LASTCURTAIN_APP_ASSETS) / "localization.csv", "campaign", false);
    for (const char *id : {"na_bao_dong", "na_ban_truoc", "dua_tin_ve_kip", "dua_tin_phan_qua", "chan_sao_gio_bien", "chan_sao_dau_chan",
                           "na_cao_su", "dua_tin", "chan_sao", "dao_mo", "chan_doan", "giay_chung_tu", "com_nguoi", "moc_tui", "chay_lang", "speedkill"})
    {
        INFO(id);
        CHECK(std::ranges::any_of(def.abilities, [&](const match::AbilityDef &a) { return a.id == id; }));
        CHECK(words["abilities"].contains(id));
    }
}

namespace
{
    const nlohmann::json allGimmicks = {
        {"windows", {{"pattern", {{0}}}, {"spawn", "rat_specter"}, {"level", 2}, {"max", 2}, {"drops", {{{"item", "bandage"}, {"percent", 30}}}}}},
        {"hazards", {{"count", 1}, {"damage", 3}}},
        {"breath", {{"unit", "rat"}, {"damage", 4}, {"every", 2}}},
        {"dark", {{"sight", 2}}}};
}

TEST_CASE("A stage's gimmicks are checked and set up in its fight; twists override them (campaign plan 4)")
{
    // 1. Checks.
    CHECK(StageProblem("gimmicks-ok", [](json &j) { j["gimmicks"] = allGimmicks; }).empty());
    CHECK(StageProblem("no-window", [](json &j) { j["gimmicks"] = allGimmicks; j["map"][1] = "#S..#...#"; }) == "gimmicks.windows");
    CHECK(StageProblem("pattern", [](json &j) { j["gimmicks"] = allGimmicks; j["gimmicks"]["windows"]["pattern"] = {{5}}; }) == "gimmicks.windows.pattern");
    CHECK(StageProblem("spawn", [](json &j) { j["gimmicks"] = allGimmicks; j["gimmicks"]["windows"]["spawn"] = "nobody"; }) == "gimmicks.windows.spawn");
    CHECK(StageProblem("breath", [](json &j) { j["gimmicks"] = allGimmicks; j["gimmicks"]["breath"]["unit"] = "mai"; }) == "gimmicks.breath.unit");
    CHECK(StageProblem("unknown", [](json &j) { j["gimmicks"] = {{"fog", 1}}; }) == "gimmicks.fog");
    // 2. Set up.
    campaign::StageDef stage = TestStage();
    stage.gimmicks = allGimmicks;
    stage.twists[0].gimmicks = {{"hazards", {{"count", 2}, {"damage", 1}}}};
    const match::MatchDef def = MatchData();
    campaign::BuiltStage b = campaign::BuildStage(def, TheCast(), TheGrowth(), stage, {{"player", 3, {}}}, 0, {}, 7);
    const match::Gimmicks &g = b.state.gimmicks;
    REQUIRE(g.windows.size() == 1);
    CHECK(g.windows[0] == Cell(b.state, 7, 1));
    CHECK(g.spawnMax == 2);
    CHECK(g.spawn.faction == 1);
    CHECK(g.spawn.maxHp > 0);
    REQUIRE(g.spawnDrops.size() == 1);
    CHECK(g.spawnDrops[0].percent == 30);
    CHECK(g.hazards == 1);
    CHECK(g.breathUnit == 1); // the rat, after the one party member
    CHECK(b.sight == 2);
    // 3. A refill's twist replaces what it names.
    const campaign::BuiltStage r = campaign::BuildStage(def, TheCast(), TheGrowth(), stage, {{"player", 3, {}}}, 1, {}, 7);
    CHECK(r.state.gimmicks.hazards == 2);
    CHECK(r.state.gimmicks.windows.size() == 1);
    // 4. A spawned unit gets its name and a plain behaviour when the stage is synced.
    for (int i = 0; i < 60 && b.state.units.size() == b.who.size(); ++i)
        REQUIRE(match::Apply(def, b.state, {match::ActionKind::EndTurn}));
    REQUIRE(b.state.units.size() > b.who.size());
    campaign::Sync(b);
    CHECK(b.who.back() == "rat_specter");
    CHECK(b.behaviour.size() == b.state.units.size());
}

TEST_CASE("A fight's moments start their scenes once each, only when told (campaign plan 4)")
{
    const match::MatchDef def = MatchData();
    campaign::StageDef stage = TestStage();
    stage.scenes = {{"start", "a"}, {"firstAttacked:rat", "b"}, {"falls:rat", "c"}, {"round:2", "d"}, {"end", "e"}, {"freed:ti", "f"}};
    campaign::BuiltStage b = campaign::BuildStage(def, TheCast(), TheGrowth(), stage, {{"player", 3, {}}}, 0, {}, 7);
    match::MatchState &s = b.state;
    campaign::Moments m;
    const auto at = [&](const char *who) { return static_cast<std::int16_t>(std::ranges::find(b.who, who) - b.who.begin()); };
    const std::int16_t rat = at("rat"), ti = at("ti");
    // 1. The start, once.
    CHECK(campaign::StageMoments(stage, b, s.log.size(), m) == std::vector<std::string>{"a"});
    CHECK(campaign::StageMoments(stage, b, s.log.size(), m).empty());
    // 2. A blow on the rat: only once it is told, and only the first.
    const std::size_t before = s.log.size();
    s.log.push_back({match::LogKind::Blow, 0, rat, 3});
    CHECK(campaign::StageMoments(stage, b, before, m).empty());
    CHECK(campaign::StageMoments(stage, b, s.log.size(), m) == std::vector<std::string>{"b"});
    s.log.push_back({match::LogKind::Blow, 0, rat, 3});
    CHECK(campaign::StageMoments(stage, b, s.log.size(), m).empty());
    // 3. A round reached, a fall, a freeing, the end; the state is not touched.
    s.round = 2;
    s.log.push_back({match::LogKind::Freed, 0, ti, 0});
    s.log.push_back({match::LogKind::Down, rat, 0, 0});
    s.over = true, s.winner = 0;
    const nlohmann::json saved = match::SaveMatch(s);
    CHECK(campaign::StageMoments(stage, b, s.log.size(), m) == std::vector<std::string>{"d", "f", "c", "e"});
    CHECK(match::SaveMatch(s) == saved);
    CHECK(campaign::StageMoments(stage, b, s.log.size(), m).empty());
    // 4. A lost fight plays "lost", not "end".
    stage.scenes = {{"end", "e"}, {"lost", "l"}};
    campaign::BuiltStage lost = campaign::BuildStage(def, TheCast(), TheGrowth(), stage, {{"player", 3, {}}}, 0, {}, 7);
    lost.state.over = true, lost.state.winner = 1;
    campaign::Moments fresh;
    CHECK(campaign::StageMoments(stage, lost, lost.state.log.size(), fresh) == std::vector<std::string>{"l"});
}

TEST_CASE("The slice's stages and scenes load (campaign plan 4)")
{
    const match::MatchDef def = MatchData();
    for (const char *id : {"t1", "t2", "t2s", "t3", "stage1", "sk_props", "sk_stairs", "sk_boxoffice"})
    {
        INFO(id);
        campaign::StageDef stage;
        std::vector<core::Diagnostic> d;
        CHECK(campaign::LoadStage(data / "stages" / (std::string(id) + ".json"), TheCast(), def, data / "scenes", stage, d));
        for (const core::Diagnostic &e : d)
            UNSCOPED_INFO(core::Format(e));
        CHECK(d.empty());
    }
    for (const auto &file : std::filesystem::directory_iterator(data / "scenes"))
    {
        INFO(file.path().string());
        campaign::Scene scene;
        std::vector<core::Diagnostic> d;
        CHECK(campaign::LoadScene(file.path(), TheCast(), scene, d));
    }
}

TEST_CASE("Each stage and each refill can be won by the party brain (campaign plan 4)", "[campaign][slow]")
{
    const match::MatchDef def = MatchData();
    for (const char *id : {"t1", "t2", "t2s", "t3", "stage1", "sk_props", "sk_stairs", "sk_boxoffice"})
    {
        campaign::StageDef stage;
        std::vector<core::Diagnostic> d;
        REQUIRE(campaign::LoadStage(data / "stages" / (std::string(id) + ".json"), TheCast(), def, data / "scenes", stage, d));
        std::vector<campaign::PartyMember> party;
        for (const char *who : {"player", "mai", "bao"})
            if (static_cast<int>(party.size()) < stage.partyCap)
                party.push_back({who, stage.level, {}});
        for (int refill = 0; refill <= static_cast<int>(stage.twists.size()); ++refill)
        {
            INFO(id << " refill " << refill);
            int wins = 0;
            for (std::uint64_t seed = 1; seed <= 20 && wins == 0; ++seed)
            {
                campaign::BuiltStage b = campaign::BuildStage(def, TheCast(), TheGrowth(), stage, party, refill, {}, seed);
                for (int step = 0; step < 4000 && !b.state.over; ++step)
                {
                    const int me = match::Current(b.state);
                    REQUIRE(me >= 0);
                    const match::Action a = b.state.units[static_cast<std::size_t>(me)].faction == 0
                                                ? campaign::PartyAction(def, b, b.state, match::BotLevel::Hard)
                                                : campaign::EnemyAction(def, b, b.state, match::BotLevel::Medium);
                    REQUIRE(match::Apply(def, b.state, a));
                    campaign::Sync(b);
                }
                wins += b.state.over && b.state.winner == 0;
            }
            CHECK(wins > 0);
        }
    }
}

#include <CampaignSoak.hpp>
#include <MatchBots.hpp>

TEST_CASE("The campaign soak counts wins, losses, falls, minutes and finds; bands hold or say why (campaign plan 4)")
{
    const match::MatchDef def = MatchData();
    campaign::StageDef stage;
    std::vector<core::Diagnostic> d;
    REQUIRE(campaign::LoadStage(data / "stages" / "stage1.json", TheCast(), def, data / "scenes", stage, d));
    const campaign::StageReport r = campaign::SoakStage(def, TheCast(), TheGrowth(), stage, 0, 1, 12);
    CHECK(r.runs == 12);
    CHECK(r.wins + r.trapped + r.otherLosses + r.draws == 12);
    CHECK(r.rounds > 0.0);
    CHECK(r.minutes > 0.0);
    CHECK(r.playerFalls >= 0.0);
    CHECK(r.playerFalls <= 1.0);
    CHECK(r.found.contains("giay_gap_tu") == (r.found.contains("giay_gap_tu") && r.found.at("giay_gap_tu") > 0));
    // 1. Same seed, same report.
    const campaign::StageReport again = campaign::SoakStage(def, TheCast(), TheGrowth(), stage, 0, 1, 12);
    CHECK(again.wins == r.wins);
    CHECK(again.found == r.found);
    // 2. Bands: a band the report misses names itself.
    const nlohmann::json bands = {{"wins", {0.0, 0.0}}};
    const std::vector<std::string> missed = campaign::Missed(r, bands);
    CHECK(missed.empty() == (r.wins == 0));
    CHECK(campaign::Missed(r, nlohmann::json{{"wins", {0.0, 1.0}}}).empty());
    CHECK_FALSE(campaign::Text(stage, r).empty());
}

TEST_CASE("Every cast member made into a unit can answer in a battle (campaign plan 4)")
{
    const match::MatchDef def = MatchData();
    const campaign::Cast cast = TheCast();
    for (const char *who : {"player", "ti", "bao", "mai", "rat", "keeper", "ghost_attacker", "ghost_defender", "ghost_speedster", "ghost_supporter", "rat_specter", "thief_specter"})
    {
        INFO(who);
        const match::Unit u = campaign::MakeUnit(def, cast, TheGrowth(), who, 3, 0, false);
        CHECK(std::ranges::any_of(u.abilities, [&](int a) { return a >= 0 && def.abilities[static_cast<std::size_t>(a)].use == match::Use::Battle && def.abilities[static_cast<std::size_t>(a)].kind == match::AbilityClass::Strike; }));
    }
}


TEST_CASE("Every stage and refill holds its bands in balance.json (campaign plan 4)", "[campaign-balance]")
{
    const match::MatchDef def = MatchData();
    const nlohmann::json bands = nlohmann::json::parse(std::ifstream(data / "balance.json"));
    for (const char *id : {"t1", "t2", "t2s", "t3", "stage1", "sk_props", "sk_stairs", "sk_boxoffice"})
    {
        campaign::StageDef stage;
        std::vector<core::Diagnostic> d;
        REQUIRE(campaign::LoadStage(data / "stages" / (std::string(id) + ".json"), TheCast(), def, data / "scenes", stage, d));
        for (int refill = 0; refill <= static_cast<int>(stage.twists.size()); ++refill)
        {
            const campaign::StageReport r = campaign::SoakStage(def, TheCast(), TheGrowth(), stage, refill, 1, 200);
            INFO(campaign::Text(stage, r));
            CHECK(campaign::Missed(r, bands[id]).empty());
        }
    }
}

#include <CampaignCafe.hpp>

namespace
{
    campaign::Recipes TheRecipes()
    {
        campaign::Recipes r;
        std::vector<core::Diagnostic> d;
        REQUIRE(campaign::LoadRecipes(data / "recipes.json", MatchData(), r, d));
        return r;
    }

    std::vector<campaign::StageDef> TheStages()
    {
        std::vector<campaign::StageDef> out;
        for (const char *id : {"t1", "t2", "t2s", "t3", "stage1", "sk_props", "sk_stairs", "sk_boxoffice"})
        {
            campaign::StageDef s;
            std::vector<core::Diagnostic> d;
            REQUIRE(campaign::LoadStage(data / "stages" / (std::string(id) + ".json"), TheCast(), MatchData(), data / "scenes", s, d));
            Text().Fill(s);
            out.push_back(s);
        }
        return out;
    }
}

TEST_CASE("Recipes: what is missing and where it is found; brewing spends; cafe-only items never take a slot (campaign plan 6)")
{
    const match::MatchDef def = MatchData();
    const campaign::Recipes recipes = TheRecipes();
    const campaign::Recipe *jam = recipes.Find("mut_gung");
    REQUIRE(jam);
    CHECK(jam->kind == "memory");
    CHECK(jam->forWho == "ti");
    campaign::Save save;
    // 1. Nothing in storage: both are missing, each with where it comes from.
    std::vector<campaign::Missing> lacks = campaign::Lacks(save, *jam, TheStages());
    REQUIRE(lacks.size() == 2);
    CHECK(lacks[0].item == "duong");
    CHECK(lacks[0].where == "Hành lang chân dung");
    CHECK(lacks[1].item == "gung");
    CHECK(lacks[1].where.find("Phòng vé") != std::string::npos);
    CHECK_FALSE(campaign::Brew(save, *jam));
    // 2. An unknown source is "?".
    campaign::Recipe odd = *jam;
    odd.needs = {{"programme", 1}};
    CHECK(campaign::Lacks(save, odd, TheStages())[0].where == "?");
    // 3. With both: it brews, spending them.
    save.storage = {{"gung", 2}, {"duong", 1}};
    CHECK(campaign::Lacks(save, *jam, TheStages()).empty());
    CHECK(campaign::Brew(save, *jam));
    CHECK(save.storage["gung"] == 1);
    CHECK(save.storage["duong"] == 0);
    CHECK_FALSE(campaign::Brew(save, *jam));
    // 4. Slots: a bandage may be carried, ginger and the keepsake may not.
    CHECK(campaign::SlotOk(def, "bandage"));
    CHECK_FALSE(campaign::SlotOk(def, "gung"));
    CHECK_FALSE(campaign::SlotOk(def, "giay_gap_tu"));
    // 5. A broken recipe names its field.
    std::vector<core::Diagnostic> d;
    campaign::Recipes bad;
    CHECK_FALSE(campaign::LoadRecipes(Write("recipes.json", R"({"format":"umbra.lc-recipes","version":1,"recipes":[{"id":"x","kind":"odd","needs":{"nothing":1}}]})"), def, bad, d));
    REQUIRE(d.size() == 2);
    CHECK(d[0].field == "recipes[0].kind");
    CHECK(d[1].field == "recipes[0].needs.nothing");
}

TEST_CASE("Memories: what each brings back, joining, a version chosen by the answer and final, the locked one (campaign plan 6)")
{
    const match::MatchDef def = MatchData();
    campaign::Memories memories;
    std::vector<core::Diagnostic> d;
    REQUIRE(campaign::LoadMemories(data / "memories.json", data / "scenes", TheRecipes(), def, memories, d));
    CHECK(d.empty());
    campaign::Save save;
    // 1. A talk brings back memory 1: Ti joins.
    const campaign::Memory *first = campaign::NextMemory(memories, save, "ti", "talk", "");
    REQUIRE(first);
    CHECK(first->id == "ti_1");
    CHECK(campaign::Remember(save, *first, {}));
    CHECK(save.joined.contains("ti"));
    CHECK_FALSE(campaign::Remember(save, *first, {})); // once
    CHECK(campaign::NextMemory(memories, save, "ti", "talk", "") == nullptr);
    // 2. Ginger jam brings back memory 2; the answer picks the version, for good.
    const campaign::Memory *jam = campaign::NextMemory(memories, save, "ti", "recipe", "mut_gung");
    REQUIRE(jam);
    campaign::Save other = save;
    CHECK(campaign::Remember(save, *jam, {{"ti.memory2", "a"}}));
    CHECK(save.versions["dua_tin"] == "dua_tin_ve_kip");
    CHECK(campaign::AbilityFor(save, "dua_tin") == "dua_tin_ve_kip");
    CHECK(campaign::Remember(other, *jam, {{"ti.memory2", "b"}}));
    CHECK(other.versions["dua_tin"] == "dua_tin_phan_qua");
    save.memories.erase("ti_2");
    CHECK(campaign::Remember(save, *jam, {{"ti.memory2", "b"}}));
    CHECK(save.versions["dua_tin"] == "dua_tin_ve_kip"); // final
    // 3. The keepsake brings back memory 3; memory 4 stays locked in the slice.
    REQUIRE(campaign::NextMemory(memories, save, "ti", "keepsake", "giay_gap_tu"));
    CHECK(campaign::NextMemory(memories, save, "ti", "keepsake", "giay_gap_tu")->id == "ti_3");
    for (const campaign::Memory &m : memories.byRecruit.at("ti"))
        if (m.id == "ti_4")
            CHECK_FALSE(campaign::Remember(save, m, {{"ti.memory4", "a"}}));
    CHECK(campaign::AbilityFor(save, "chan_sao") == "chan_sao");
}

TEST_CASE("General drinks: one at a time on one character, refused at the cap; the boost adds to the stat (campaign plan 6)")
{
    const campaign::Recipes recipes = TheRecipes();
    const campaign::Recipe &tea = *recipes.Find("nuoc_gung");
    campaign::Save save;
    save.storage["gung"] = 5;
    for (int i = 0; i < 3; ++i)
        CHECK(campaign::Boost(save, recipes, tea, "bao", 3));
    CHECK_FALSE(campaign::Boost(save, recipes, tea, "bao", 3)); // 3 per stat per character
    CHECK(save.storage["gung"] == 2);                         // the refused one spent nothing
    CHECK(campaign::Boost(save, recipes, tea, "mai", 3));       // another character has her own
    CHECK(campaign::Boosted(save, recipes, "bao", "maxHp") == 6);
    CHECK(campaign::Boosted(save, recipes, "ti", "maxHp") == 0);
    // A memory drink is not a boost.
    CHECK_FALSE(campaign::Boost(save, recipes, *recipes.Find("mut_gung"), "ti", 3));
}

TEST_CASE("Each one at the cafe always has a default line when nothing is new, turning over visit by visit (the user, 2026-11-24 23:57, note 29)")
{
    campaign::Talks talks;
    std::vector<core::Diagnostic> d;
    REQUIRE(campaign::LoadTalks(data / "talks", data / "scenes", TheCast(), talks, d));
    campaign::Save save;
    save.facts["won:t1"] = "1", save.facts["won:t2s"] = "1";
    // 1. Default lines never count as new.
    for (const campaign::Talk *t : campaign::OpenTalks(talks, save, 99))
        CHECK_FALSE(t->idle);
    // 2. Le Thanh, Ba Mai and Doctor Bao each have one; asking again gives the next.
    for (const char *who : {"lethanh", "mai", "bao"})
    {
        INFO(who);
        const campaign::Talk *first = campaign::IdleTalk(talks, save, who);
        REQUIRE(first != nullptr);
        CHECK(first->who == who);
        const campaign::Talk *second = campaign::IdleTalk(talks, save, who);
        REQUIRE(second != nullptr);
        CHECK(second->id != first->id);
    }
    CHECK(campaign::IdleTalk(talks, save, "nobody") == nullptr);
}

TEST_CASE("Talks open by stage and facts, at most three new a visit; a fight leaves its facts (campaign plan 6)")
{
    campaign::Talks talks;
    std::vector<core::Diagnostic> d;
    REQUIRE(campaign::LoadTalks(data / "talks", data / "scenes", TheCast(), talks, d));
    CHECK(d.empty());
    campaign::Save save;
    // 1. The first visit: only Le Thanh's first talk.
    std::vector<const campaign::Talk *> open = campaign::OpenTalks(talks, save, 3);
    REQUIRE(open.size() == 1);
    CHECK(open[0]->id == "lethanh-first");
    campaign::Talked(save, *open[0]);
    CHECK(campaign::OpenTalks(talks, save, 3).empty());
    // 2. After T1, Ba Mai's two (Bao's waits for T2s, after he joins); a fall in the fight opens her worry.
    match::MatchState s;
    s.units.resize(2);
    s.units[0].faction = 0;
    s.units[1].faction = 1;
    s.over = true, s.winner = 0;
    s.log.push_back({match::LogKind::Down, 0, 1, 0});
    campaign::RecordFight(MatchData(), save, "t1", s, {"player", "rat_specter"});
    CHECK(save.facts["won:t1"] == "1");
    CHECK(save.facts["fell:player"] == "1");
    open = campaign::OpenTalks(talks, save, 3);
    CHECK(open.size() == 2);
    // 3. At most `limit`; a talk's facts are set when talked.
    CHECK(campaign::OpenTalks(talks, save, 1).size() == 1);
    save.facts["won:t3"] = "1";
    for (const campaign::Talk *t : campaign::OpenTalks(talks, save, 10))
        if (t->id == "bao-t3")
            campaign::Talked(save, *t);
    CHECK(save.facts["bao.keeper"] == "known");
    // 4. A talk that brings back a memory says so.
    save.facts["won:stage1"] = "1";
    bool memory = false;
    for (const campaign::Talk *t : campaign::OpenTalks(talks, save, 10))
        memory |= t->memory == "ti_1";
    CHECK(memory);
    // 5. Talking with Le Thanh after Stage 1 teaches the two ginger recipes; Ti's idle lines open once he joined.
    for (const campaign::Talk *t : campaign::OpenTalks(talks, save, 10))
        if (t->id == "lethanh-ti")
            campaign::Talked(save, *t);
    CHECK(save.recipes.contains("mut_gung"));
    CHECK(save.recipes.contains("nuoc_gung"));
    save.facts["joined:ti"] = "1";
    bool idle = false;
    for (const campaign::Talk *t : campaign::OpenTalks(talks, save, 10))
        idle |= t->who == "ti";
    CHECK(idle);
}

TEST_CASE("The campaign save: a round trip, a wrong version refused, a corrupt file kept, an interrupted write harmless (campaign plan 7)")
{
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "umbra-campaign-save";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path file = dir / "campaign.json";
    campaign::Save save;
    save.player = {"Lan", "female"};
    save.roster = {{"player", 3, 40}, {"bao", 3, 10}};
    save.storage = {{"gung", 2}};
    save.recipes = {"mut_gung"};
    save.facts = {{"won:t1", "1"}};
    save.versions = {{"dua_tin", "dua_tin_ve_kip"}};
    save.rooms["sk_props"] = {true, 1, {"x"}};
    save.chapter = 1, save.riteUnlocked = true, save.riteUsed = false;
    std::string error;
    // 1. A round trip.
    REQUIRE(campaign::SaveCampaign(file, save, error));
    campaign::Save back;
    REQUIRE(campaign::LoadCampaign(file, back, error));
    CHECK(back.player.name == "Lan");
    CHECK(back.roster.size() == 2);
    CHECK(back.roster[0].xp == 40);
    CHECK(back.storage["gung"] == 2);
    CHECK(back.versions["dua_tin"] == "dua_tin_ve_kip");
    CHECK(back.rooms["sk_props"].refill == 1);
    CHECK(back.riteUnlocked);
    // 2. A wrong version is refused; a corrupt file is reported and left alone.
    std::ofstream(file) << R"({"format":"umbra.lc-campaign","version":1})";
    CHECK_FALSE(campaign::LoadCampaign(file, back, error));
    CHECK(error.find("version") != std::string::npos);
    std::ofstream(file) << "{ broken";
    CHECK_FALSE(campaign::LoadCampaign(file, back, error));
    CHECK(fs::file_size(file) > 0);
    // 3. A write that stops halfway (a stale temp file) does not replace the good file.
    REQUIRE(campaign::SaveCampaign(file, save, error));
    std::ofstream(file.string() + ".tmp") << "{ half";
    REQUIRE(campaign::LoadCampaign(file, back, error));
    CHECK(back.player.name == "Lan");
}

TEST_CASE("Progress: a win shares XP (capped in skirmishes), keeps finds and refills skirmishes; a loss keeps only the rite's use (campaign plan 7)")
{
    const match::MatchDef def = MatchData();
    const std::vector<campaign::StageDef> stages = TheStages();
    const auto stage = [&](const char *id) { return *std::ranges::find(stages, std::string(id), &campaign::StageDef::id); };
    campaign::Save save;
    save.roster = {{"player", 1, 0}};
    save.party = {"player"};
    // 1. T1 won: 100 XP, level 2; the room cleared; the skirmishes that are open refill.
    campaign::BuiltStage t1 = campaign::BuildStage(def, TheCast(), TheGrowth(), stage("t1"), {{"player", 1, {}}}, 0, {}, 3);
    t1.state.over = true, t1.state.winner = 0;
    t1.state.found = {static_cast<std::int8_t>(std::ranges::find(def.items, std::string("gung"), &match::ItemDef::id) - def.items.begin())};
    campaign::WinStage(def, save, stage("t1"), t1, stages);
    CHECK(save.roster[0].level == 2);
    CHECK(save.rooms["t1"].cleared);
    CHECK(save.storage["gung"] == 1);
    CHECK(save.facts["won:t1"] == "1");
    // 2. A skirmish gives no XP past the next story stage's level (T3: 2).
    campaign::BuiltStage sk = campaign::BuildStage(def, TheCast(), TheGrowth(), stage("sk_props"), {{"player", 2, {}}}, 0, {}, 3);
    sk.state.over = true, sk.state.winner = 0;
    const int xp = save.roster[0].xp;
    campaign::WinStage(def, save, stage("sk_props"), sk, stages);
    CHECK(save.roster[0].level == 2);
    CHECK(save.roster[0].xp == xp);
    // 3. A story win refills skirmishes: the next twist.
    const int refill = save.rooms["sk_props"].refill;
    campaign::BuiltStage t3 = campaign::BuildStage(def, TheCast(), TheGrowth(), stage("t3"), {{"player", 2, {}}}, 0, {}, 3);
    t3.state.over = true, t3.state.winner = 0;
    campaign::WinStage(def, save, stage("t3"), t3, stages);
    CHECK(save.rooms["sk_props"].refill == refill + 1);
    CHECK(save.riteUnlocked);
    // 4. A lost stage keeps nothing, except that the rite was used.
    const campaign::Save before = save;
    campaign::BuiltStage lost = campaign::BuildStage(def, TheCast(), TheGrowth(), stage("stage1"), {{"player", 3, {}}}, 0, {}, 3);
    lost.state.over = true, lost.state.winner = 1, lost.state.riteUsed = true;
    lost.state.found = t1.state.found;
    const campaign::Save after = campaign::LoseStage(before, lost);
    CHECK(after.storage == before.storage);
    CHECK(after.roster[0].xp == before.roster[0].xp);
    CHECK(after.riteUsed);
}

TEST_CASE("The theatre map: tickets open in order with marks, the cursor reaches every ticket, panels per kind (campaign plan 7)")
{
    campaign::TheatreMap map;
    std::vector<core::Diagnostic> d;
    REQUIRE(campaign::LoadMap(data / "map.json", TheStages(), map, d));
    CHECK(d.empty());
    Text().Fill(map);
    campaign::Save save;
    const auto mark = [&](const std::string &id) {
        for (const auto &t : campaign::MapView(map, save, TheStages()))
            if (t.id == id)
                return t.mark;
        return std::string("none");
    };
    // 1. A new game: T1 is next, T2 and T3 locked (?), the box office unseen; after T1, T2 is next.
    CHECK(mark("t1") == "next");
    CHECK(mark("t2") == "locked");
    CHECK(mark("t3") == "locked");
    CHECK(mark("makeup") == "unknown");
    save.facts = {{"won:t1", "1"}};
    save.rooms["t1"].cleared = true;
    CHECK(mark("t2") == "next");
    CHECK(mark("t2s") == "locked");
    // 2. After the tutorial ladder (T1, T2, T2s, T3): T3 cleared, the skirmishes open, Stage 1 next.
    save.facts = {{"won:t1", "1"}, {"won:t2", "1"}, {"won:t2s", "1"}, {"won:t3", "1"}};
    save.rooms["t1"].cleared = save.rooms["t2"].cleared = save.rooms["t2s"].cleared = save.rooms["t3"].cleared = true;
    CHECK(mark("t3") == "cleared");
    CHECK(mark("sk_props") == "skirmish");
    CHECK(mark("stage1") == "next");
    CHECK(mark("sk_boxoffice") == "locked");
    // 3. The cursor: from T1, arrows reach every ticket.
    std::set<std::string> seen{"t1"};
    std::vector<std::string> todo{"t1"};
    while (!todo.empty())
    {
        const std::string at = todo.back();
        todo.pop_back();
        for (const char dir : {'u', 'd', 'l', 'r'})
            if (const std::string n = campaign::Nearest(map, at, dir); !n.empty() && seen.insert(n).second)
                todo.push_back(n);
    }
    CHECK(seen.size() == map.tickets.size());
    // 4. A skirmish's panel names its twist; a locked one says what opens it.
    save.rooms["sk_props"].refill = 1;
    for (const auto &t : campaign::MapView(map, save, TheStages()))
    {
        if (t.id == "sk_props")
            CHECK(t.detail == "Chuột trong tường");
        if (t.id == "makeup")
            CHECK(t.detail == "Mở sau Phòng vé. Gương trong đó không phản chiếu ai.");
    }
}

TEST_CASE("Around a stage: who may fight, the revive warning, the result's gains, game over's reason and the rite's box (campaign plan 7)")
{
    const match::MatchDef def = MatchData();
    const std::vector<campaign::StageDef> stages = TheStages();
    const auto stage = [&](const char *id) { return *std::ranges::find(stages, std::string(id), &campaign::StageDef::id); };
    campaign::Save save;
    save.roster = {{"player", 3, 0}, {"bao", 3, 0}, {"mai", 3, 0}, {"ti", 3, 0}};
    // 1. Pre-stage: the player locked in, up to the cap; no reviver in the party warns.
    campaign::PreStage pre = campaign::PreStageFor(def, TheCast(), TheGrowth(), save, stage("stage1"), {"player", "bao"});
    CHECK(pre.cap == 3);
    CHECK(pre.party.front() == "player");
    CHECK(pre.reviveWarning); // Bao has no revive; Ba Mai's cold rice is
    pre = campaign::PreStageFor(def, TheCast(), TheGrowth(), save, stage("stage1"), {"player", "mai"});
    CHECK_FALSE(pre.reviveWarning);
    pre = campaign::PreStageFor(def, TheCast(), TheGrowth(), save, stage("stage1"), {"bao", "mai", "ti", "player"});
    CHECK(pre.party.size() == 3);
    CHECK(pre.party.front() == "player");
    // 2. The result: levels gained, finds, freed, as the save changed.
    campaign::Save after = save;
    after.roster[0].level = 4;
    after.storage["gung"] = 1;
    after.facts["freed:ti"] = "1";
    const campaign::Result r = campaign::ResultOf(save, after);
    REQUIRE(r.levels.size() == 1);
    CHECK(r.levels[0] == "player");
    CHECK(r.found.at("gung") == 1);
    CHECK(r.freed == std::vector<std::string>{"ti"});
    // XP gained per member, across a level (feedback 2026-11-24 16:57, note 11).
    CHECK(r.xp.at("player") == (after.roster[0].level - save.roster[0].level) * campaign::xpPerLevel + after.roster[0].xp - save.roster[0].xp);
    // 3. Game over: the reason; the rite only for the player's fall, unlocked, unused, never in T1.
    match::MatchState s;
    s.over = true, s.winner = 1, s.loss = match::Loss::PlayerFell;
    save.riteUnlocked = true;
    campaign::Over o = campaign::OverFor(save, stage("stage1"), s);
    CHECK(o.reason == "playerFell");
    CHECK(o.rite);
    CHECK_FALSE(o.retryOnly);
    CHECK_FALSE(campaign::OverFor(save, stage("t1"), s).rite);
    CHECK(campaign::OverFor(save, stage("t1"), s).retryOnly);
    s.loss = match::Loss::Trapped;
    CHECK_FALSE(campaign::OverFor(save, stage("stage1"), s).rite);
    CHECK(campaign::OverFor(save, stage("stage1"), s).reason == "trapped");
    s.loss = match::Loss::PlayerFell;
    save.riteUsed = true;
    CHECK_FALSE(campaign::OverFor(save, stage("stage1"), s).rite);
}

TEST_CASE("The save goes into a fight: chosen versions replace the base abilities, boosts add HP (campaign plan 7)")
{
    const match::MatchDef def = MatchData();
    const campaign::Recipes recipes = TheRecipes();
    campaign::Save save;
    save.roster = {{"player", 3, 0}, {"ti", 3, 0}};
    save.versions = {{"dua_tin", "dua_tin_ve_kip"}};
    save.boosts["ti"]["nuoc_gung"] = 2;
    const std::vector<campaign::StageDef> stages = TheStages();
    const campaign::StageDef &st = *std::ranges::find(stages, std::string("sk_props"), &campaign::StageDef::id);
    campaign::BuiltStage b = campaign::BuildStage(def, TheCast(), TheGrowth(), st, campaign::PartyOf(save, {"player", "ti"}), 0, save.taken, 3);
    const int before = b.state.units[1].maxHp;
    campaign::ApplySave(def, recipes, save, b);
    const auto id = [&](int a) { return a >= 0 ? def.abilities[static_cast<std::size_t>(a)].id : std::string(); };
    CHECK(std::ranges::any_of(b.state.units[1].abilities, [&](int a) { return id(a) == "dua_tin_ve_kip"; }));
    CHECK(std::ranges::none_of(b.state.units[1].abilities, [&](int a) { return id(a) == "dua_tin"; }));
    CHECK(b.state.units[1].maxHp == before + 4);
    CHECK(b.state.units[1].hp == b.state.units[1].maxHp);
}

TEST_CASE("Every character's first ability is the basic attack, Danh thuong (the user, 2026-11-24)")
{
    const match::MatchDef def = MatchData();
    const campaign::Cast cast = TheCast();
    for (const char *who : {"player", "ti", "bao", "mai", "rat", "keeper", "ghost_attacker", "ghost_supporter", "rat_specter", "thief_specter"})
    {
        INFO(who);
        const match::Unit u = campaign::MakeUnit(def, cast, TheGrowth(), who, 3, 0, false);
        REQUIRE(u.abilities[0] >= 0);
        const match::AbilityDef &a = def.abilities[static_cast<std::size_t>(u.abilities[0])];
        CHECK(a.id == "basic");
        CHECK(a.use == match::Use::Battle);
        CHECK(a.kind == match::AbilityClass::Strike);
        CHECK(a.hpCost == 0);
        CHECK(std::ranges::count(u.abilities, u.abilities[0]) == 1);
    }
}

TEST_CASE("A talk can hand over items into the cafe's storage, once (Le Thanh's first talk; feedback 2026-11-24)", "[campaign][cafe]")
{
    campaign::Talk talk;
    talk.id = "gift", talk.gives = {{"bandage", 2}, {"programme", 1}};
    campaign::Save save;
    save.storage["bandage"] = 1;
    campaign::Talked(save, talk);
    CHECK(save.storage["bandage"] == 3);
    CHECK(save.storage["programme"] == 1);
    campaign::Talked(save, talk); // told again (a repeat talk): nothing more
    CHECK(save.storage["bandage"] == 3);
}

TEST_CASE("The map's lens: the plan fills the area, zooms about a point and never drags off it (feedback 2026-11-24)", "[campaign][map]")
{
    campaign::MapLens lens;
    const float w = 840.0F, h = 560.0F, aspect = 1344.0F / 768.0F;
    // 1. At rest the plan's width fits the area, centred.
    campaign::PlanBox b = campaign::Plan(lens, w, h, aspect);
    CHECK(b.w == 840.0F);
    CHECK(b.h == Catch::Approx(480.0F));
    CHECK(b.y == Catch::Approx(40.0F));
    // 2. Zoom in about the area's middle: twice as large, still centred.
    campaign::Zoom(lens, 1.0F, {420.0F, 280.0F}, w, h, aspect);
    b = campaign::Plan(lens, w, h, aspect);
    CHECK(b.w > 840.0F);
    CHECK(b.x + b.w * 0.5F == Catch::Approx(420.0F).margin(0.5F));
    // 3. A long drag stops with the plan's edge at the area's edge.
    campaign::Drag(lens, {5000.0F, 0.0F}, w, h, aspect);
    b = campaign::Plan(lens, w, h, aspect);
    CHECK(b.x == Catch::Approx(0.0F));
    // 4. Zoomed all the way out, nothing to drag; the zoom stays between 1 and 3.
    for (int i = 0; i < 20; ++i)
        campaign::Zoom(lens, -1.0F, {0.0F, 0.0F}, w, h, aspect);
    CHECK(lens.zoom == 1.0F);
    campaign::Drag(lens, {300.0F, 300.0F}, w, h, aspect);
    CHECK(campaign::Plan(lens, w, h, aspect).x == 0.0F);
}

TEST_CASE("Skills: unlock levels, a starting loadout, four at most, only unlocked ones; the fight takes the loadout (feedback 2026-11-24 15:10, notes 13 and 17)")
{
    const match::MatchDef def = MatchData();
    campaign::Skills skills;
    std::vector<core::Diagnostic> d;
    REQUIRE(campaign::LoadSkills(data / "skills.json", def, skills, d));
    CHECK(d.empty());
    Text().Fill(skills);
    // Every character's timeline (Ky uc): a year, a known skill or none, its words in localization.csv.
    for (const char *who : {"bao", "mai", "ti"})
    {
        INFO(who);
        REQUIRE(skills.timeline.contains(who));
        for (const campaign::Moment &m : skills.timeline.at(who))
        {
            CHECK(m.year > 1800);
            CHECK_FALSE(m.text.empty());
            CHECK((m.skill.empty() || std::ranges::any_of(def.abilities, [&](const match::AbilityDef &a) { return a.id == m.skill; })));
        }
    }
    const auto ids = [&](const match::Unit &u) {
        std::vector<std::string> out;
        for (const int a : u.abilities)
            if (a >= 0)
                out.push_back(def.abilities[static_cast<std::size_t>(a)].id);
        return out;
    };
    const std::vector<std::string> known = ids(campaign::MakeUnit(def, TheCast(), TheGrowth(), "bao", 2, 0, false));
    REQUIRE(known == std::vector<std::string>{"basic", "dao_mo", "chan_doan", "giay_chung_tu"});
    // 1. Each ability's story; a locked one names its level.
    CHECK_FALSE(skills.lore.at("dao_mo").empty());
    CHECK(campaign::UnlockLevel(skills, "giay_chung_tu") == 3);
    CHECK(campaign::UnlockLevel(skills, "dao_mo") == 1);
    // 2. Bao starts with two (the tutorial leaves the rest for the player); the save changes that.
    campaign::Save save;
    CHECK(campaign::Loadout(skills, save, "bao", known, 2) == std::vector<std::string>{"basic", "dao_mo"});
    CHECK(campaign::Equip(skills, save, "bao", known, 2, "chan_doan"));
    CHECK(campaign::Loadout(skills, save, "bao", known, 2) == std::vector<std::string>{"basic", "dao_mo", "chan_doan"});
    CHECK_FALSE(campaign::Equip(skills, save, "bao", known, 2, "giay_chung_tu")); // locked until level 3
    CHECK_FALSE(campaign::Equip(skills, save, "bao", known, 2, "chan_doan"));     // already carried
    CHECK_FALSE(campaign::Equip(skills, save, "bao", known, 2, "slash"));         // not his
    CHECK(campaign::Equip(skills, save, "bao", known, 3, "giay_chung_tu"));
    CHECK(campaign::Unequip(skills, save, "bao", known, 3, "dao_mo"));
    CHECK(campaign::Loadout(skills, save, "bao", known, 3) == std::vector<std::string>{"basic", "chan_doan", "giay_chung_tu"});
    CHECK_FALSE(campaign::Unequip(skills, save, "bao", known, 3, "dao_mo")); // not carried
    // 3. Four at most; someone with no starting loadout carries the first four unlocked.
    const std::vector<std::string> five{"basic", "a", "b", "c", "d"};
    CHECK(campaign::Loadout(skills, save, "nobody", five, 9).size() == 4);
    // 4. The fight: Bao's unit carries exactly his loadout.
    save.roster = {{"player", 2, 0}, {"bao", 3, 0}};
    const std::vector<campaign::StageDef> stages = TheStages();
    const campaign::StageDef &st = *std::ranges::find(stages, std::string("sk_props"), &campaign::StageDef::id);
    campaign::BuiltStage b = campaign::BuildStage(def, TheCast(), TheGrowth(), st, campaign::PartyOf(save, {"player", "bao"}), 0, save.taken, 3);
    campaign::ApplySave(def, TheRecipes(), save, b, &skills);
    const std::size_t bao = static_cast<std::size_t>(std::ranges::find(b.who, std::string("bao")) - b.who.begin());
    REQUIRE(bao < b.state.units.size());
    CHECK(ids(b.state.units[bao]) == std::vector<std::string>{"basic", "chan_doan", "giay_chung_tu"});
}

TEST_CASE("Enemies never revive in a stage: more come as reinforcements instead (feedback 2026-11-24 16:57, note 7)")
{
    const match::MatchDef def = MatchData();
    for (const campaign::StageDef &st : TheStages())
    {
        const campaign::BuiltStage b = campaign::BuildStage(def, TheCast(), TheGrowth(), st, {{"player", st.level, {}}}, 0, {}, 3);
        INFO(st.id);
        CHECK((b.state.noRevive & (1 << 1)) != 0);
    }
}

TEST_CASE("Tutorial enemies fight up close: no Snipe or other ranged ability (the user, 2026-11-24 23:57, note 10)")
{
    const match::MatchDef def = MatchData();
    for (const campaign::StageDef &st : TheStages())
    {
        if (!st.meleeEnemies)
            continue;
        const campaign::BuiltStage b = campaign::BuildStage(def, TheCast(), TheGrowth(), st, {{"player", st.level, {}}}, 0, {}, 3);
        for (const match::Unit &u : b.state.units)
            for (const auto a : u.abilities)
            {
                INFO(st.id << " " << (a >= 0 ? def.abilities[static_cast<std::size_t>(a)].id : ""));
                CHECK((u.faction == 0 || a < 0 || def.abilities[static_cast<std::size_t>(a)].range <= 1));
            }
    }
    CHECK(std::ranges::count_if(TheStages(), &campaign::StageDef::meleeEnemies) == 4); // t1, t2, t2s, t3
}

TEST_CASE("A stage can start the party hurt: startHp per cast id, kept within 1 and the maximum (the tutorial ladder)")
{
    const match::MatchDef def = MatchData();
    campaign::StageDef st = TheStages().front();
    st.startHp = {{"player", 5}, {"mai", 999}};
    st.partyCap = 1;
    const campaign::BuiltStage b = campaign::BuildStage(def, TheCast(), TheGrowth(), st, {{"player", 2, {}}}, 0, {}, 3);
    CHECK(b.state.units[0].hp == 5);
    CHECK(b.state.units[0].maxHp > 5);
}

TEST_CASE("Every scene line and choice has Vietnamese and English; English swaps in when chosen (the user, 2026-11-24 19:40)")
{
    for (const auto &file : std::filesystem::directory_iterator(data / "scenes"))
    {
        campaign::Scene scene;
        std::vector<core::Diagnostic> d;
        REQUIRE(campaign::LoadScene(file.path(), TheCast(), scene, d));
        for (const campaign::Line &l : scene.lines)
        {
            INFO(file.path().filename().string() << " " << l.id);
            CHECK_FALSE(l.text.empty());
            CHECK_FALSE(l.en.empty());
            for (const campaign::Choice &c : l.choices)
                CHECK_FALSE(c.en.empty());
        }
        campaign::Scene english = scene;
        campaign::UseEnglish(english);
        CHECK(english.lines.front().text == scene.lines.front().en);
    }
}

TEST_CASE("Every stage, ticket, cast member, skill, recipe and memory has its text in localization.csv (the user, 2026-11-24 19:40)")
{
    CampaignText text;
    REQUIRE(text.table.Load(std::filesystem::path(UMBRA_LASTCURTAIN_APP_ASSETS) / "localization.csv", *std::make_unique<std::string>()));
    const auto has = [&](const std::string &key) {
        INFO(key);
        CHECK(text.table.text.contains(key));
    };
    for (const campaign::StageDef &s : TheStages())
    {
        has("stage." + s.id + ".title"), has("stage." + s.id + ".aim");
        for (std::size_t i = 0; i < s.twists.size(); ++i)
            has("stage." + s.id + ".twist." + std::to_string(i));
    }
    campaign::TheatreMap map;
    std::vector<core::Diagnostic> d;
    REQUIRE(campaign::LoadMap(data / "map.json", TheStages(), map, d));
    for (const campaign::Ticket &t : map.tickets)
    {
        has("ticket." + t.id + ".name");
        if (t.kind == "unknown")
            has("ticket." + t.id + ".lock");
    }
    for (const campaign::CastMember &m : TheCast().members)
        if (m.id != "player" && m.id != "nameless")
            has("cast." + m.id + ".name");
    for (const campaign::Recipe &r : TheRecipes().list)
        has("recipe." + r.id + ".name"), has("recipe." + r.id + ".text");
    // Filled in English, a stage reads English.
    text.english = true;
    campaign::StageDef t1 = TheStages().front();
    text.Fill(t1);
    CHECK(t1.title == "The Stranger");
}

#include <StageGuide.hpp>

TEST_CASE("Tutorials guide each lesson's first use: T1 walks to the lit square before the rat and attacks it with the basic attack (the user, 2026-11-24 19:05, point 1)")
{
    const match::MatchDef def = MatchData();
    for (const campaign::StageDef &st : TheStages())
    {
        INFO(st.id);
        const bool tutorial = st.id == "t1" || st.id == "t2" || st.id == "t2s" || st.id == "t3";
        CHECK(st.guide.empty() != tutorial);
        const campaign::BuiltStage b = campaign::BuildStage(def, TheCast(), TheGrowth(), st, {{"player", st.level, {}}, {"mai", st.level, {}}, {"bao", st.level, {}}}, 0, {}, 3);
        for (const MatchPlay::GuideStep &g : GuideFor(st, b))
            CHECK(g.unit >= 0); // every step names someone in the fight
    }
    const campaign::StageDef t1 = TheStages().front();
    const campaign::BuiltStage b = campaign::BuildStage(def, TheCast(), TheGrowth(), t1, {{"player", 1, {}}}, 0, {}, 3);
    const std::vector<MatchPlay::GuideStep> steps = GuideFor(t1, b);
    REQUIRE(steps.size() == 1);
    CHECK(steps[0].cell == 4 * t1.width + 4);
    CHECK(steps[0].act == "attack");
    CHECK(b.who[static_cast<std::size_t>(steps[0].target)] == "rat_specter");
    CHECK(steps[0].pick == "basic");
}
