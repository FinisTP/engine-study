#include "RpgTestContent.hpp"

#include <Inventory.hpp>
#include <RealtimeEncounter.hpp>

#include <catch2/catch_approx.hpp>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

using namespace umbra;
using Catch::Approx;
using realtime::Phase;
using rpg::Rejection;

namespace
{
    const rpg::Content &C() { return rpgtest::Reference(); }
    constexpr float frame = 1.0F / 60.0F;

    // hero at the origin facing -z; wolf 1.5 m in front (facing the hero); bandit further back.
    struct Arena
    {
        rpg::Character hero = rpgtest::Make("hero", "hero");
        rpg::Character wolf = rpgtest::Make("wolf", "wolf_1");
        rpg::Character bandit = rpgtest::Make("bandit", "bandit_1");
        realtime::RealtimeEncounter encounter{C(), {{&hero, 0}, {&wolf, 1}, {&bandit, 1}}};
        std::uint64_t nextId = 1;

        Arena()
        {
            encounter.SetPlacement(0, {0.0F, 0.0F}, 0.0F);
            encounter.SetPlacement(1, {0.0F, -1.5F}, glm::pi<float>());
            encounter.SetPlacement(2, {0.0F, -6.0F}, glm::pi<float>());
        }
        void Run(float seconds, float step = frame)
        {
            for (float t = 0.0F; t < seconds - 1e-5F; t += step)
                encounter.Update(step);
        }
        int Count(const rpg::Events &events, rpg::EventKind kind)
        {
            return static_cast<int>(std::count_if(events.begin(), events.end(), [&](const rpg::Event &e) { return e.kind == kind; }));
        }
    };
}

TEST_CASE("Reach is range plus a facing cone")
{
    const rpg::RealtimeTiming swing{0, 0.2F, 0, 0, 2.0F, 90.0F};
    REQUIRE(realtime::InReach({0, 0}, 0.0F, {0, -1.9F}, swing));       // straight ahead (-z)
    REQUIRE_FALSE(realtime::InReach({0, 0}, 0.0F, {0, -2.1F}, swing)); // too far
    REQUIRE_FALSE(realtime::InReach({0, 0}, 0.0F, {0, 1.0F}, swing));  // behind
    REQUIRE(realtime::InReach({0, 0}, 0.0F, {0.9F, -1.0F}, swing));    // 42 degrees off: inside 45
    REQUIRE_FALSE(realtime::InReach({0, 0}, 0.0F, {1.1F, -1.0F}, swing));
}

TEST_CASE("The cost is paid when the wind-up ends, and the hit lands once (T06)")
{
    Arena a;
    a.hero.skills.insert("heavy_blow"); // cost 10, wind-up 0.7, active 0.2, cooldown 2
    REQUIRE(a.encounter.StartAction({a.nextId++, 0, "heavy_blow", "", 1}) == Rejection::None);
    a.Run(0.6F);
    REQUIRE(a.encounter.GetPhase(0) == Phase::Windup);
    REQUIRE(a.hero.energy == 30); // nothing paid during the wind-up
    a.Run(0.15F);
    REQUIRE(a.encounter.GetPhase(0) == Phase::Active);
    REQUIRE(a.hero.energy == 20);
    REQUIRE(a.encounter.CooldownLeft(0, "heavy_blow") > 1.8F);
    REQUIRE(a.wolf.health == 45 - rpg::DamageAmount(18, 14, 2));

    a.Run(0.15F, 0.005F); // many updates inside the hit window: still one hit
    REQUIRE(a.wolf.health == 45 - rpg::DamageAmount(18, 14, 2));
    const auto events = a.encounter.TakeEvents();
    REQUIRE(a.Count(events, rpg::EventKind::Damage) == 1);
    REQUIRE(a.Count(events, rpg::EventKind::CostPaid) == 1);
}

TEST_CASE("Busy, cooldown and repeated decisions are rejected without changes")
{
    Arena a;
    REQUIRE(a.encounter.StartAction({1, 0, "strike", "", 1}) == Rejection::None);
    REQUIRE(a.encounter.StartAction({2, 0, "strike", "", 1}) == Rejection::Busy);
    a.Run(0.85F); // strike: 0.3 + 0.2 + 0.3, cooldown 0.4 from commit (0.3) -> ready at 0.7
    REQUIRE(a.encounter.GetPhase(0) == Phase::Idle);
    REQUIRE(a.encounter.StartAction({1, 0, "strike", "", 1}) == Rejection::DuplicateIntent);
    REQUIRE(a.encounter.StartAction({3, 0, "strike", "", 1}) == Rejection::None);
    REQUIRE(a.encounter.StartAction({4, 1, "strike", "", 0}) == Rejection::SkillLocked); // wolves only bite

    a.wolf.health = 45; // the strikes above wounded it; keep it alive so the encounter goes on
    realtime::RealtimeEncounter other(C(), {{&a.hero, 0}, {&a.wolf, 1}});
    other.SetPlacement(1, {0.0F, -1.5F}, 0.0F);
    a.hero.skills.insert("heavy_blow");
    REQUIRE(other.StartAction({1, 0, "heavy_blow", "", 1}) == Rejection::None);
    for (int i = 0; i < 90; ++i)
        other.Update(frame); // 1.5 s: done (1.4 s), cooldown runs until 0.7 + 2.0 s
    REQUIRE(other.GetPhase(0) == Phase::Idle);
    REQUIRE(other.StartAction({2, 0, "heavy_blow", "", 1}) == Rejection::OnCooldown);
}

