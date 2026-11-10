#pragma once

#include <ComfyUi.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace umbra::localai
{
    // Glue between ComfyUI's rigging / text-to-motion workflows and Umbra's assets (S8,
    // AnimationGen.md). Shared by the editor and UmbraTool.

    // Exported workflows of this kind in a folder that LoadAnimWorkflow accepts, sorted.
    [[nodiscard]] std::vector<std::filesystem::path> FindAnimWorkflows(const std::filesystem::path &folder, AnimKind kind);
    // <project>/tools/comfyui/rig.api.json (or motion.api.json) if it exists, else the first usable
    // one there, else that path (so the error names where to put it).
    [[nodiscard]] std::filesystem::path DefaultAnimWorkflow(const std::filesystem::path &assetRoot, AnimKind kind);

    // FBX -> GLB with Blender in the background: UMBRA_BLENDER, else "blender" on the PATH, else the
    // usual Windows install folder. False with a diagnostic naming what to install.
    bool FbxToGlb(const std::vector<std::uint8_t> &fbx, std::vector<std::uint8_t> &glb, std::vector<core::Diagnostic> &diagnostics);

    // Writes <folder>/<name>.glb and <name>.source.json (new files only: "_2", "_3"...), then imports
    // it: a rig must have a skeleton, a motion at least one clip; otherwise both files are removed.
    // Returns the asset path ("models/hero_rigged.glb"), or "" with diagnostics.
    [[nodiscard]] std::string SaveAnimResult(const std::filesystem::path &assetRoot, AnimKind kind, const std::string &name,
                                             const std::vector<std::uint8_t> &glb, const nlohmann::ordered_json &source,
                                             std::vector<core::Diagnostic> &diagnostics);
}
