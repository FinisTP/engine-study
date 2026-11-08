#pragma once

#include <Diagnostic.hpp>

#include <glm/vec4.hpp>
#include <nlohmann/json.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace umbra::ui
{
    // How a widget looks (UI.md). Every field may be left out: it then comes from "base" or the
    // built-in default. "hover", "focus", "selected" and "disabled" hold changes for those states.
    struct Style final
    {
        std::string font = "body";
        glm::vec4 color{1.0F};           // text
        glm::vec4 background{0.0F};      // fill behind the widget
        glm::vec4 border{0.0F};
        float borderWidth = 0.0F;
        glm::vec4 padding{0.0F};         // left, top, right, bottom
        glm::vec4 fill{0.4F, 0.8F, 0.4F, 1.0F}; // bars: the filled part
        std::string image;               // panel picture (asset path), drawn 9-sliced with `slice`
        glm::vec4 slice{0.0F};
        glm::vec4 tint{1.0F};            // image color
    };

    enum class State
    {
        Normal,
        Hover,    // mouse over it, or keyboard/gamepad focus
        Selected, // a list entry the game marks as chosen
        Disabled
    };

    struct FontFile final
    {
        std::string file; // asset path of a .ttf
        float size = 18.0F;
    };

    // ui/skin.json: fonts, the dim color behind modal screens, and named styles.
    class Skin final
    {
    public:
        bool Load(const nlohmann::json &json, const std::string &file, std::vector<core::Diagnostic> &diagnostics);

        [[nodiscard]] Style Get(const std::string &name, State state) const;
        [[nodiscard]] bool Has(const std::string &name) const { return styles_.contains(name); }
        [[nodiscard]] std::vector<std::string> Names() const
        {
            std::vector<std::string> names;
            for (const auto &[name, style] : styles_)
                names.push_back(name);
            return names;
        }
        [[nodiscard]] const std::map<std::string, FontFile> &Fonts() const { return fonts_; }
        [[nodiscard]] glm::vec4 Dim() const { return dim_; }

    private:
        void Apply(const nlohmann::json &style, Style &out, int depth) const;

        std::map<std::string, nlohmann::json> styles_;
        std::map<std::string, FontFile> fonts_;
        glm::vec4 dim_{0.0F, 0.0F, 0.0F, 0.55F};
    };

    // [r, g, b, a] (0..1) or "#rrggbb" / "#rrggbbaa"; nullopt if neither.
    std::optional<glm::vec4> ParseColor(const nlohmann::json &value);
}
