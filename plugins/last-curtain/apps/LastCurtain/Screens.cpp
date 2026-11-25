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
    // 3. The screens are made for 1280 x 720 and drawn scaled to the window (UI.md "UI scale").
    ui_.SetDesignHeight(720.0F);
    ui_.SetAtlasUploader([this](const umbra::core::TextureData &atlas) {
        oldAtlases_.push_back(atlas_); // the renderer may still draw with it this frame
        atlas_ = renderer_.CreateTexture(atlas);
        return atlas_;
    });
}

void Screens::SetTextScale(float scale)
{
    std::vector<umbra::core::Diagnostic> diagnostics;
    if (!ready_ || ui_.TextScale() == scale || !ui_.SetTextScale(scale, diagnostics))
        return;
    oldAtlases_.push_back(atlas_);
    atlas_ = renderer_.CreateTexture(ui_.Atlas());
    ui_.SetAtlasTexture(atlas_);
}

Screens::~Screens()
{
    if (atlas_)
        renderer_.DestroyTexture(atlas_);
    for (const umbra::core::TextureHandle old : oldAtlases_)
        renderer_.DestroyTexture(old);
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
    if (umbra::assets::LoadImageFile(assets_ / path, image, error) && (image.clamp = true))
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
    ui.up = pressed({InputKey::Up, InputKey::PadUp}); // the d-pad moves the focus too, A confirms, B backs (P1)
    ui.down = pressed({InputKey::Down, InputKey::PadDown});
    ui.left = pressed({InputKey::Left, InputKey::PadLeft});
    ui.right = pressed({InputKey::Right, InputKey::PadRight});
    ui.confirm = pressed({InputKey::Enter, InputKey::PadA});
    ui.back = pressed({InputKey::Escape, InputKey::PadB});
    ui.text = input.Text(); // for a focused text field (UI.md "Text fields")
    ui.wheel = input.Wheel(); // a scroll box under the mouse (UI.md "Scroll")
    ui.padScroll = -input.Axis(3) * 0.25F; // the right stick scrolls the top screen's boxes
    // The left stick steps the focus like the d-pad: once each time it is pushed past half way.
    static glm::ivec2 was{0};
    const glm::ivec2 now{input.Axis(0) > 0.6F ? 1 : input.Axis(0) < -0.6F ? -1 : 0, input.Axis(1) > 0.6F ? 1 : input.Axis(1) < -0.6F ? -1 : 0};
    ui.left |= now.x < 0 && was.x >= 0, ui.right |= now.x > 0 && was.x <= 0, ui.up |= now.y < 0 && was.y >= 0, ui.down |= now.y > 0 && was.y <= 0;
    was = now;
    return ui;
}
