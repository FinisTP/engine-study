#pragma once

#include <ComfyUi.hpp>
#include <TextureAssistant.hpp>

#include <glm/vec3.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace umbra::localai
{
    // Glue between ComfyUI's image-to-3D and Umbra's model assets, shared by the editor and
    // UmbraTool (roadmap P8D, ModelGen.md).

    // The look for concept pictures of props: "objectPrompt"/"objectNegative" in assets/materials/style.json.
    [[nodiscard]] TextureStyle LoadObjectStyle(const std::filesystem::path &assetRoot);

    // Exported image-to-3D workflows (any name) in a folder that LoadModelWorkflow accepts, sorted.
    [[nodiscard]] std::vector<std::filesystem::path> FindModelWorkflows(const std::filesystem::path &folder);
    // The workflow to use when none was chosen: <project>/tools/comfyui/image-to-3d.api.json if it
    // exists, else the first usable one there, else that path (so the error names where to put it).
    [[nodiscard]] std::filesystem::path DefaultModelWorkflow(const std::filesystem::path &assetRoot);

    // What a model file holds, measured by the game's own importer.
    struct ModelInfo final
    {
        std::size_t triangles = 0;
        std::size_t meshes = 0;
        std::uint32_t textureSize = 0; // largest side of its biggest texture
        glm::vec3 min{0.0F};
        glm::vec3 max{0.0F};
    };
    bool InspectModel(const std::filesystem::path &file, ModelInfo &info, std::vector<core::Diagnostic> &diagnostics);

    // How a generated model was made, written next to it as models/<name>.source.json.
    struct ModelSource final
    {
        std::string picture; // the prompt, or "file: <name>" for a creator's picture
        std::string workflow; // workflow file name
        long long seed = 0;
        int faces = 0;
    };

    // Writes models/<name>.glb, its concept picture (<name>.concept.png) and <name>.source.json
    // (new files only: "_2", "_3"...), then imports it to check it. Returns "models/<name>.glb", or
    // "" with diagnostics (a model the importer refuses is not kept).
    [[nodiscard]] std::string SaveGeneratedModel(const std::filesystem::path &assetRoot, const std::string &name, const std::vector<std::uint8_t> &glb,
                                                 const core::TextureData &picture, const ModelSource &source, const nlohmann::json &workflow,
                                                 std::vector<core::Diagnostic> &diagnostics);

    // A new scene entity for the model: standing on y = ground at `at`, scaled to `height` meters,
    // with a free id. "model" for one mesh, "instance" for several.
    [[nodiscard]] nlohmann::ordered_json PlacedModelEntity(const nlohmann::ordered_json &scene, const std::string &name, const std::string &modelPath,
                                                           const ModelInfo &info, glm::vec3 at, float height);
}
