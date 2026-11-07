#include "RpgTestContent.hpp"

#include <SkillTree.hpp>

using namespace umbra::rpg;
using rpgtest::Parse;

TEST_CASE("Unlocking follows prerequisites and spends points once (T12)")
{
    const Content &c = rpgtest::Reference();
    Character hero = rpgtest::Make("hero", "hero"); // 3 points, knows strike

    REQUIRE(Unlock(c, hero, "venom_strike") == UnlockResult::MissingPrerequisite);
    REQUIRE(hero.skillPoints == 3);
    REQUIRE(Unlock(c, hero, "heavy_blow") == UnlockResult::Unlocked);
    REQUIRE(hero.skillPoints == 2);
    REQUIRE(Unlock(c, hero, "heavy_blow") == UnlockResult::AlreadyUnlocked);
    REQUIRE(hero.skillPoints == 2); // a repeated unlock costs nothing
    REQUIRE(Unlock(c, hero, "venom_strike") == UnlockResult::Unlocked);
    REQUIRE(hero.skillPoints == 0);
    REQUIRE(Unlock(c, hero, "guard") == UnlockResult::NotEnoughPoints);
    REQUIRE_FALSE(hero.skills.contains("guard"));
    REQUIRE(Unlock(c, hero, "fireball") == UnlockResult::UnknownSkill);
    REQUIRE_FALSE(CanUnlock(c, hero, "weaken"));
}

TEST_CASE("Cyclic or missing requirements are rejected when content loads")
{
    const auto cycle = Parse(R"("skills":[
        {"id":"a","modes":["turn"],"requires":["c"]},
        {"id":"b","modes":["turn"],"requires":["a"]},
        {"id":"c","modes":["turn"],"requires":["b"]}])");
    REQUIRE_FALSE(cycle.ok);
    REQUIRE(cycle.Has("skill.requires.cycle"));

    REQUIRE(Parse(R"("skills":[{"id":"a","modes":["turn"],"requires":["a"]}])").Has("skill.requires.cycle"));
    REQUIRE(Parse(R"("skills":[{"id":"a","modes":["turn"],"requires":["ghost"]}])").Has("ref.skill"));

    // A diamond (two paths to the same root) is fine.
    REQUIRE(Parse(R"("skills":[
        {"id":"root","modes":["turn"]},
        {"id":"l","modes":["turn"],"requires":["root"]},
        {"id":"r","modes":["turn"],"requires":["root"]},
        {"id":"top","modes":["turn"],"requires":["l","r"]}])").ok);
}
