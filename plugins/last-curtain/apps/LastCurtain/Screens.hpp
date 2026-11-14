#pragma once

#include <AssetFolders.hpp>
#include <Input.hpp>
#include <Renderer.hpp>
#include <Ui.hpp>

#include <map>
#include <string>
#include <vector>

// The program's screens (engine/ui): the skin and ui/*.ui.json from its assets, the font atlas and
// pictures uploaded to the renderer, and keyboard and mouse mapped to UI input. Adapted from the
// Umbra game's GameUi.
class Screens final
{
public:
    Screens(umbra::render::Renderer &renderer, const umbra::assets::AssetFolders &assets);
    ~Screens();
    Screens(const Screens &) = delete;
    Screens &operator=(const Screens &) = delete;

    [[nodiscard]] bool Ready() const { return ready_; }
    [[nodiscard]] const std::vector<std::string> &Problems() const { return problems_; }
    [[nodiscard]] umbra::ui::UiSystem &Ui() { return ui_; }
    [[nodiscard]] umbra::ui::ImageRef Image(const std::string &path);

    // Input for this frame; window mouse coordinates become render pixels.
    [[nodiscard]] static umbra::ui::UiInput MapInput(const umbra::platform::Input &input, glm::vec2 windowSize, glm::vec2 pixels);

private:
    umbra::render::Renderer &renderer_;
    umbra::assets::AssetFolders assets_;
    umbra::ui::UiSystem ui_;
    bool ready_ = false;
    std::vector<std::string> problems_;
    umbra::core::TextureHandle atlas_;
    std::map<std::string, umbra::ui::ImageRef> images_;
};
