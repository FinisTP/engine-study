#include <catch2/catch_test_macros.hpp>

#include <GameComponents.hpp>
#include <SceneFile.hpp>

#include <glm/gtc/quaternion.hpp>

using namespace umbra;

namespace
{
    // A test-only component: counts its calls in a shared place.
    int starts = 0, updates = 0;
    class Counter final : public game::GameComponent
    {
    public:
        explicit Counter(const nlohmann::json &) {}
        void Start(game::ComponentContext &) override { ++starts; }
        void Update(game::ComponentContext &) override { ++updates; }
    };
}

UMBRA_COMPONENT("test.counter", Counter);

TEST_CASE("Project components get their registered C++ behaviour (T77)")
{
    // The reference project's Spin is compiled in (projects/reference/game/Spin.cpp).
    REQUIRE(game::IsRegistered("spin"));
    CHECK_FALSE(game::RegisterComponent("spin", [](const nlohmann::json &) { return nullptr; })); // names are unique

    scene::SceneLoadOptions options;
    options.assetRoot = UMBRA_TEST_ASSET_DIRECTORY;
    world::World world;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(scene::LoadSceneText(R"({"format":"umbra.scene","version":1,"entities":[
        {"id":"stone","components":{"spin":{"speed":90}}},
        {"id":"probe","components":{"test.counter":{}, "nobody.made.this":{}}}]})",
                                 "t.scene.json", options, world, diagnostics));

    // Create: one behaviour per registered type; the unregistered type is reported, not fatal.
    game::ComponentSet set;
    const std::vector<std::string> missing = set.Create(world);
    CHECK(missing == std::vector<std::string>{"nobody.made.this"});
    CHECK(starts == 1);

    // One second at 90 degrees per second: a quarter turn around up.
    for (int i = 0; i < 10; ++i)
        set.Update(world, 0.1F);
    CHECK(updates == 10);
    const glm::quat r = world.GetLocalTransform(world.Find("stone"))->rotation;
    CHECK(std::abs(glm::degrees(glm::angle(r)) - 90.0F) < 0.1F);
}
