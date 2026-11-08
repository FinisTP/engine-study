#pragma once

#include <AssetFolders.hpp>
#include <Input.hpp>
#include <Renderer.hpp>
#include <Ui.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

// The game's UI system (engine/ui) set up for the game program: screens from the project and its
// plugins, the font atlas and pictures uploaded to the renderer, and keyboard/mouse/gamepad
// mapped to UI input. See GameFlow.md, "Screens".
class GameUi final
{
public:
    GameUi(umbra::render::Renderer &renderer, const umbra::assets::AssetFolders &assets);
    ~GameUi();
    GameUi(const GameUi &) = delete;
    GameUi &operator=(const GameUi &) = delete;

    [[nodiscard]] bool Ready() const { return ready_; }
    [[nodiscard]] const std::vector<std::string> &Problems() const { return problems_; }
    [[nodiscard]] umbra::ui::UiSystem &Screens() { return ui_; }
    // A picture from the assets, uploaded once (screens and the touch controls share them).
    [[nodiscard]] umbra::ui::ImageRef Image(const std::string &path);
    [[nodiscard]] umbra::core::TextureHandle AtlasTexture() const { return atlas_; }

    // The one game screen the RPG asks for ("" = none); the previous one closes.
    void ShowGameScreen(const std::string &screen);
    [[nodiscard]] const std::string &GameScreen() const { return gameScreen_; }

    // Where a point of the world is on the screen, in pixels (nullopt when behind the camera).
    [[nodiscard]] static std::optional<glm::vec2> ScreenPoint(const umbra::core::Camera &camera, glm::vec2 pixels, glm::vec3 world);

    // Input for this frame. `windowSize` and `pixels` turn window mouse coordinates into pixels.
    [[nodiscard]] static umbra::ui::UiInput MapInput(const umbra::platform::Input &input, glm::vec2 windowSize, glm::vec2 pixels);

private:
    umbra::render::Renderer &renderer_;
    umbra::assets::AssetFolders assets_;
    umbra::ui::UiSystem ui_;
    bool ready_ = false;
    std::vector<std::string> problems_;
    umbra::core::TextureHandle atlas_;
    std::map<std::string, umbra::ui::ImageRef> images_;
    std::string gameScreen_;
};
