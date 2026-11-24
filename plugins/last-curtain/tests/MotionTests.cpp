#include <Motion.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include <filesystem>
#include <fstream>

using Catch::Approx;
using namespace umbra::night;

namespace
{
    Motions Parse(const std::string &text)
    {
        std::string error;
        Motions m = Motions::Parse(text, error);
        INFO(error);
        CHECK(error.empty());
        return m;
    }
    const char *two = R"({
        "_note": "ignored",
        "lift": { "seconds": 1.0, "faces": [[0, "calm"], [0.5, "happy"]],
                  "keys": [{ "t": 0, "y": 0, "sx": 1 }, { "t": 1.0, "y": 1.0 }] },
        "spin": { "seconds": 2.0, "loop": true, "flip": true, "trail": true,
                  "keys": [{ "t": 0, "rot": 0, "tint": [1, 0, 0, 0] }, { "t": 2.0, "rot": 20, "tint": [1, 0, 0, 1] }] } })";
}

TEST_CASE("A motion eases between its keys and keeps a field's last value (T267)", "[night][motion]")
{
    const Motions m = Parse(two);
    CHECK(m.Sample("lift", 0.0F).y == Approx(0.0F));
    CHECK(m.Sample("lift", 0.5F).y == Approx(0.5F));    // smoothstep(0.5) = 0.5
    CHECK(m.Sample("lift", 0.25F).y == Approx(0.15625F)); // smoothstep(0.25)
    CHECK(m.Sample("lift", 3.0F).y == Approx(1.0F));    // past the end: held
    CHECK(m.Sample("lift", 0.7F).sx == Approx(1.0F));   // set once, kept
    CHECK(m.Sample("lift", 0.7F).rot == Approx(0.0F));  // never set: the neutral pose
    CHECK(m.Sample("lift", 0.2F).face == "calm");
    CHECK(m.Sample("lift", 0.6F).face == "happy");
}

TEST_CASE("A looping motion wraps; flip, trail and tint come through (T268)", "[night][motion]")
{
    const Motions m = Parse(two);
    CHECK(m.Loops("spin"));
    CHECK_FALSE(m.Loops("lift"));
    CHECK(m.Sample("spin", 2.5F).rot == Approx(m.Sample("spin", 0.5F).rot));
    const MotionPose p = m.Sample("spin", 1.0F);
    CHECK(p.flip);
    CHECK(p.trail);
    CHECK(p.tint.a == Approx(0.5F));
    CHECK(p.tint.r == Approx(1.0F));
    CHECK(m.Seconds("spin") == Approx(2.0F));
}

TEST_CASE("An unknown motion is the neutral pose; bad data is refused with a reason (T269)", "[night][motion]")
{
    const Motions m = Parse(two);
    const MotionPose p = m.Sample("nope", 0.3F);
    CHECK(p.y == 0.0F);
    CHECK(p.sx == 1.0F);
    CHECK(p.alpha == 1.0F);
    CHECK(p.face == "calm");
    CHECK_FALSE(m.Has("nope"));
    std::string error;
    Motions::Parse("{ not json", error);
    CHECK_FALSE(error.empty());
    error.clear();
    Motions::Parse(R"({ "bad": { "seconds": 0, "keys": [] } })", error);
    CHECK(error.find("bad") != std::string::npos);
    error.clear();
    Motions::Parse(R"({ "late": { "seconds": 1, "keys": [{ "t": 0.5 }, { "t": 0.2 }] } })", error);
    CHECK(error.find("late") != std::string::npos);
}

TEST_CASE("Every motion the game plays is in motions.json and loads (T270)", "[night][motion]")
{
    std::string error;
    const Motions m = Motions::Load(std::filesystem::path(UMBRA_LASTCURTAIN_ASSETS) / "motions.json", error);
    INFO(error);
    REQUIRE(error.empty());
    for (const char *name : {"idle", "walk", "talk", "nod", "refuse", "scheme", "pickup", "drop", "use", "brace", "pass", "fail",
                             "attack", "hit", "guard", "dodge", "flee", "power", "levelup", "fear", "down", "die", "ghost", "revive",
                             "bow", "booed", "haunt", "loom", "twitch", "taunt", "lunge", "vanish"})
        CHECK(m.Has(name));
    for (const std::string &name : m.Names())
        for (float t = 0.0F; t < m.Seconds(name); t += 0.05F)
        {
            const MotionPose p = m.Sample(name, t);
            CHECK(MotionFaceKnown(p.face));
            CHECK(p.alpha >= 0.0F);
            CHECK(p.alpha <= 1.0F);
        }
}

