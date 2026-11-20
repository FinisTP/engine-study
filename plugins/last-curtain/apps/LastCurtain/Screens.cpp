#include "Screens.hpp"

#include <Log.hpp>
#include <TextureTools.hpp>

Screens::Screens(umbra::render::Renderer &renderer, const umbra::assets::AssetFolders &assets) : renderer_(renderer), assets_(assets)
{
    // 1. Skin, fonts and screens; problems go to the log and the warnings in the corner.
    std::vector<umbra::core::Diagnostic> diagnostics;
    ready_ = ui_.Load(assets_, diagnostics);
    for (const umbra::core::Diagnostic &d : diagnostics)
    {
        umbra::diagnostics::Error(umbra::core::Format(d));
        problems_.push_back("Warning: UI " + d.file + ": " + d.message);
    }
    if (!ready_)
        return;
    // 2. The font atlas, and pictures uploaded the first time a screen draws them.
    atlas_ = renderer_.CreateTexture(ui_.Atlas());
    ui_.SetAtlasTexture(atlas_);
    ui_.SetImageLoader([this](const std::string &path) { return Image(path); });
}

Screens::~Screens()
{
    if (atlas_)
        renderer_.DestroyTexture(atlas_);
    for (const auto &[path, image] : images_)
        if (image.texture)
            renderer_.DestroyTexture(image.texture);
}

umbra::ui::ImageRef Screens::Image(const std::string &path)
{
    if (const auto it = images_.find(path); it != images_.end())
        return it->second;
    umbra::core::TextureData image;
    std::string error;
    umbra::ui::ImageRef ref;
    if (umbra::assets::LoadImageFile(assets_ / path, image, error))
        ref = {renderer_.CreateTexture(image), {static_cast<float>(image.width), static_cast<float>(image.height)}};
    else
        umbra::diagnostics::Error("UI picture " + path + ": " + error);
    images_[path] = ref; // a missing picture is asked for once
    return ref;
}

umbra::ui::UiInput Screens::MapInput(const umbra::platform::Input &input, glm::vec2 windowSize, glm::vec2 pixels)
{
    using umbra::platform::InputKey;
    const auto pressed = [&](std::initializer_list<InputKey> keys) {
        for (const InputKey key : keys)
            if (input.IsKeyPressed(key))
                return true;
        return false;
    };
    umbra::ui::UiInput ui;
    const glm::vec2 scale = windowSize.x > 0.0F && windowSize.y > 0.0F ? pixels / windowSize : glm::vec2{1.0F};
    ui.mouse = glm::vec2{input.GetMouseX(), input.GetMouseY()} * scale;
    ui.mouseMoved = input.MouseMoved();
    ui.click = input.IsMouseButtonPressed(0);
    ui.held = input.IsMouseButtonHeld(0);
    ui.up = pressed({InputKey::Up});
    ui.down = pressed({InputKey::Down});
    ui.left = pressed({InputKey::Left});
    ui.right = pressed({InputKey::Right});
    ui.confirm = pressed({InputKey::Enter});
    ui.back = pressed({InputKey::Escape});
    return ui;
}
