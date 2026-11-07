#pragma once

#include <Diagnostic.hpp>
#include <Model.hpp>
#include <World.hpp>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace umbra::scene
{
    // Scene file format version written by SaveScene. See SceneFormat.md.
    inline constexpr int sceneFormatVersion = 1;

    // Returns the model for an "instance" entry, or nullptr if it can't be loaded.
    using ModelProvider = std::function<const assets::Model *(const std::string &modelPath)>;

    struct SceneLoadOptions final
    {
        std::filesystem::path assetRoot; // if set, model/instance files must exist under it
        ModelProvider models;            // needed only for scenes that use "instance"
    };

    // Validates without creating anything. Appends to `diagnostics`; true if there are no errors.
    bool ValidateScene(const std::filesystem::path &path, const SceneLoadOptions &options, std::vector<core::Diagnostic> &diagnostics);

    // Validates, then fills `world` (which should be empty). On any error `world` is left untouched.
    // Load into a fresh World and swap it in on success so a bad file never half-changes the live scene.
    bool LoadScene(const std::filesystem::path &path, const SceneLoadOptions &options, world::World &world,
                   std::vector<core::Diagnostic> &diagnostics);

    // The same two, for scene text that is not (yet) in a file, e.g. a document being edited.
    // `name` is only used in diagnostics.
    bool ValidateSceneText(const std::string &text, const std::filesystem::path &name, const SceneLoadOptions &options,
                           std::vector<core::Diagnostic> &diagnostics);
    bool LoadSceneText(const std::string &text, const std::filesystem::path &name, const SceneLoadOptions &options,
                       world::World &world, std::vector<core::Diagnostic> &diagnostics);

    // Writes every entity (parents before children) atomically. Instances are written expanded.
    bool SaveScene(const world::World &world, const std::filesystem::path &path, std::vector<core::Diagnostic> &diagnostics);
}
