#include <Material.hpp>
#include <TextureTools.hpp>

#include "TestFiles.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <fstream>

using namespace umbra;

namespace
{
    // A horizontal ramp: dark on the left, bright on the right, so the left/right wrap is a hard jump.
    core::TextureData Ramp(std::uint32_t size)
    {
        core::TextureData t{size, size, {}};
        for (std::uint32_t y = 0; y < size; ++y)
            for (std::uint32_t x = 0; x < size; ++x)
            {
                const auto v = static_cast<std::uint8_t>(x * 255 / (size - 1));
                t.rgba.insert(t.rgba.end(), {v, v, v, 255});
            }
        return t;
    }

    // A pattern that repeats exactly: a sine wave with whole periods across the image.
    core::TextureData Waves(std::uint32_t size)
    {
        core::TextureData t{size, size, {}};
        for (std::uint32_t y = 0; y < size; ++y)
            for (std::uint32_t x = 0; x < size; ++x)
            {
                const double a = 2 * 3.14159265358979 * (2.0 * x / size + 1.0 * y / size);
                const auto v = static_cast<std::uint8_t>(std::lround(127.5 + 100 * std::sin(a)));
                t.rgba.insert(t.rgba.end(), {v, static_cast<std::uint8_t>(v / 2), 40, 255});
            }
        return t;
    }

    void WriteBytes(const std::filesystem::path &file, const std::vector<std::uint8_t> &bytes)
    {
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary).write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    bool HasCode(const std::vector<core::Diagnostic> &diagnostics, const std::string &code)
    {
        for (const auto &d : diagnostics)
            if (d.code == code)
                return true;
        return false;
    }
}

