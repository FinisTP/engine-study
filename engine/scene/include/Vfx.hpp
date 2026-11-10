#pragma once

#include <Diagnostic.hpp>
#include <Particles.hpp>
#include <World.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Particle effect files (`vfx/*.vfx.json`, S9). Format and math: engine/core/include/Particles.md.
namespace umbra::scene
{
    // Reads and checks an effect's text; false with diagnostics (code "vfx.*") if it can't be used.
    bool ParseVfx(const std::string &text, const std::string &file, core::VfxEffect &out, std::vector<core::Diagnostic> &diagnostics);
    // The same from a file; nullptr (with diagnostics) if it can't be used. `name` is the asset path.
    std::shared_ptr<const core::VfxEffect> LoadVfx(const std::filesystem::path &file, const std::string &name,
                                                   std::vector<core::Diagnostic> &diagnostics);

    // Effect by asset path (loaded once by the caller); nullptr if it can't be used.
    using VfxFinder = std::function<std::shared_ptr<const core::VfxEffect>(const std::string &path)>;
    // The key an entity's "vfx" component emitter lives under; `slot` separates several per entity.
    [[nodiscard]] std::uint64_t VfxKey(world::Entity entity, std::uint32_t slot = 0);
    // Keeps one emitter running at every entity with a "vfx" component (call once per frame, after
    // UpdateTransforms). Returns the keys in use, for ParticleSystem::KeepOnly.
    std::vector<std::uint64_t> SyncVfx(const world::World &world, core::ParticleSystem &particles, const VfxFinder &effects);
}
