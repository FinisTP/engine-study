#pragma once

#include <Handles.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/ext/vector_int2.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Particle effects (S9): fire, sparks, bubbles, dust. Math and file format in Particles.md.
namespace umbra::core
{
    struct DrawList;

    // One effect (`vfx/*.vfx.json`, parsed by scene::ParseVfx). Ranges are {min, max}.
    struct VfxEffect final
    {
        float rate = 20.0F;                 // particles per second while the emitter runs
        int burst = 0;                      // particles made at once when it starts
        float duration = 0.0F;              // seconds the emitter runs (0 = until removed)
        glm::vec2 lifetime{0.5F, 1.0F};     // seconds each particle lives
        float radius = 0.1F;                // they start inside a ball this big
        glm::vec3 velocity{0.0F, 1.0F, 0.0F}; // start velocity (m/s)...
        glm::vec3 spread{0.3F};             // ...plus or minus this, per axis
        float gravity = 0.0F;               // m/s^2 along Y (negative falls, positive rises)
        float drag = 0.0F;                  // 1/s: how fast the velocity dies away
        glm::vec2 size{0.2F, 0.05F};        // meters across: at birth, at death
        glm::vec4 colorStart{1.0F};         // RGBA at birth... (RGB may exceed 1: it glows, Post.md)
        glm::vec4 colorEnd{1.0F, 1.0F, 1.0F, 0.0F}; // ...and at death
        bool additive = true;               // adds light (fire, sparks) or covers (smoke, bubbles)
        float ring = 0.0F;                  // 0 = soft dot; towards 1 = hollow ring (bubbles)
        int max = 256;                      // particles alive at once
        // Pictures (VFX library): a texture sheet of cells x rows pictures, each particle one of them.
        bool textured = false;              // "texture" was given (drawn as soft dots if it can't load)
        std::string texture;                // asset path, as written
        TextureHandle image{};              // set by whoever loads the picture; none -> soft dots
        glm::ivec2 cells{1, 1};             // pictures across and down the sheet
        glm::vec2 rotation{0.0F, 0.0F};     // start angle range, degrees
        glm::vec2 spin{0.0F, 0.0F};         // turning speed range, degrees per second
        float stretch = 0.0F;               // longer along the velocity: seconds of travel added to its length
        bool line = false;                  // born along the line from the emitter to its end (LineTo)
    };

    // One particle as the renderers draw it: a camera-facing square (64 bytes, four float4).
    struct ParticleInstance final
    {
        glm::vec3 position{0.0F};
        float size = 0.0F;
        glm::vec4 color{1.0F};
        float ring = 0.0F;
        float additive = 1.0F;
        float angle = 0.0F;    // radians, turning the picture on screen
        float sheet = 0.0F;    // 0: a soft dot; else the batch's texture, cells across + 16 * cells down
        glm::vec3 stretch{0.0F}; // world vector the square is lengthened along (velocity * stretch)
        float cell = 0.0F;       // which picture of the sheet
    };

    // Particles drawn with one texture: instances [first, first + count) of DrawList::particles.
    struct ParticleBatch final
    {
        std::size_t first = 0, count = 0;
        TextureHandle texture{}; // none: soft dots
    };

    // One running copy of an effect at a place. The same seed and the same steps give the same
    // particles (tests, and later the deterministic world).
    class ParticleEmitter final
    {
    public:
        ParticleEmitter(std::shared_ptr<const VfxEffect> effect, glm::vec3 position, std::uint32_t seed);

        void MoveTo(glm::vec3 position) { position_ = position; }
        void LineTo(glm::vec3 end) { end_ = end; } // a line effect is born between the position and here
        void Stop() { stopped_ = true; } // no new particles; the living ones finish
        void Update(float seconds);
        [[nodiscard]] bool Done() const { return (stopped_ || Expired()) && particles_.empty(); }
        [[nodiscard]] std::size_t Count() const { return particles_.size(); }
        [[nodiscard]] const VfxEffect &Effect() const { return *effect_; }
        void Extract(std::vector<ParticleInstance> &out) const;

    private:
        struct Particle final
        {
            glm::vec3 position{0.0F};
            glm::vec3 velocity{0.0F};
            float age = 0.0F;
            float life = 1.0F;
            float angle = 0.0F, spin = 0.0F; // radians, radians per second
            float cell = 0.0F;
        };
        [[nodiscard]] bool Expired() const { return effect_->duration > 0.0F && age_ >= effect_->duration; }
        [[nodiscard]] float Random(); // 0..1
        void Spawn();

        std::shared_ptr<const VfxEffect> effect_;
        glm::vec3 position_, end_;
        std::uint32_t random_;
        std::vector<Particle> particles_;
        float age_ = 0.0F;
        float owed_ = 0.0F; // fractions of a particle carried to the next step
        bool started_ = false;
        bool stopped_ = false;
    };

    // Every emitter of a scene. One-shots remove themselves; attached ones (a torch's fire, poison
    // over a fighter) live under a key until detached.
    class ParticleSystem final
    {
    public:
        static constexpr std::size_t maxDrawn = 8192; // particles one frame draws (the nearest are kept)

        void Spawn(std::shared_ptr<const VfxEffect> effect, glm::vec3 position);
        // Keeps one emitter per key at `position`; a new effect under a key replaces the old one.
        void Attach(std::uint64_t key, const std::shared_ptr<const VfxEffect> &effect, glm::vec3 position);
        // Stops every attached emitter whose key is not in `keep` (sorted or not).
        void KeepOnly(const std::vector<std::uint64_t> &keep);
        void Clear();

        void Update(float seconds);
        // Appends this frame's particles to the draw list, far ones first (so near ones cover them).
        void Extract(DrawList &drawList) const;
        [[nodiscard]] std::size_t Count() const;

    private:
        struct Attached final
        {
            std::shared_ptr<const VfxEffect> effect;
            ParticleEmitter emitter;
        };
        std::map<std::uint64_t, Attached> attached_;
        std::vector<ParticleEmitter> loose_; // one-shots and stopped attached ones, until done
        std::uint32_t nextSeed_ = 1;
    };
}
