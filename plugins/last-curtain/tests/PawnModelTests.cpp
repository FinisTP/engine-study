#include <AnimGraph.hpp>
#include <Animator.hpp>
#include <GltfImporter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>

namespace
{
    // The table's pawn: a rigged model and its graph, from LastCurtain's assets.
    void CheckPawn(const char *model, const char *graph)
    {
        const std::filesystem::path assets = UMBRA_LASTCURTAIN_ASSETS;
        umbra::assets::ImportResult imported = umbra::assets::ImportGltf(assets / "models" / model);
        REQUIRE(imported.Ok());
        const auto shared = std::make_shared<const umbra::assets::Model>(std::move(imported.model));
        REQUIRE(shared->skeleton);
        std::vector<umbra::core::Diagnostic> diagnostics;
        const auto anims = umbra::scene::LoadAnimGraph(assets / "animations" / graph, graph, diagnostics);
        REQUIRE(anims);
        // 1. Every state's clip is in the model.
        umbra::core::Animator animator(std::shared_ptr<const umbra::core::Skeleton>(shared, &*shared->skeleton), anims, [&](const std::string &name) {
            for (const umbra::core::Clip &clip : shared->clips)
                if (clip.name == name)
                    return std::shared_ptr<const umbra::core::Clip>(shared, &clip);
            return std::shared_ptr<const umbra::core::Clip>{};
        });
        CHECK_FALSE(animator.Missing());
        // 2. It stands idle, walks while it moves, and fills one matrix per joint.
        animator.Update(0.1F);
        CHECK(animator.State() == "idle");
        animator.SetNumber("speed", 1.0F);
        animator.Update(0.1F);
        CHECK(animator.State() == "walk");
        CHECK(animator.Palette().size() == shared->skeleton->joints.size());
    }
}

TEST_CASE("The pawn models load with every clip their graphs play (T131)", "[night][pawn]")
{
    CheckPawn("guest.glb", "guest.anim.json");
    CheckPawn("taken.glb", "taken.anim.json");
}
