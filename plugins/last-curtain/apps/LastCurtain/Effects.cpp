#include "Effects.hpp"

#include <Log.hpp>
#include <Renderer.hpp>
#include <TextureTools.hpp>
#include <Vfx.hpp>

#include <glm/common.hpp>

#include <algorithm>

void Effects::Load(const umbra::assets::AssetFolders &assets, umbra::render::Renderer &renderer, bool reduced)
{
    library_.clear();
    std::map<std::string, umbra::core::TextureHandle> &pictures = pictures_;
    std::error_code ec;
    for (const auto &file : std::filesystem::directory_iterator(assets / "vfx", ec))
    {
        const std::string name = file.path().filename().string();
        if (!name.ends_with(".vfx.json"))
            continue;
        // 1. The effect file; a broken one is reported and left out.
        std::vector<umbra::core::Diagnostic> d;
        const auto loaded = umbra::scene::LoadVfx(file.path(), "vfx/" + name, d);
        if (!loaded)
        {
            for (const auto &x : d)
                umbra::diagnostics::Error(x.file + ": " + x.field + ": " + x.message);
            continue;
        }
        umbra::core::VfxEffect effect = *loaded;
        // 2. Its picture, loaded once; one that won't load leaves soft dots, said once.
        if (effect.textured)
        {
            if (!pictures.contains(effect.texture))
            {
                umbra::core::TextureData data;
                std::string error;
                pictures[effect.texture] = umbra::assets::LoadImageFile(assets / effect.texture, data, error) ? renderer.CreateTexture(data) : umbra::core::TextureHandle{};
                if (!pictures[effect.texture])
                    umbra::diagnostics::Info("Effect picture " + effect.texture + " not found; soft dots instead.");
            }
            effect.image = pictures[effect.texture];
        }
        // 3. Fewer particles when asked (half of each count).
        if (reduced)
            effect.rate *= 0.5F, effect.burst = (effect.burst + 1) / 2, effect.max = std::max(1, effect.max / 2);
        library_[name.substr(0, name.size() - 9)] = std::make_shared<const umbra::core::VfxEffect>(effect);
    }
}

void Effects::Burst(const std::string &name, glm::vec3 at)
{
    if (const auto it = library_.find(name); it != library_.end() && particles_.Count() < budget)
        particles_.Spawn(it->second, at);
}

void Effects::Fly(const std::string &name, glm::vec3 from, glm::vec3 to, float seconds)
{
    const auto it = library_.find(name);
    if (it == library_.end() || particles_.Count() >= budget)
        return;
    flights_.push_back({nextFlight_++, it->second, from, to, 0.0F, std::max(seconds, 0.05F)});
    particles_.Attach(flights_.back().key, it->second, from);
}

void Effects::Follow(std::uint64_t key, const std::string &name, glm::vec3 at)
{
    if (const auto it = library_.find(name); it != library_.end())
        particles_.Attach(key, it->second, at), followed_.push_back(key);
}

void Effects::EndFrame()
{
    // Followers and flights still going are kept; the rest stop and let their particles finish.
    std::vector<std::uint64_t> keep = followed_;
    for (const Flight &f : flights_)
        keep.push_back(f.key);
    particles_.KeepOnly(keep);
    followed_.clear();
}

void Effects::Update(float seconds)
{
    // 1. Flights move along their line (eased in and out); a landed one is let go.
    for (Flight &f : flights_)
    {
        f.t = std::min(f.t + seconds / f.seconds, 1.0F);
        const float e = f.t * f.t * (3.0F - 2.0F * f.t);
        particles_.Attach(f.key, f.effect, glm::mix(f.from, f.to, e));
    }
    std::erase_if(flights_, [](const Flight &f) { return f.t >= 1.0F; });
    // 2. Every particle steps.
    particles_.Update(seconds);
}
