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
