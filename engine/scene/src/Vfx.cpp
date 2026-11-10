#include "Vfx.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iterator>

namespace umbra::scene
{
    namespace
    {
        using json = nlohmann::json;

        // A list of `count` numbers, each within [low, high]; false if it isn't one.
        bool Numbers(const json &value, std::size_t count, float low, float high, float *out)
        {
            if (!value.is_array() || value.size() != count)
                return false;
            for (std::size_t i = 0; i < count; ++i)
            {
                if (!value[i].is_number() || value[i].get<float>() < low || value[i].get<float>() > high)
                    return false;
                out[i] = value[i].get<float>();
            }
            return true;
        }
    }

    bool ParseVfx(const std::string &text, const std::string &file, core::VfxEffect &out, std::vector<core::Diagnostic> &diagnostics)
    {
        const std::size_t before = diagnostics.size();
        const auto bad = [&](const std::string &field, const std::string &message) {
            diagnostics.push_back({core::Severity::Error, "vfx." + field, file, field, message});
        };
        const json root = json::parse(text, nullptr, false);
        if (!root.is_object() || root.value("format", "") != "umbra.vfx" || root.value("version", 0) != 1)
        {
            diagnostics.push_back({core::Severity::Error, "vfx.format", file, "", "not an effect file (\"format\": \"umbra.vfx\", \"version\": 1)"});
            return false;
        }
        // Each field is optional (VfxEffect's defaults); a present one must be the right shape.
        const auto number = [&](const char *key, float low, float high, float &value) {
            if (!root.contains(key))
                return;
            if (root[key].is_number() && root[key].get<float>() >= low && root[key].get<float>() <= high)
                value = root[key].get<float>();
            else
                bad(key, "a number from " + std::to_string(low) + " to " + std::to_string(high));
        };
        const auto list = [&](const char *key, std::size_t count, float low, float high, float *value) {
            if (root.contains(key) && !Numbers(root[key], count, low, high, value))
                bad(key, std::to_string(count) + " numbers from " + std::to_string(low) + " to " + std::to_string(high));
        };
        float burst = 0.0F, max = static_cast<float>(out.max);
        number("rate", 0.0F, 2000.0F, out.rate);
        number("burst", 0.0F, 1000.0F, burst);
        number("duration", 0.0F, 600.0F, out.duration);
        number("radius", 0.0F, 50.0F, out.radius);
        number("gravity", -100.0F, 100.0F, out.gravity);
        number("drag", 0.0F, 60.0F, out.drag);
        number("ring", 0.0F, 0.95F, out.ring);
        number("max", 1.0F, 4096.0F, max);
        list("lifetime", 2, 0.01F, 60.0F, &out.lifetime.x);
        list("velocity", 3, -100.0F, 100.0F, &out.velocity.x);
        list("spread", 3, 0.0F, 100.0F, &out.spread.x);
        list("size", 2, 0.0F, 50.0F, &out.size.x);
        if (root.contains("color") && !(root["color"].is_array() && root["color"].size() == 2 && Numbers(root["color"][0], 4, 0.0F, 64.0F, &out.colorStart.x) &&
                                        Numbers(root["color"][1], 4, 0.0F, 64.0F, &out.colorEnd.x)))
            bad("color", "two colours [[r, g, b, a], [r, g, b, a]]: at birth and at death");
        if (root.contains("blend") && root["blend"] != "add" && root["blend"] != "alpha")
            bad("blend", "\"add\" (glows) or \"alpha\" (covers)");
        out.additive = root.value("blend", json("add")) != "alpha";
        out.burst = static_cast<int>(burst);
        out.max = static_cast<int>(max);
        if (out.lifetime.x > out.lifetime.y)
            bad("lifetime", "the shortest life comes first");
        // An effect that never ends and never makes a particle is a mistake; so is an endless burst-only one.
        if (out.rate <= 0.0F && out.burst <= 0)
            bad("rate", "the effect makes no particles: give it a \"rate\" or a \"burst\"");
        return diagnostics.size() == before;
    }

    std::shared_ptr<const core::VfxEffect> LoadVfx(const std::filesystem::path &file, const std::string &name, std::vector<core::Diagnostic> &diagnostics)
    {
        std::ifstream in(file, std::ios::binary);
        if (!in)
        {
            diagnostics.push_back({core::Severity::Error, "vfx.read", name, "", "cannot open the file"});
            return nullptr;
        }
        core::VfxEffect effect;
        if (!ParseVfx(std::string(std::istreambuf_iterator<char>(in), {}), name, effect, diagnostics))
            return nullptr;
        return std::make_shared<const core::VfxEffect>(effect);
    }

    std::uint64_t VfxKey(world::Entity entity, std::uint32_t slot)
    {
        return (static_cast<std::uint64_t>(entity.slot.index) << 32) | (static_cast<std::uint64_t>(entity.slot.generation & 0xFFFFFFu) << 8) | (slot & 0xFFu);
    }

    std::vector<std::uint64_t> SyncVfx(const world::World &world, core::ParticleSystem &particles, const VfxFinder &effects)
    {
        std::vector<std::uint64_t> keys;
        world.ForEach([&](world::Entity entity) {
            const std::string *path = world.GetVfx(entity);
            const glm::mat4 *matrix = world.GetWorldMatrix(entity);
            if (path == nullptr || matrix == nullptr)
                return;
            particles.Attach(VfxKey(entity), effects(*path), glm::vec3((*matrix)[3]));
            keys.push_back(VfxKey(entity));
        });
        return keys;
    }
}
