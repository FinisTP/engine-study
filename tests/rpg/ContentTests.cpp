#include "RpgTestContent.hpp"

using namespace umbra::rpg;
using rpgtest::Parse;

TEST_CASE("The reference content loads without problems")
{
    Content content;
    std::vector<umbra::core::Diagnostic> diagnostics;
    REQUIRE(LoadContent(rpgtest::assetDir / "rpg" / "reference.rpg.json", content, diagnostics));
    REQUIRE(diagnostics.empty());
    REQUIRE(content.FindSkill("venom_strike")->prerequisites == std::vector<std::string>{"heavy_blow"});
    REQUIRE(content.FindSkill("strike")->timing.range > 0.0F);
    REQUIRE(content.FindStatus("weakened")->seconds == std::nullopt);
}

TEST_CASE("Invalid definitions are rejected with a code and nothing is loaded")
{
    REQUIRE(Parse(R"("skills":[{"id":"a","effects":[]}])").Has("skill.mode"));
    REQUIRE(Parse(R"("skills":[{"id":"a","modes":["realtime"]}])").Has("skill.timing"));
    REQUIRE(Parse(R"("statuses":[{"id":"s"}])").Has("status.clock"));
    REQUIRE(Parse(R"("statuses":[{"id":"s","turns":1},{"id":"s","turns":2}])").Has("id.duplicate"));
    REQUIRE(Parse(R"("items":[{"id":"x","modifiers":[{"stat":"luck","add":1}],"slot":"weapon"}])").Has("modifier.stat"));
    REQUIRE(Parse(R"("items":[{"id":"x","slot":"armor","stack":5}])").Has("item.stack"));
    REQUIRE(Parse(R"("actors":[{"id":"a","stats":{"maxHealth":0}}])").Has("field.range"));
    REQUIRE(Parse(R"("actors":[{"id":"a","stats":{"attack":3}}])").Has("actor.stats"));
    REQUIRE(Parse(R"("actors":[{"id":"a","stats":{"maxHealth":5},"skills":["nope"]}])").Has("ref.skill"));
    REQUIRE(Parse(R"("skills":[{"id":"a","modes":["turn"],"effects":[{"kind":"status","status":"nope"}]}])").Has("ref.status"));

    const auto broken = Parse(R"("skills":[{"id":"ok","modes":["turn"]},{"id":"bad"}])");
    REQUIRE_FALSE(broken.ok);
    REQUIRE(broken.content.skills.empty()); // all or nothing
}

TEST_CASE("A status without the clock of a mode that uses it fails validation")
{
    // Turn-only status used by a real-time skill: rejected, not silently converted.
    const auto p = Parse(R"("statuses":[{"id":"slow","turns":2}],
        "skills":[{"id":"a","modes":["realtime"],"realtime":{"active":0.2,"range":1},
                   "effects":[{"kind":"status","status":"slow"}]}])");
    REQUIRE_FALSE(p.ok);
    REQUIRE(p.Has("status.clock.unsupported"));
    // Items can be used in both modes, so their statuses need both clocks.
    REQUIRE(Parse(R"("statuses":[{"id":"slow","seconds":2}],"items":[{"id":"x","use":[{"kind":"status","status":"slow"}]}])")
                .Has("status.clock.unsupported"));
}

TEST_CASE("Unknown fields only warn; bad headers fail")
{
    const auto p = Parse(R"("skills":[{"id":"a","modes":["turn"],"colour":"red"}])");
    REQUIRE(p.ok);
    REQUIRE(p.Has("field.unknown"));

    Content content;
    std::vector<umbra::core::Diagnostic> diagnostics;
    REQUIRE_FALSE(ParseContent(R"({"format":"umbra.rpg","version":2})", "x", content, diagnostics));
    REQUIRE_FALSE(ParseContent("not json", "x", content, diagnostics));
}

TEST_CASE("New characters start at full derived health and energy")
{
    const Character hero = rpgtest::Make("hero", "hero");
    const StatBlock stats = DerivedStats(rpgtest::Reference(), hero);
    REQUIRE(Get(stats, Stat::MaxHealth) == 110); // 100 + leather armor 10
    REQUIRE(Get(stats, Stat::Attack) == 14);     // 10 + rusty sword 4
    REQUIRE(hero.health == 110);
    REQUIRE(hero.energy == 30);
    REQUIRE(hero.skills.contains("strike"));
    REQUIRE(hero.inventory.size() == 2);
    REQUIRE_FALSE(MakeCharacter(rpgtest::Reference(), "dragon", "d"));
}

TEST_CASE("Modifiers: flat adds first, then percents, recomputed the same every time")
{
    Character hero = rpgtest::Make("hero", "hero");
    hero.statuses.push_back({"guarded", 2});  // defense +100%
    hero.statuses.push_back({"weakened", 3}); // attack -30%
    const StatBlock stats = DerivedStats(rpgtest::Reference(), hero);
    REQUIRE(Get(stats, Stat::Defense) == 16); // (5 + 3) * 2
    REQUIRE(Get(stats, Stat::Attack) == 9);   // (10 + 4) * 0.7 = 9.8 -> 9
    REQUIRE(DerivedStats(rpgtest::Reference(), hero) == stats);
}

TEST_CASE("Clamping after a lower maximum never heals")
{
    Character hero = rpgtest::Make("hero", "hero");
    hero.equipment.erase(Slot::Armor); // max health 110 -> 100
    ClampResources(rpgtest::Reference(), hero);
    REQUIRE(hero.health == 100);
    hero.health = 40;
    hero.equipment[Slot::Armor] = "leather_armor"; // max back to 110
    ClampResources(rpgtest::Reference(), hero);
    REQUIRE(hero.health == 40);
}
