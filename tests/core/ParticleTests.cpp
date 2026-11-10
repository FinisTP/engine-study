#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <DrawList.hpp>
#include <Particles.hpp>

#include <cstring>

using namespace umbra::core;
using Catch::Approx;

namespace
{
    std::shared_ptr<const VfxEffect> Fire()
    {
        VfxEffect e;
        e.rate = 40.0F;
        e.burst = 5;
        e.lifetime = {0.5F, 1.0F};
        e.max = 64;
        return std::make_shared<const VfxEffect>(e);
    }

    std::vector<ParticleInstance> Run(std::uint32_t seed, int steps, float step)
    {
        ParticleEmitter emitter(Fire(), {1, 2, 3}, seed);
        for (int i = 0; i < steps; ++i)
            emitter.Update(step);
        std::vector<ParticleInstance> out;
        emitter.Extract(out);
        return out;
    }
}

TEST_CASE("The same seed and steps give the same particles; another seed differs (T89)", "[particles]")
{
    const auto a = Run(7, 30, 1.0F / 60.0F);
    const auto b = Run(7, 30, 1.0F / 60.0F);
    const auto c = Run(8, 30, 1.0F / 60.0F);
    REQUIRE(a.size() == b.size());
    REQUIRE(a.size() == 25); // the burst of 5, then 40 a second for half a second
    CHECK(std::memcmp(a.data(), b.data(), a.size() * sizeof(ParticleInstance)) == 0);
    REQUIRE(c.size() == a.size());
    CHECK(std::memcmp(a.data(), c.data(), a.size() * sizeof(ParticleInstance)) != 0);
    CHECK(sizeof(ParticleInstance) == 48); // the renderers' instance layout
}

TEST_CASE("Particles stay under the effect's limit, fade over life and die (T89)", "[particles]")
{
    // 1. Never more alive than `max`.
    ParticleEmitter emitter(Fire(), {0, 0, 0}, 1);
    for (int i = 0; i < 600; ++i)
    {
        emitter.Update(1.0F / 60.0F);
        REQUIRE(emitter.Count() <= 64);
    }
    CHECK(emitter.Count() > 20);

    // 2. Size and colour go from the birth values towards the death values.
    std::vector<ParticleInstance> out;
    emitter.Extract(out);
    for (const ParticleInstance &p : out)
    {
        CHECK(p.size <= 0.2F);
        CHECK(p.size >= 0.05F);
        CHECK(p.color.a <= 1.0F);
        CHECK(p.additive == 1.0F);
    }

    // 3. Stopped: no new ones, the living ones finish, then the emitter is done.
    emitter.Stop();
    CHECK_FALSE(emitter.Done());
    for (int i = 0; i < 70; ++i)
        emitter.Update(1.0F / 60.0F);
    CHECK(emitter.Done());
}

TEST_CASE("One-shots end by themselves; attached effects live until detached (T89)", "[particles]")
{
    VfxEffect burst;
    burst.rate = 0.0F;
    burst.burst = 12;
    burst.duration = 0.1F;
    burst.lifetime = {0.2F, 0.3F};
    const auto spark = std::make_shared<const VfxEffect>(burst);

    ParticleSystem particles;
    particles.Spawn(spark, {0, 0, 0});
    particles.Attach(42, Fire(), {5, 0, 0});
    particles.Update(0.05F);
    CHECK(particles.Count() == 12 + 5 + 2);

    // 1. The burst is gone after its particles' lives; the attached fire keeps burning.
    for (int i = 0; i < 30; ++i)
        particles.Update(0.05F);
    const std::size_t burning = particles.Count();
    CHECK(burning > 10);
    CHECK(burning <= 64);

    // 2. Attaching the same key again moves the emitter instead of starting a second one.
    particles.Attach(42, Fire(), {6, 0, 0});
    particles.Update(0.05F);
    CHECK(particles.Count() <= 64);

    // 3. Far particles come first in the draw list (near ones cover them).
    DrawList drawList;
    drawList.camera.position = {-10, 0, 0};
    particles.Extract(drawList);
    REQUIRE(drawList.particles.size() == particles.Count());
    for (std::size_t i = 1; i < drawList.particles.size(); ++i)
        CHECK(drawList.particles[i - 1].position.x >= drawList.particles[i].position.x - 2.0F);

    // 4. Detached: it fades out and nothing is left.
    particles.KeepOnly({});
    for (int i = 0; i < 30; ++i)
        particles.Update(0.05F);
    CHECK(particles.Count() == 0);
}