TEST_CASE("The sprite stands on its feet, leans forward, and a flipped one still faces the camera (T271)", "[night][motion]")
{
    const glm::vec3 feet{2.0F, 0.0F, 3.0F}, right{1.0F, 0.0F, 0.0F};
    MotionPose pose;
    // 1. The quad's bottom middle (0, -0.5) lands on the feet; its top middle is a height above.
    const glm::mat4 m = SpriteModel(pose, feet, right, 2.0F, 0.5F, 1.0F, 0.0F);
    const glm::vec3 bottom = glm::vec3(m * glm::vec4(0.0F, -0.5F, 0.0F, 1.0F));
    const glm::vec3 top = glm::vec3(m * glm::vec4(0.0F, 0.5F, 0.0F, 1.0F));
    CHECK(bottom.x == Approx(2.0F));
    CHECK(bottom.y == Approx(0.0F).margin(1e-5));
    CHECK(top.y == Approx(2.0F));
    CHECK(glm::length(glm::vec3(m * glm::vec4(0.5F, 0.0F, 0.0F, 0.0F))) == Approx(0.5F)); // half the width
    // 2. A forward lean moves the top toward where the sprite faces, for either facing.
    pose.rot = 20.0F;
    CHECK(glm::vec3(SpriteModel(pose, feet, right, 2.0F, 0.5F, 1.0F, 0.0F) * glm::vec4(0, 0.5F, 0, 1)).x > 2.0F);
    CHECK(glm::vec3(SpriteModel(pose, feet, right, 2.0F, 0.5F, -1.0F, 0.0F) * glm::vec4(0, 0.5F, 0, 1)).x < 2.0F);
    // 3. Flipping keeps the winding (det > 0), so the quad is not culled; the mirrored quad mesh
    //    (u = 1 - u) turns the picture itself.
    pose.rot = 20.0F;
    CHECK(glm::determinant(glm::mat3(SpriteModel(pose, feet, right, 2.0F, 0.5F, -1.0F, 0.0F))) > 0.0F);
    pose.rot = 0.0F;
    // 4. An offset goes the way the sprite faces, in sprite heights.
    pose.x = 0.5F;
    CHECK(glm::vec3(SpriteModel(pose, feet, right, 2.0F, 0.5F, -1.0F, 0.0F) * glm::vec4(0, -0.5F, 0, 1)).x == Approx(1.0F));
}

TEST_CASE("What happened decides who moves how (T272)", "[night][motion]")
{
    using Cue = std::pair<int, std::string>;
    const auto cues = [](LogEntry e) { return MotionsFor(e); };
    CHECK(cues({0, LogKind::Hit, 1, 2, 3, 4}) == std::vector<Cue>{{1, "attack"}, {2, "hit"}});
    CHECK(cues({0, LogKind::Hit, -1, 2, 3, 4}) == std::vector<Cue>{{2, "hit"}}); // a monster strikes
    CHECK(cues({0, LogKind::Check, 3, 4, 12, 15}) == std::vector<Cue>{{3, "pass"}}); // 12 + 4 >= 15
    CHECK(cues({0, LogKind::Check, 3, 2, 10, 15}) == std::vector<Cue>{{3, "fail"}});
    CHECK(cues({0, LogKind::Effect, 5, -1, 0, -2}) == std::vector<Cue>{{5, "hit"}});
    CHECK(cues({0, LogKind::Effect, 5, -1, 0, 2}) == std::vector<Cue>{{5, "use"}});
    CHECK(cues({0, LogKind::Item, 1, -1, 4, 0}) == std::vector<Cue>{{1, "pickup"}});
    CHECK(cues({0, LogKind::Persuade, 1, 2, 0, 0}) == std::vector<Cue>{{1, "talk"}, {2, "talk"}});
    CHECK(cues({0, LogKind::Down, 4, -1, 0, 0}) == std::vector<Cue>{{4, "down"}});
    CHECK(cues({0, LogKind::Died, 4, -1, 1, 0}) == std::vector<Cue>{{4, "die"}});
    CHECK(cues({0, LogKind::Woke, 4, -1, 0, 0}) == std::vector<Cue>{{4, "revive"}});
    CHECK(cues({0, LogKind::Rose, 4, -1, 0, 0}) == std::vector<Cue>{{4, "revive"}});
    CHECK(cues({0, LogKind::Level, 2, -1, 0, 0}) == std::vector<Cue>{{2, "levelup"}});
    CHECK(cues({0, LogKind::Power, 2, -1, 0, 0}) == std::vector<Cue>{{2, "power"}});
    CHECK(cues({0, LogKind::Opened, 2, -1, 0, 0}).empty());
}

TEST_CASE("A guest's player: one-shots over a base loop, then back (T273)", "[night][motion]")
{
    const Motions m = Parse(two);
    SpriteState s;
    s.Base("spin");
    s.Update(0.5F, m);
    CHECK(s.Motion() == "spin");
    s.Play("lift");
    CHECK(s.Motion() == "lift");
    s.Update(0.6F, m);
    CHECK(s.Pose(m).face == "happy");
    s.Update(0.5F, m); // past lift's one second: back to the base
    CHECK(s.Motion() == "spin");
    s.Hold("lift"); // a held one-shot (down) stays on its last frame
    s.Update(5.0F, m);
    CHECK(s.Motion() == "lift");
    CHECK(s.Pose(m).y == Approx(1.0F));
    s.Release();
    CHECK(s.Motion() == "spin");
}

TEST_CASE("A sprite stands where its feet are drawn: the middle of the opaque pixels near the bottom", "[motion][sprite]")
{
    // 1. A 10 x 10 picture whose figure stands in columns 6..8 (the top half wider, which must not count).
    umbra::core::TextureData t;
    t.width = 10, t.height = 10, t.rgba.assign(400, 0);
    for (std::uint32_t y = 0; y < 10; ++y)
        for (std::uint32_t x = y < 5 ? 2 : 6; x <= 8; ++x)
            t.rgba[(y * 10 + x) * 4 + 3] = 255;
    CHECK(umbra::night::FootCentre(t) == Catch::Approx(0.75F)); // columns 6..8: their middle 7.5 of 10
    // 2. Nothing opaque, or no picture: the middle.
    std::fill(t.rgba.begin(), t.rgba.end(), 0);
    CHECK(umbra::night::FootCentre(t) == Catch::Approx(0.5F));
    CHECK(umbra::night::FootCentre({}) == Catch::Approx(0.5F));
}
