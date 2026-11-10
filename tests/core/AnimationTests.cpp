#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Animator.hpp>

#include <glm/gtc/matrix_transform.hpp>

using namespace umbra::core;
using Catch::Approx;

namespace
{
    // Two joints in a chain: "root" at the origin, "arm" 1 m up the Y axis.
    Skeleton Chain()
    {
        Skeleton s;
        s.joints.push_back({"root", -1, glm::mat4(1.0F), glm::mat4(1.0F), {}});
        Joint arm{"arm", 0, glm::mat4(1.0F), glm::translate(glm::mat4(1.0F), {0, -1, 0}), {}};
        arm.rest.position = {0, 1, 0};
        s.joints.push_back(arm);
        REQUIRE(SortJoints(s));
        return s;
    }

    // Turns "root" a quarter around Z over one second.
    Clip Turn(const std::string &name, float degrees = 90.0F)
    {
        const glm::quat end = glm::angleAxis(glm::radians(degrees), glm::vec3(0, 0, 1));
        Clip c{name, 1.0F, {}};
        c.channels.push_back({"root", ClipChannel::Path::Rotation, false, {0.0F, 1.0F}, {{0, 0, 0, 1}, {end.x, end.y, end.z, end.w}}});
        return c;
    }

    glm::vec3 Move(const std::vector<glm::mat4> &palette, std::size_t joint, glm::vec3 point)
    {
        return glm::vec3(palette[joint] * glm::vec4(point, 1.0F));
    }
}

TEST_CASE("Clips sample between keys, loop or hold, and step keys hold (T87)", "[animation]")
{
    const Skeleton s = Chain();
    const Clip clip = Turn("turn");
    const std::vector<int> map = MapClip(clip, s);
    REQUIRE(map == std::vector<int>{0});
    std::vector<JointPose> pose;
    SampleClip(clip, map, 0.5F, false, s, pose);
    CHECK(glm::degrees(glm::angle(pose[0].rotation)) == Approx(45.0F).margin(0.01F));
    SampleClip(clip, map, 1.5F, true, s, pose); // loops: 0.5 again
    CHECK(glm::degrees(glm::angle(pose[0].rotation)) == Approx(45.0F).margin(0.01F));
    SampleClip(clip, map, 1.5F, false, s, pose); // holds the end
    CHECK(glm::degrees(glm::angle(pose[0].rotation)) == Approx(90.0F).margin(0.01F));
    CHECK(pose[1].position == glm::vec3(0, 1, 0)); // not in the clip: rest pose

    Clip step = Turn("step");
    step.channels[0].step = true;
    SampleClip(step, MapClip(step, s), 0.9F, false, s, pose);
    CHECK(glm::degrees(glm::angle(pose[0].rotation)) == Approx(0.0F).margin(0.01F));

    // A channel for a joint this skeleton doesn't have is skipped.
    Clip other = Turn("other");
    other.channels[0].joint = "tail";
    CHECK(MapClip(other, s) == std::vector<int>{-1});
}

TEST_CASE("The palette moves bind-pose vertices with their joints (T87)", "[animation]")
{
    const Skeleton s = Chain();
    std::vector<glm::mat4> palette;
    ComputePalette(s, RestPose(s), palette);
    // At rest every vertex stays where it is.
    CHECK(Move(palette, 1, {0, 2, 0}).y == Approx(2.0F));
    // Root turned 90 degrees about Z: a point 2 m up the arm swings to x = -2.
    std::vector<JointPose> pose = RestPose(s);
    pose[0].rotation = glm::angleAxis(glm::radians(90.0F), glm::vec3(0, 0, 1));
    ComputePalette(s, pose, palette);
    const glm::vec3 p = Move(palette, 1, {0, 2, 0});
    CHECK(p.x == Approx(-2.0F).margin(1e-5));
    CHECK(p.y == Approx(0.0F).margin(1e-5));
    // Halfway between the two poses: 45 degrees.
    std::vector<JointPose> half;
    BlendPoses(RestPose(s), pose, 0.5F, half);
    CHECK(glm::degrees(glm::angle(half[0].rotation)) == Approx(45.0F).margin(0.01F));
}

TEST_CASE("The animator picks states by flags, numbers and triggers, and fades between them (T87)", "[animation]")
{
    auto skeleton = std::make_shared<Skeleton>(Chain());
    auto clips = std::make_shared<std::vector<Clip>>(std::vector<Clip>{Turn("idle", 0.0F), Turn("walk", 30.0F), Turn("attack", 60.0F), Turn("death", 90.0F)});
    auto graph = std::make_shared<AnimGraph>();
    graph->fade = 0.2F;
    AnimState death{"death", "death", false, "dead"};
    AnimState attack{"attack", "attack", false};
    attack.on = "attack";
    AnimState walk{"walk", "walk", true, "", "speed", 0.1F};
    walk.rateParam = "speed";
    walk.ratePer = 2.0F;
    AnimState idle{"idle", "idle"};
    graph->states = {death, attack, walk, idle};
    const auto find = [&](const std::string &name) -> std::shared_ptr<const Clip> {
        for (const Clip &c : *clips)
            if (c.name == name)
                return std::shared_ptr<const Clip>(clips, &c);
        return nullptr;
    };
    Animator a(skeleton, graph, find);
    CHECK_FALSE(a.Missing());

    a.Update(0.1F);
    CHECK(a.State() == "idle");
    a.SetNumber("speed", 4.0F); // walking at twice the clip's speed
    a.Update(0.1F);
    CHECK(a.State() == "walk");
    CHECK(a.StateTime() == Approx(0.2F)); // rate 4 / 2 = 2
    a.Trigger("attack");
    a.Update(0.1F);
    CHECK(a.State() == "attack");
    a.Update(0.5F);
    CHECK(a.State() == "attack"); // plays to its end...
    a.Update(0.6F);
    a.Update(0.01F);
    CHECK(a.State() == "walk"); // ...then the number decides again
    a.SetFlag("dead", true);
    a.Trigger("attack");
    a.Update(0.1F);
    CHECK(a.State() == "death"); // first in the list wins
    // Fading: 0.1 s into a 0.2 s fade, the pose is between walk and death.
    a.Update(2.0F);
    CHECK(a.State() == "death");
    CHECK(a.Palette().size() == 2);

    // A graph naming a clip the model doesn't have is reported.
    graph->states.push_back({"wave", "wave"});
    Animator b(skeleton, graph, find);
    CHECK(b.Missing());
    CHECK(b.MissingClip() == "wave");
}
