#pragma once

#include <AssetFolders.hpp>
#include <Font.hpp>
#include <Painter.hpp>
#include <Screen.hpp>
#include <Skin.hpp>

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace umbra::ui
{
    // A picture the UI can draw: a renderer texture and its size in pixels.
    struct ImageRef final
    {
        core::TextureHandle texture;
        glm::vec2 size{0.0F};
    };

    // One frame of UI input, already mapped from keyboard, mouse and gamepad by the game.
    struct UiInput final
    {
        glm::vec2 mouse{-1.0F};
        bool mouseMoved = false, click = false, held = false; // held: the button is down (a pressed look)
        bool up = false, down = false, left = false, right = false; // focus moves
        bool confirm = false, back = false;                          // Enter / A, Esc / B
        std::string text; // typed this frame (UTF-8) for a focused text field; '\b' erases a letter
    };

    // What the player chose: a button's or list entry's action, with its argument.
    struct UiAction final
    {
        std::string screen, action, arg;
    };

    // The game UI (UI.md): a skin, fonts and screens from assets/ui, a stack of open screens,
    // and each frame: input -> focus and actions, then layout and drawing from the game's data.
    class UiSystem final
    {
    public:
        // Reads ui/skin.json, its fonts and every ui/*.ui.json (project over plugins).
        bool Load(const assets::AssetFolders &assets, std::vector<core::Diagnostic> &diagnostics);
        // The renderer texture made from Atlas(), and a way to load pictures ("image" fields, skins).
        void SetAtlasTexture(core::TextureHandle texture) { atlasTexture_ = texture; }
        void SetImageLoader(std::function<ImageRef(const std::string &path)> loader) { images_ = std::move(loader); }
        [[nodiscard]] const core::TextureData &Atlas() const { return fonts_.Image(); }

        // The screen stack: the last opened is on top. Opening an open screen moves it to the top.
        bool Open(const std::string &screen);
        void Close(const std::string &screen);
        [[nodiscard]] bool IsOpen(const std::string &screen) const;
        [[nodiscard]] const std::vector<std::string> &OpenScreens() const { return stack_; } // bottom first
        [[nodiscard]] bool TakesInput() const; // a modal screen with something to choose is open
        [[nodiscard]] bool Has(const std::string &screen) const { return screens_.contains(screen); }
        // Focus the first choosable widget of a screen (e.g. after its content changed).
        void ResetFocus(const std::string &screen);
        // Text fields (UI.md "Text fields"): the text typed into one, by screen and field id.
        [[nodiscard]] std::string FieldText(const std::string &screen, const std::string &id) const;
        void SetFieldText(const std::string &screen, const std::string &id, std::string text) { fields_[screen + "#" + id] = std::move(text); }
        // A text field has the focus: the game turns the window's text input on (Window::SetTextInput).
        [[nodiscard]] bool WantsText() const;

        // One frame: input first (with last frame's layout), then every open screen is laid out
        // and drawn bottom to top from `data`. Returns the actions chosen this frame.
        std::vector<UiAction> Update(const nlohmann::json &data, glm::vec2 viewport, const UiInput &input, core::UiDrawList &out);

        [[nodiscard]] const FontAtlas &Fonts() const { return fonts_; }
        // Large text (UI.md "Text scale"): every font rebuilt at its size times scale (0.5..2); screens lay
        // out from the new sizes next frame. The game uploads Atlas() again after a change.
        bool SetTextScale(float scale, std::vector<core::Diagnostic> &diagnostics);
        [[nodiscard]] float TextScale() const { return textScale_; }
        // UI scale (UI.md "UI scale"): screens are laid out for a window `height` rows tall and drawn
        // scaled to the real one, fonts rebuilt at the real size; 0 draws them in pixels. The uploader
        // makes the renderer's texture whenever the atlas is rebuilt for a new size.
        void SetDesignHeight(float height) { designHeight_ = height; }
        void SetAtlasUploader(std::function<core::TextureHandle(const core::TextureData &)> uploader) { uploader_ = std::move(uploader); }
        [[nodiscard]] float UiScale() const { return uiScale_; }
        [[nodiscard]] const std::map<std::string, Screen> &Screens() const { return screens_; }
        void SetScreen(Screen screen) { screens_[screen.name] = std::move(screen); } // e.g. edited in the editor
        [[nodiscard]] const Skin &GetSkin() const { return skin_; }
        // The `tip` of the topmost widget under the mouse in the last Update ("" none), disabled ones too.
        [[nodiscard]] const std::string &Tip() const { return tip_; }
        // How large a screen's n-th choice is drawn now (1 at rest; eased toward its style's scale).
        [[nodiscard]] float ChoiceScale(const std::string &screen, int choice) const;

    private:
        struct Choice final // something the player can choose on the input screen
        {
            Rect rect;
            std::string action, arg;
            std::string field; // a text field's id ("" for other choices)
        };
        struct Context; // one screen's layout pass
        glm::vec2 Measure(const Node &node, Context &c, float width);
        void Place(const Node &node, Rect rect, Context &c);
        void DrawBox(const Style &style, Rect rect, Context &c);

        FontAtlas fonts_;
        std::vector<FontSource> sources_; // the skin's fonts at scale 1
        float textScale_ = 1.0F, uiScale_ = 1.0F, designHeight_ = 0.0F;
        std::function<core::TextureHandle(const core::TextureData &)> uploader_;
        bool BuildFonts(std::vector<core::Diagnostic> &diagnostics); // at text scale x UI scale
        [[nodiscard]] glm::vec2 TextSize(const Face &face, std::string_view text, float wrap = 0.0F) const; // in layout units
        Skin skin_;
        std::map<std::string, Screen> screens_;
        std::vector<std::string> stack_;
        std::map<std::string, int> focus_;          // per screen
        std::map<std::string, std::vector<Choice>> choices_; // last frame's, per screen
        std::map<std::string, std::vector<Choice>> boxes_;   // ... and its clickable boxes
        core::TextureHandle atlasTexture_;
        std::function<ImageRef(const std::string &)> images_;
        std::string tip_;
        std::map<std::string, std::string> fields_; // "screen#id" -> a text field's text
        std::map<std::string, float> scales_; // "screen#choice" -> its drawn scale, eased each Update
    };
}
