// UmbraTool ui-preview: draws game screens (engine/ui) into a PNG without a window, with sample
// data, so a person or an AI agent can check a screen file's layout.

#include <ToolCommand.hpp>

#include <Raster.hpp>
#include <TextureTools.hpp>
#include <Ui.hpp>

#include <fstream>
#include <map>

namespace
{
    using umbra::core::Diagnostic;
    using umbra::core::Severity;
    using umbra::tool::Arguments;

    int UiPreview(const Arguments &args)
    {
        if (args.positional.empty() || !args.Has("out") || !args.Has("assets"))
            return umbra::tool::Usage();
        std::vector<Diagnostic> diagnostics;
        const umbra::assets::AssetFolders assets = args.Assets();
        const auto fail = [&](const std::string &file, const std::string &message) {
            diagnostics.push_back({Severity::Error, "ui.preview", file, "", message});
            return umbra::tool::Report(diagnostics, 1, args.json);
        };

        // 1. Skin, fonts, screens; the data (ui/sample.json unless --data is given).
        umbra::ui::UiSystem ui;
        if (!ui.Load(assets, diagnostics))
            return umbra::tool::Report(diagnostics, 1, args.json);
        const std::filesystem::path dataFile = args.Has("data") ? args.Path("data") : assets / "ui/sample.json";
        std::ifstream in(dataFile);
        const nlohmann::json data = nlohmann::json::parse(in, nullptr, false);
        if (!in || !data.is_object())
            return fail(dataFile.generic_string(), "no data: a JSON object is needed (--data, or ui/sample.json)");

        // 2. Textures by handle: 1 = the font atlas, then pictures as screens ask for them.
        std::map<std::uint32_t, umbra::core::TextureData> textures{{1, ui.Atlas()}};
        ui.SetAtlasTexture(umbra::core::TextureHandle{1, 1});
        ui.SetImageLoader([&](const std::string &path) {
            umbra::core::TextureData image;
            std::string error;
            if (!umbra::assets::LoadImageFile(assets / path, image, error))
            {
                diagnostics.push_back({Severity::Warning, "ui.preview", path, "", error});
                return umbra::ui::ImageRef{};
            }
            const auto handle = static_cast<std::uint32_t>(textures.size() + 1);
            const glm::vec2 size{static_cast<float>(image.width), static_cast<float>(image.height)};
            textures[handle] = std::move(image);
            return umbra::ui::ImageRef{umbra::core::TextureHandle{handle, 1}, size};
        });

        // 3. Open the screens in order (the last on top), lay out twice (pictures load in the
        // first pass, lists settle), and draw the last pass.
        for (const std::string &screen : args.positional)
            if (!ui.Open(screen))
                return fail(screen, "no screen named '" + screen + "' (ui/" + screen + ".ui.json)");
        const int width = args.Int("width", 1280), height = args.Int("height", 720);
        umbra::core::UiDrawList list;
        for (int pass = 0; pass < 2; ++pass)
        {
            list.Clear();
            (void)ui.Update(data, {static_cast<float>(width), static_cast<float>(height)}, {}, list);
        }
        const umbra::core::TextureData image = umbra::ui::Rasterize(list, width, height, [&](umbra::core::TextureHandle handle) {
            const auto it = textures.find(handle.index);
            return it == textures.end() ? nullptr : &it->second;
        });
        std::vector<std::uint8_t> png;
        std::ofstream out(args.Path("out"), std::ios::binary);
        if (!umbra::assets::EncodePng(image, png) || !out.write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size())))
            return fail(args.Text("out"), "could not write the picture");
        return umbra::tool::Report(diagnostics, 1, args.json);
    }
}

UMBRA_TOOL_COMMAND(UiPreview, "ui-preview",
                   "ui-preview <screen>... --assets <folder> --out <png> [--data <json>] [--width 1280] [--height 720] [--json]", UiPreview);
