#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Post.hpp>

using namespace umbra::core;
using Catch::Approx;

TEST_CASE("The tonemap keeps middle gray, rolls off highlights and never passes white (T85)", "[post]")
{
    CHECK(Tonemap(glm::vec3(0.18F)).r == Approx(0.205F).margin(0.01F));
    CHECK(Tonemap(glm::vec3(0.0F)).r == 0.0F);
    CHECK(Tonemap(glm::vec3(4.0F)).r > Tonemap(glm::vec3(2.0F)).r); // still brighter...
    CHECK(Tonemap(glm::vec3(1000.0F)).r <= 1.0F);                      // ...but never past white
    CHECK(Tonemap(glm::vec3(-1.0F)).r == 0.0F);
}

TEST_CASE("A neutral grade changes nothing; temperature, saturation and contrast do what they say (T85)", "[post]")
{
    const TextureData neutral = BakeGradeLut(PostSettings{});
    REQUIRE(neutral.width == 256);
    REQUIRE(neutral.height == 16);
    for (const glm::vec3 c : {glm::vec3(0.1F, 0.4F, 0.8F), glm::vec3(0.5F), glm::vec3(0.9F, 0.2F, 0.05F)})
    {
        const glm::vec3 out = ApplyGrade(neutral, c);
        CHECK(out.r == Approx(c.r).margin(0.02F));
        CHECK(out.g == Approx(c.g).margin(0.02F));
        CHECK(out.b == Approx(c.b).margin(0.02F));
    }
    PostSettings warm;
    warm.temperature = 1.0F;
    const glm::vec3 w = ApplyGrade(BakeGradeLut(warm), glm::vec3(0.5F));
    CHECK(w.r > w.b + 0.1F);
    PostSettings gray;
    gray.saturation = 0.0F;
    const glm::vec3 g = ApplyGrade(BakeGradeLut(gray), glm::vec3(0.8F, 0.1F, 0.1F));
    CHECK(g.r == Approx(g.g).margin(0.02F));
    PostSettings punchy;
    punchy.contrast = 1.5F;
    const TextureData p = BakeGradeLut(punchy);
    CHECK(ApplyGrade(p, glm::vec3(0.05F)).r < 0.04F); // darks darker
    CHECK(ApplyGrade(p, glm::vec3(0.6F)).r > 0.65F);  // lights lighter
}

TEST_CASE("A LUT image is used when it is 256x16 and refused otherwise (T85)", "[post]")
{
    TextureData inverted = BakeGradeLut(PostSettings{});
    for (std::size_t i = 0; i < inverted.rgba.size(); i += 4)
        for (int k = 0; k < 3; ++k)
            inverted.rgba[i + k] = static_cast<std::uint8_t>(255 - inverted.rgba[i + k]);
    CHECK(CheckLutImage(inverted).empty());
    const glm::vec3 out = ApplyGrade(BakeGradeLut(PostSettings{}, &inverted), glm::vec3(1.0F));
    CHECK(out.r < 0.02F); // white became black
    const TextureData wrong{64, 64, std::vector<std::uint8_t>(64 * 64 * 4, 0)};
    CHECK_FALSE(CheckLutImage(wrong).empty());
    CHECK(ApplyGrade(BakeGradeLut(PostSettings{}, &wrong), glm::vec3(1.0F)).r > 0.98F); // ignored
}

TEST_CASE("Exposure, bloom and vignette in the whole pixel; quality levels (T85)", "[post]")
{
    PostSettings s;
    s.vignette = 0.0F;
    const TextureData lut = BakeGradeLut(s);
    const glm::vec3 base = PostPixel(s, lut, glm::vec3(0.18F), glm::vec3(0.0F), 0.5F, 0.5F);
    s.exposure = 1.0F;
    CHECK(PostPixel(s, lut, glm::vec3(0.18F), glm::vec3(0.0F), 0.5F, 0.5F).r > base.r + 0.05F);
    s.exposure = 0.0F;
    CHECK(PostPixel(s, lut, glm::vec3(0.18F), glm::vec3(1.0F), 0.5F, 0.5F).r > base.r + 0.1F); // bloom adds light
    s.vignette = 1.0F;
    CHECK(PostPixel(s, lut, glm::vec3(0.18F), glm::vec3(0.0F), 0.5F, 0.5F).r == Approx(base.r)); // middle untouched
    CHECK(PostPixel(s, lut, glm::vec3(0.18F), glm::vec3(0.0F), 0.0F, 0.0F).r < 0.05F);           // corner dark

    const PostBlock b = PackPost(PostSettings{}, 1920, 1080);
    CHECK(b.exposure.x == 1.0F);
    CHECK(b.sizes.x == Approx(1.0F / 1920.0F));
    CHECK(BloomStartDivisor(PostQuality::Reduced) == 2 * BloomStartDivisor(PostQuality::Full));
    CHECK(BloomLevels(PostQuality::Reduced) < BloomLevels(PostQuality::Full));
}

TEST_CASE("The analog screen: off by default, packed for the shader, the CRT bulges the picture (S45)", "[post]")
{
    // 1. Off unless a strength is set.
    PostSettings settings;
    CHECK_FALSE(AnalogOn(settings));
    settings.vhs = 0.5F;
    CHECK(AnalogOn(settings));
    settings.crt = 0.8F;
    // 2. What the analog pass receives: strengths (clamped), seconds, the screen's size.
    settings.vhs = 3.0F;
    const PostBlock block = PackAnalog(settings, 1280, 720, 12.5F);
    CHECK(block.exposure.x == Approx(0.8F));
    CHECK(block.exposure.y == Approx(1.0F));
    CHECK(block.exposure.z == Approx(12.5F));
    CHECK(block.sizes.x == Approx(1.0F / 1280.0F));
    CHECK(block.sizes.w == Approx(720.0F));
    CHECK(PackAnalog(settings, 1, 1, 5000.25F).exposure.z == Approx(0.25F)); // seconds wrap at 1000 so the noise stays precise
    // 3. The bulge: the centre stays, the edges are pushed out (so the corners fall off the glass).
    CHECK(CrtWarp({0.5F, 0.5F}, 1.0F).x == Approx(0.5F));
    CHECK(CrtWarp({0.9F, 0.5F}, 1.0F).x > 0.9F);
    CHECK(CrtWarp({0.1F, 0.1F}, 1.0F).x < 0.1F);
    CHECK(CrtWarp({0.9F, 0.5F}, 0.0F).x == Approx(0.9F)); // no CRT, no bulge
    const glm::vec2 corner = CrtWarp({0.0F, 0.0F}, 1.0F);
    CHECK((corner.x < 0.0F && corner.y < 0.0F)); // outside the glass: drawn black
}

TEST_CASE("A tracking tear is packed for the composite only, kept within 0..1 (Post.md 6b)", "[post]")
{
    umbra::core::PostSettings s;
    CHECK(umbra::core::PackPost(s, 100, 100).sizes.w == 0.0F);
    s.tear = 0.4F;
    CHECK(umbra::core::PackPost(s, 100, 100).sizes.w == 0.4F);
    s.tear = 3.0F;
    CHECK(umbra::core::PackPost(s, 100, 100).sizes.w == 1.0F);
}
