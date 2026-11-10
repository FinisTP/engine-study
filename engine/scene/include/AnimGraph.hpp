#pragma once

#include <Animator.hpp>
#include <Diagnostic.hpp>
#include <Model.hpp>
#include <World.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Animation graph files (`animations/<name>.anim.json`, S8): format in AnimGraph.md.
namespace umbra::scene
{
    // Reads a graph; false (with diagnostics naming the field) if it is not valid.
    bool ParseAnimGraph(const std::string &text, const std::string &file, core::AnimGraph &graph, std::vector<core::Diagnostic> &diagnostics);

    // Reads and checks a graph file; nullptr (with diagnostics) if it can't be used. `name` is the
    // asset path shown in messages.
    std::shared_ptr<const core::AnimGraph> LoadAnimGraph(const std::filesystem::path &file, const std::string &name,
                                                         std::vector<core::Diagnostic> &diagnostics);

    // What BindAnimators needs from the game: a loaded model by path, and a graph by path.
    using ModelFinder = std::function<std::shared_ptr<const assets::Model>(const std::string &model)>;
    using GraphFinder = std::function<std::shared_ptr<const core::AnimGraph>(const std::string &graph)>;

    // Gives every entity with an "animation" component an animator for its renderable's model
    // (and every other entity drawing a rigged model a still one, holding the rest pose).
    // Clips are the model's own, or "models/other.glb#clip" from another model with the same joint
    // names. Problems (no skeleton, a missing clip, a bad graph) are reported and the entity stays still.
    void BindAnimators(world::World &world, const ModelFinder &models, const GraphFinder &graphs, std::vector<core::Diagnostic> &diagnostics);
}
