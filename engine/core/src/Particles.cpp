#include "Particles.hpp"

#include "DrawList.hpp"

#include <glm/geometric.hpp>

#include <algorithm>

namespace umbra::core
{
    ParticleEmitter::ParticleEmitter(std::shared_ptr<const VfxEffect> effect, glm::vec3 position, std::uint32_t seed)
        : effect_(std::move(effect)), position_(position), random_(seed == 0 ? 1 : seed)
    {
    }

    float ParticleEmitter::Random()
    {
        // xorshift32: small, fast, the same on every compiler.
        random_ ^= random_ << 13;
        random_ ^= random_ >> 17;
        random_ ^= random_ << 5;
        return static_cast<float>(random_ & 0xFFFFFF) / static_cast<float>(0x1000000);
    }

    void ParticleEmitter::Spawn()
    {
        if (static_cast<int>(particles_.size()) >= effect_->max)
            return;
        const VfxEffect &e = *effect_;
        const auto signedRandom = [&] { return Random() * 2.0F - 1.0F; };
        Particle p;
        // 1. Somewhere in the start ball (a random point of the cube, pulled inside the ball).
        glm::vec3 offset{signedRandom(), signedRandom(), signedRandom()};
        if (glm::dot(offset, offset) > 1.0F)
            offset = glm::normalize(offset);
        p.position = position_ + offset * e.radius;
        // 2. The start velocity, a little different each time; how long it lives.
        p.velocity = e.velocity + e.spread * glm::vec3{signedRandom(), signedRandom(), signedRandom()};
        p.life = std::max(e.lifetime.x + (e.lifetime.y - e.lifetime.x) * Random(), 0.01F);
        particles_.push_back(p);
    }

    void ParticleEmitter::Update(float seconds)
    {
        const VfxEffect &e = *effect_;
        // 1. Move and age the living; drop the dead (Particles.md "One step").
        for (Particle &p : particles_)
        {
            p.velocity.y += e.gravity * seconds;
            p.velocity *= std::max(1.0F - e.drag * seconds, 0.0F);
            p.position += p.velocity * seconds;
            p.age += seconds;
        }
        std::erase_if(particles_, [](const Particle &p) { return p.age >= p.life; });

        // 2. New ones: the burst once, then `rate` per second while the emitter runs.
        if (!started_ && !stopped_)
            for (int i = 0; i < e.burst; ++i)
                Spawn();
        started_ = true;
        if (!stopped_ && !Expired())
        {
            owed_ += e.rate * seconds;
            for (; owed_ >= 1.0F; owed_ -= 1.0F)
                Spawn();
        }
        age_ += seconds;
    }

    void ParticleEmitter::Extract(std::vector<ParticleInstance> &out) const
    {
        const VfxEffect &e = *effect_;
        for (const Particle &p : particles_)
        {
            const float t = p.age / p.life; // 0 at birth, 1 at death
            ParticleInstance instance;
            instance.position = p.position;
            instance.size = e.size.x + (e.size.y - e.size.x) * t;
            instance.color = e.colorStart + (e.colorEnd - e.colorStart) * t;
            instance.ring = e.ring;
            instance.additive = e.additive ? 1.0F : 0.0F;
            out.push_back(instance);
        }
    }

    void ParticleSystem::Spawn(std::shared_ptr<const VfxEffect> effect, glm::vec3 position)
    {
        if (effect)
            loose_.emplace_back(std::move(effect), position, nextSeed_++);
    }

    void ParticleSystem::Attach(std::uint64_t key, const std::shared_ptr<const VfxEffect> &effect, glm::vec3 position)
    {
        if (!effect)
            return;
        const auto it = attached_.find(key);
        if (it != attached_.end() && it->second.effect == effect)
        {
            it->second.emitter.MoveTo(position);
            return;
        }
        if (it != attached_.end()) // another effect under this key: the old one finishes on its own
        {
            it->second.emitter.Stop();
            loose_.push_back(std::move(it->second.emitter));
            attached_.erase(it);
        }
        attached_.emplace(key, Attached{effect, ParticleEmitter(effect, position, nextSeed_++)});
    }

    void ParticleSystem::KeepOnly(const std::vector<std::uint64_t> &keep)
    {
        for (auto it = attached_.begin(); it != attached_.end();)
            if (std::find(keep.begin(), keep.end(), it->first) == keep.end())
            {
                it->second.emitter.Stop();
                loose_.push_back(std::move(it->second.emitter));
                it = attached_.erase(it);
            }
            else
                ++it;
    }

    void ParticleSystem::Clear()
    {
        attached_.clear();
        loose_.clear();
    }

    void ParticleSystem::Update(float seconds)
    {
        for (auto &[key, attached] : attached_)
            attached.emitter.Update(seconds);
        for (ParticleEmitter &emitter : loose_)
            emitter.Update(seconds);
        std::erase_if(loose_, [](const ParticleEmitter &emitter) { return emitter.Done(); });
    }

    std::size_t ParticleSystem::Count() const
    {
        std::size_t count = 0;
        for (const auto &[key, attached] : attached_)
            count += attached.emitter.Count();
        for (const ParticleEmitter &emitter : loose_)
            count += emitter.Count();
        return count;
    }

    void ParticleSystem::Extract(DrawList &drawList) const
    {
        std::vector<ParticleInstance> &out = drawList.particles;
        const std::size_t first = out.size();
        for (const auto &[key, attached] : attached_)
            attached.emitter.Extract(out);
        for (const ParticleEmitter &emitter : loose_)
            emitter.Extract(out);
        // Far to near, so covering (non-additive) particles hide what is behind them; if there are
        // too many, the farthest go.
        const glm::vec3 eye = drawList.camera.position;
        const auto distance = [&](const ParticleInstance &p) { return glm::dot(p.position - eye, p.position - eye); };
        std::sort(out.begin() + static_cast<std::ptrdiff_t>(first), out.end(),
                  [&](const ParticleInstance &a, const ParticleInstance &b) { return distance(a) > distance(b); });
        if (out.size() > maxDrawn)
            out.erase(out.begin(), out.end() - static_cast<std::ptrdiff_t>(maxDrawn));
    }
}
