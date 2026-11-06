#pragma once

#include <SceneFile.hpp>

#include <glm/vec3.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace umbra::scene
{
    // Development checkpoint: the whole world as a scene file plus where the player stands.
    // Files in `directory`: world.scene.json (SaveScene output) and checkpoint.json.
    struct Checkpoint final
    {
        std::string scene;   // asset path of the area it was taken in (for hot reload and portals)
        glm::vec3 feet{0.0F}; // player position
        float yaw = 0.0F;     // player facing
    };

    bool SaveCheckpoint(const world::World &world, const Checkpoint &checkpoint, const std::filesystem::path &directory,
                        std::vector<core::Diagnostic> &diagnostics);

    // Fills the empty `world` and `checkpoint`. Same all-or-nothing rules as LoadScene.
    bool LoadCheckpoint(const std::filesystem::path &directory, const SceneLoadOptions &options, world::World &world,
                        Checkpoint &checkpoint, std::vector<core::Diagnostic> &diagnostics);
}
