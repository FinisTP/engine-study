#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <DrawList.hpp>
#include <Particles.hpp>

#include <cstring>
#include <set>

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
    CHECK(sizeof(ParticleInstance) == 64); // the renderers' instance layout
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

TEST_CASE("Textured particles: a start angle and spin, a cell of a sheet, a stretch along the velocity (VFX library)", "[particles]")
{
    VfxEffect e;
    e.burst = 12, e.rate = 0.0F, e.duration = 1.0F, e.lifetime = {2.0F, 2.0F};
    e.velocity = {1.0F, 0.0F, 0.0F}, e.spread = glm::vec3{0.0F};
    e.textured = true, e.image = TextureHandle{1, 1}, e.cells = {2, 2}, e.rotation = {0.0F, 0.0F}, e.spin = {90.0F, 90.0F}, e.stretch = 0.5F;
    ParticleEmitter emitter(std::make_shared<const VfxEffect>(e), {0, 0, 0}, 3);
    emitter.Update(0.0F);
    emitter.Update(1.0F);
    std::vector<ParticleInstance> out;
    emitter.Extract(out);
    REQUIRE(out.size() == 12);
    std::set<int> cells;
    for (const ParticleInstance &p : out)
    {
        CHECK(p.sheet == 2.0F + 16.0F * 2.0F); // 2 across, 2 down
        CHECK(p.angle == Approx(90.0F * 3.14159265F / 180.0F).margin(1e-4)); // a second at 90 degrees a second
        CHECK(p.stretch.x == Approx(0.5F)); // velocity (1, 0, 0) times the stretch
        CHECK(p.cell >= 0.0F);
        CHECK(p.cell < 4.0F);
        cells.insert(static_cast<int>(p.cell));
    }
    CHECK(cells.size() > 1); // each particle picks its own cell
}

TEST_CASE("A line emitter spawns between its two ends (VFX library)", "[particles]")
{
    VfxEffect e;
    e.burst = 40, e.rate = 0.0F, e.duration = 0.1F, e.lifetime = {1.0F, 1.0F}, e.radius = 0.0F;
    e.velocity = glm::vec3{0.0F}, e.spread = glm::vec3{0.0F}, e.line = true;
    ParticleEmitter emitter(std::make_shared<const VfxEffect>(e), {0, 0, 0}, 5);
    emitter.LineTo({10, 0, 0});
    emitter.Update(0.0F);
    std::vector<ParticleInstance> out;
    emitter.Extract(out);
    REQUIRE(out.size() == 40);
    float lo = 99.0F, hi = -99.0F;
    for (const ParticleInstance &p : out)
        lo = std::min(lo, p.position.x), hi = std::max(hi, p.position.x);
    CHECK(lo >= 0.0F);
    CHECK(hi <= 10.0F);
    CHECK(hi - lo > 5.0F);
}

TEST_CASE("Particles batch by texture for the renderers, in draw order (VFX library)", "[particles]")
{
    VfxEffect a, b;
    a.burst = b.burst = 3, a.rate = b.rate = 0.0F, a.duration = b.duration = 0.1F;
    a.image = TextureHandle{1, 1}, b.image = TextureHandle{2, 1};
    a.textured = b.textured = true;
    ParticleSystem system;
    system.Spawn(std::make_shared<const VfxEffect>(a), {0, 0, -5});
    system.Spawn(std::make_shared<const VfxEffect>(b), {0, 0, -1});
    system.Update(0.0F);
    DrawList list;
    system.Extract(list);
    REQUIRE(list.particles.size() == 6);
    std::size_t covered = 0;
    for (const ParticleBatch &batch : list.particleBatches)
    {
        CHECK(batch.first == covered);
        covered += batch.count;
    }
    CHECK(covered == 6);
    CHECK(list.particleBatches.size() == 2);
}