TEST_CASE("PNG encode and decode round-trip exactly", "[textures]")
{
    const core::TextureData ramp = Ramp(64);
    std::vector<std::uint8_t> png;
    REQUIRE(assets::EncodePng(ramp, png));
    core::TextureData back;
    std::string error;
    REQUIRE(assets::DecodeImage(png, back, error));
    CHECK(back.width == 64);
    CHECK(back.rgba == ramp.rgba);
    CHECK_FALSE(assets::DecodeImage({1, 2, 3}, back, error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("Resize keeps the picture and makes the asked size", "[textures]")
{
    const core::TextureData small = assets::Resize(Ramp(128), 64, 32);
    CHECK((small.width == 64 && small.height == 32));
    CHECK(small.rgba.size() == 64u * 32 * 4);
    CHECK(small.rgba[0] < 10);                  // still dark on the left
    CHECK(small.rgba[(63) * 4] > 245);          // still bright on the right
}

TEST_CASE("The seam score tells a hard seam from a repeating pattern (T61)", "[textures]")
{
    CHECK(assets::SeamScore(Waves(128)) < 1.5F);
    core::TextureData ramp = Ramp(128);
    const float before = assets::SeamScore(ramp);
    CHECK(before > assets::visibleSeam);
    // Making it seamless brings the wrap-around jump down to ordinary neighbour differences.
    assets::MakeSeamless(ramp);
    CHECK(assets::SeamScore(ramp) < assets::visibleSeam);
    CHECK(assets::SeamScore(ramp) < before / 10);
}

TEST_CASE("Material sizes are powers of two from 64 to 2048", "[textures]")
{
    CHECK(assets::GoodTextureSize(64, 2048));
    CHECK(assets::GoodTextureSize(1024, 512));
    CHECK_FALSE(assets::GoodTextureSize(1000, 1024));
    CHECK_FALSE(assets::GoodTextureSize(32, 32));
    CHECK_FALSE(assets::GoodTextureSize(4096, 4096));
}

TEST_CASE("Material files parse, write back and check their image (T63)", "[textures][material]")
{
    const std::filesystem::path root = test::TempDir("material");
    std::vector<std::uint8_t> png;
    REQUIRE(assets::EncodePng(Waves(128), png));
    WriteBytes(root / "textures/waves.png", png);
    REQUIRE(assets::EncodePng(Ramp(100), png));
    WriteBytes(root / "textures/odd.png", png);

    assets::Material material;
    material.baseColor = "textures/waves.png";
    material.worldProjection = true;
    material.metersPerRepeat = 1.5F;
    material.generated = assets::Generated{"comfyui", "sd_xl_base_1.0.safetensors", "CreativeML OpenRAIL++-M", "mossy stone", "text", 42, 25, "2026-11-08"};
    const std::string text = assets::MaterialText(material);

    std::vector<core::Diagnostic> diagnostics;
    assets::Material back;
    REQUIRE(assets::ParseMaterial(text, "m", back, diagnostics));
    CHECK(back.worldProjection);
    CHECK(back.metersPerRepeat == 1.5F);
    REQUIRE(back.generated);
    CHECK(back.generated->seed == 42);
    CHECK(assets::MaterialText(back) == text);
    CHECK(assets::CheckMaterialImage(root, back, "m", diagnostics));

    SECTION("bad fields")
    {
        CHECK_FALSE(assets::ParseMaterial(R"({"format":"umbra.material","version":1,"baseColor":"../x.png","projection":"sphere","metersPerRepeat":0})", "m", back, diagnostics));
        CHECK(HasCode(diagnostics, "asset.path.unsafe"));
        CHECK(HasCode(diagnostics, "material.projection"));
        CHECK(HasCode(diagnostics, "material.range"));
    }
    SECTION("missing or badly sized image")
    {
        back.baseColor = "textures/none.png";
        CHECK_FALSE(assets::CheckMaterialImage(root, back, "m", diagnostics));
        back.baseColor = "textures/odd.png";
        CHECK_FALSE(assets::CheckMaterialImage(root, back, "m", diagnostics));
        CHECK(diagnostics.back().message.find("100x100") != std::string::npos);
    }
}

TEST_CASE("A material may name a custom surface shader; bad names and missing files are refused (T86)", "[textures][material]")
{
    const std::filesystem::path root = test::TempDir("material-shader");
    std::vector<std::uint8_t> png;
    REQUIRE(assets::EncodePng(Waves(128), png));
    WriteBytes(root / "textures/waves.png", png);
    WriteBytes(root / "shaders/water.surface.hlsl", std::vector<std::uint8_t>{'/', '/'});

    std::vector<core::Diagnostic> diagnostics;
    assets::Material m;
    REQUIRE(assets::ParseMaterial(R"({"format":"umbra.material","version":1,"baseColor":"textures/waves.png","shader":"shaders/water.surface.hlsl"})", "m", m, diagnostics));
    CHECK(m.shader == "shaders/water.surface.hlsl");
    CHECK(assets::SurfaceName(m.shader) == "water");
    CHECK(assets::MaterialText(m).find("\"shader\": \"shaders/water.surface.hlsl\"") != std::string::npos);
    CHECK(assets::CheckMaterialImage(root, m, "m", diagnostics));

    m.shader = "shaders/lava.surface.hlsl"; // not there
    CHECK_FALSE(assets::CheckMaterialImage(root, m, "m", diagnostics));
    CHECK(HasCode(diagnostics, "material.shader"));
    diagnostics.clear();
    CHECK_FALSE(assets::ParseMaterial(R"({"format":"umbra.material","version":1,"baseColor":"textures/waves.png","shader":"shaders/water.hlsl"})", "m", m, diagnostics));
    CHECK(HasCode(diagnostics, "material.shader"));
    CHECK(assets::SurfaceName("shaders/bad name.surface.hlsl").empty());
    CHECK(assets::SurfaceName("water.hlsl").empty());
}

TEST_CASE("Model licenses are looked up, and risky ones warned about (T65)", "[textures][material]")
{
    CHECK(assets::ModelLicense("sd_xl_base_1.0.safetensors").commercial);
    CHECK(assets::ModelLicense("flux1-schnell-fp8.safetensors").name == "Apache-2.0");
    CHECK_FALSE(assets::ModelLicense("flux1-dev.safetensors").commercial);
    CHECK_FALSE(assets::ModelLicense("myMix_v3.safetensors").known);

    assets::Material material;
    std::vector<core::Diagnostic> diagnostics;
    assets::CheckMaterialLicense(material, "hand-made", diagnostics);
    CHECK(diagnostics.empty()); // not generated: nothing to say
    material.generated = assets::Generated{};
    material.generated->model = "sd_xl_base_1.0.safetensors";
    assets::CheckMaterialLicense(material, "ok", diagnostics);
    CHECK(diagnostics.empty());
    material.generated->model = "flux1-dev.safetensors";
    assets::CheckMaterialLicense(material, "nc", diagnostics);
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics[0].code == "material.license");
    CHECK(diagnostics[0].severity == core::Severity::Warning);
}