TEST_CASE("A hit during a wind-up interrupts it for free; misses hit nothing")
{
    Arena a;
    // Both start together: the hero's strike (wind-up 0.3) lands during the wolf's bite wind-up (0.4).
    REQUIRE(a.encounter.StartAction({1, 1, "bite", "", 0}) == Rejection::None);
    REQUIRE(a.encounter.StartAction({2, 0, "strike", "", 1}) == Rejection::None);
    a.Run(0.35F);
    const auto events = a.encounter.TakeEvents();
    REQUIRE(a.Count(events, rpg::EventKind::Interrupted) == 1);
    REQUIRE(a.encounter.GetPhase(1) == Phase::Idle);
    REQUIRE(a.encounter.CooldownLeft(1, "bite") == 0.0F); // never committed
    REQUIRE(a.hero.health == 110);                        // the bite never landed

    // Facing away: the swing finds nobody.
    a.encounter.SetPlacement(0, {0.0F, 0.0F}, glm::pi<float>());
    a.Run(0.5F);
    const int before = a.wolf.health;
    REQUIRE(a.encounter.StartAction({3, 0, "strike", "", 1}) == Rejection::None);
    a.Run(1.0F);
    REQUIRE(a.wolf.health == before);
}

TEST_CASE("Killed after committing: the cost stays spent, once, and nothing lands (T08)")
{
    Arena a;
    a.bandit.health = 1;
    a.encounter.SetPlacement(2, {0.0F, -1.2F}, 0.0F); // right in front of the hero, facing away
    a.encounter.SetPlacement(1, {5.0F, 0.0F}, 0.0F);  // wolf out of the way
    REQUIRE(a.encounter.StartAction({1, 2, "heavy_blow", "", 0}) == Rejection::None);
    a.Run(0.75F); // bandit committed: 20 -> 10 energy
    REQUIRE(a.bandit.energy == 10);
    REQUIRE(a.encounter.StartAction({2, 0, "strike", "", 2}) == Rejection::None);
    a.Run(0.4F);
    REQUIRE_FALSE(a.bandit.IsAlive());
    REQUIRE(a.encounter.GetPhase(2) == Phase::Idle);
    REQUIRE(a.bandit.energy == 10); // no refund, no second charge
    REQUIRE(a.hero.health == 110);
}

TEST_CASE("Items take time to use; statuses tick in seconds")
{
    Arena a;
    a.hero.health = 50;
    REQUIRE(a.encounter.StartAction({1, 0, "", "health_potion", 0}) == Rejection::None);
    a.Run(0.4F);
    REQUIRE(a.hero.health == 50); // still drinking
    a.Run(0.2F);
    REQUIRE(a.hero.health == 80);
    REQUIRE(rpg::CountItem(a.hero, "health_potion") == 2);

    a.wolf.statuses.push_back({"poison", 0, 6.0F, 1.0F});
    a.Run(2.0F, 0.25F);
    REQUIRE(a.wolf.health == 45 - 6);
}

TEST_CASE("Real-time-only fixture: AI against AI ends with a fixed result (regression)")
{
    Arena a;
    int frames = 0;
    // Everyone walks at 3 m/s toward their target and attacks with the conventional AI.
    while (!a.encounter.IsOver() && frames < 120 * 60)
    {
        for (std::size_t i = 0; i < 3; ++i)
        {
            realtime::Decision d = realtime::ChooseRealtimeAction(C(), a.encounter, i);
            a.encounter.SetPlacement(i, a.encounter.GetPosition(i) + d.move * 3.0F * frame, d.yaw);
            if (d.act)
            {
                d.act->id = a.nextId++;
                (void)a.encounter.StartAction(*d.act);
            }
        }
        a.encounter.Update(frame);
        ++frames;
    }
    REQUIRE(a.encounter.IsOver());
    CAPTURE(frames, a.hero.health, rpg::CountItem(a.hero, "health_potion"));
    // Linux GCC/Clang: 398 frames (6.6 s), hero at 39 health, no potion used. Movement uses
    // float trigonometry, which may differ in the last bit between compilers, so allow a margin.
    REQUIRE(a.encounter.Winner() == 0);
    REQUIRE(frames >= 380);
    REQUIRE(frames <= 420);
    REQUIRE(a.hero.health >= 30);
}
