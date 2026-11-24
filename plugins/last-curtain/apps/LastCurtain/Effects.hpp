#pragma once

#include <AssetFolders.hpp>
#include <Particles.hpp>

#include <glm/vec3.hpp>

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace umbra::render
{
    class Renderer;
}

// The game's effect library (Sequences.md "Effects"): assets/vfx/*.vfx.json with their pictures,
// one-shots at a point, effects that fly from one point to another, and effects that follow someone
// (an enemy's dread). Looks only: nothing here changes a fight.
class Effects final
{
public:
    static constexpr std::size_t budget = 256; // particles alive at once; past it nothing new is made

    // Loads every effect in assets/vfx; a picture that won't load leaves soft dots (one warning each).
    // reduced: half the particles (the "Hiệu ứng hình ảnh: Giảm" setting).
    void Load(const umbra::assets::AssetFolders &assets, umbra::render::Renderer &renderer, bool reduced);
    [[nodiscard]] bool Has(const std::string &name) const { return library_.contains(name); }

    void Burst(const std::string &name, glm::vec3 at);
    void Fly(const std::string &name, glm::vec3 from, glm::vec3 to, float seconds);
    void Follow(std::uint64_t key, const std::string &name, glm::vec3 at); // call each frame it should stay
    void EndFrame();                                                       // whoever was not followed this frame lets go
    void Clear() { particles_.Clear(), flights_.clear(), followed_.clear(); }

    void Update(float seconds); // hurry passes a scaled time: effects keep pace with the lines
    void Extract(umbra::core::DrawList &out) const { particles_.Extract(out); }
    [[nodiscard]] std::size_t Count() const { return particles_.Count(); }

private:
    std::map<std::string, std::shared_ptr<const umbra::core::VfxEffect>> library_;
    std::map<std::string, umbra::core::TextureHandle> pictures_; // kept across loads (a settings change reloads)
    umbra::core::ParticleSystem particles_;
    struct Flight
    {
        std::uint64_t key = 0;
        std::shared_ptr<const umbra::core::VfxEffect> effect;
        glm::vec3 from{0.0F}, to{0.0F};
        float t = 0.0F, seconds = 0.0F;
    };
    std::vector<Flight> flights_;
    std::vector<std::uint64_t> followed_;
    std::uint64_t nextFlight_ = 1ULL << 62; // flights' keys, apart from the followers'
};
