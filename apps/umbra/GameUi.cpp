#include "GameUi.hpp"

#include <Log.hpp>
#include <TextureTools.hpp>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

GameUi::GameUi(umbra::render::Renderer &renderer, const umbra::assets::AssetFolders &assets) : renderer_(renderer), assets_(assets)
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
    ui_.Open("hud");
}

umbra::ui::ImageRef GameUi::Image(const std::string &path)
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

GameUi::~GameUi()
{
    if (atlas_)
        renderer_.DestroyTexture(atlas_);
    for (const auto &[path, image] : images_)
        if (image.texture)
            renderer_.DestroyTexture(image.texture);
}

void GameUi::ShowGameScreen(const std::string &screen)
{
    if (screen == gameScreen_)
        return;
    if (!gameScreen_.empty())
        ui_.Close(gameScreen_);
    gameScreen_ = screen;
    if (!screen.empty())
    {
        ui_.Open(screen);
        ui_.ResetFocus(screen);
    }
}

std::optional<glm::vec2> GameUi::ScreenPoint(const umbra::core::Camera &camera, glm::vec2 pixels, glm::vec3 world)
{
    // 1. World -> clip space with the camera the scene is drawn with.
    const glm::mat4 view = glm::lookAtRH(camera.position, camera.target, glm::vec3{0.0F, 1.0F, 0.0F});
    const glm::mat4 projection = glm::perspectiveRH_NO(glm::radians(camera.fovYDegrees), pixels.x / std::max(pixels.y, 1.0F), camera.nearPlane, camera.farPlane);
    const glm::vec4 clip = projection * view * glm::vec4(world, 1.0F);
    if (clip.w <= 0.0F)
        return std::nullopt;
    // 2. Divide by w (normalized -1..1), then to pixels; screen y grows downward.
    const glm::vec2 ndc = glm::vec2(clip) / clip.w;
    return glm::vec2{(ndc.x * 0.5F + 0.5F) * pixels.x, (0.5F - ndc.y * 0.5F) * pixels.y};
}

umbra::ui::UiInput GameUi::MapInput(const umbra::platform::Input &input, glm::vec2 windowSize, glm::vec2 pixels)
{
    using umbra::platform::InputKey;
    const auto pressed = [&](std::initializer_list<InputKey> keys) {
        for (const InputKey key : keys)
            if (input.IsKeyPressed(key))
                return true;
        return false;
    };
    umbra::ui::UiInput ui;
    // Window coordinates -> render pixels (they differ on high-DPI screens).
    const glm::vec2 scale = windowSize.x > 0.0F && windowSize.y > 0.0F ? pixels / windowSize : glm::vec2{1.0F};
    ui.mouse = glm::vec2{input.GetMouseX(), input.GetMouseY()} * scale;
    ui.mouseMoved = input.MouseMoved();
    ui.click = input.IsMouseButtonPressed(0);
    ui.up = pressed({InputKey::W, InputKey::Up, InputKey::PadUp});
    ui.down = pressed({InputKey::S, InputKey::Down, InputKey::PadDown});
    ui.left = pressed({InputKey::A, InputKey::Left, InputKey::PadLeft});
    ui.right = pressed({InputKey::D, InputKey::Right, InputKey::PadRight});
    ui.confirm = pressed({InputKey::Enter, InputKey::PadA});
    ui.back = pressed({InputKey::Escape, InputKey::PadB});
    ui.text = input.Text(); // for a focused text field (UI.md "Text fields")
    return ui;
}
