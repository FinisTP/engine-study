#include <Skin.hpp>

#include <cstdio>

namespace umbra::ui
{
    std::optional<glm::vec4> ParseColor(const nlohmann::json &value)
    {
        if (value.is_array() && (value.size() == 3 || value.size() == 4))
        {
            glm::vec4 c{1.0F};
            for (std::size_t i = 0; i < value.size(); ++i)
            {
                if (!value[i].is_number())
                    return std::nullopt;
                c[static_cast<int>(i)] = value[i].get<float>();
            }
            return c;
        }
        if (value.is_string())
        {
            const std::string text = value.get<std::string>();
            unsigned r = 0, g = 0, b = 0, a = 255;
            if ((text.size() == 7 && std::sscanf(text.c_str(), "#%2x%2x%2x", &r, &g, &b) == 3) ||
                (text.size() == 9 && std::sscanf(text.c_str(), "#%2x%2x%2x%2x", &r, &g, &b, &a) == 4))
                return glm::vec4{static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), static_cast<float>(a)} / 255.0F;
        }
        return std::nullopt;
    }

    namespace
    {
        // A number for all four sides, or [left, top, right, bottom], or [horizontal, vertical].
        std::optional<glm::vec4> ParseSides(const nlohmann::json &value)
        {
            if (value.is_number())
                return glm::vec4{value.get<float>()};
            if (value.is_array() && value.size() == 2 && value[0].is_number() && value[1].is_number())
                return glm::vec4{value[0].get<float>(), value[1].get<float>(), value[0].get<float>(), value[1].get<float>()};
            if (value.is_array() && value.size() == 4)
                return glm::vec4{value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>()};
            return std::nullopt;
        }

        const char *stateNames[] = {"", "hover", "selected", "disabled"};
    }

    bool Skin::Load(const nlohmann::json &json, const std::string &file, std::vector<core::Diagnostic> &diagnostics)
    {
        const auto error = [&](const std::string &field, const std::string &message) {
            diagnostics.push_back({core::Severity::Error, "ui.skin", file, field, message});
        };
        if (!json.is_object() || json.value("format", "") != "umbra.ui-skin")
        {
            error("format", "expected \"format\": \"umbra.ui-skin\"");
            return false;
        }
        const nlohmann::json fonts = json.value("fonts", nlohmann::json::object()); // a local: items() must not outlive it
        for (const auto &[name, font] : fonts.items())
            if (!font.is_object() || !font.contains("file"))
                error("fonts." + name, "needs a \"file\" (a .ttf asset path)");
            else
                fonts_[name] = {font["file"].get<std::string>(), font.value("size", 18.0F)};
        if (json.contains("dim"))
        {
            if (const auto dim = ParseColor(json["dim"]))
                dim_ = *dim;
            else
                error("dim", "expected a color");
        }
        // Styles are kept as JSON and resolved on use (base chains, states).
        const nlohmann::json styles = json.value("styles", nlohmann::json::object());
        for (const auto &[name, style] : styles.items())
        {
            if (!style.is_object())
            {
                error("styles." + name, "expected an object");
                continue;
            }
            for (const auto &[key, value] : style.items())
                if ((key == "color" || key == "background" || key == "border" || key == "fill" || key == "tint") && !ParseColor(value))
                    error("styles." + name + "." + key, "expected a color: [r, g, b, a] or \"#rrggbb\"");
            if (style.contains("base") && !styles.contains(style["base"].get<std::string>()))
                error("styles." + name + ".base", "no style '" + style["base"].get<std::string>() + "'");
            if (style.contains("font") && !fonts_.contains(style["font"].get<std::string>()))
                error("styles." + name + ".font", "no font '" + style["font"].get<std::string>() + "'");
            styles_[name] = style;
        }
        return !core::HasErrors(diagnostics);
    }

    void Skin::Apply(const nlohmann::json &style, Style &out, int depth) const
    {
        // 1. The base style first (at most 8 deep, so a loop can't hang), then this one's fields.
        if (depth < 8 && style.contains("base"))
            if (const auto base = styles_.find(style["base"].get<std::string>()); base != styles_.end())
                Apply(base->second, out, depth + 1);
        const auto color = [&](const char *key, glm::vec4 &field) {
            if (style.contains(key))
                field = ParseColor(style[key]).value_or(field);
        };
        color("color", out.color);
        color("background", out.background);
        color("border", out.border);
        color("fill", out.fill);
        color("tint", out.tint);
        out.font = style.value("font", out.font);
        out.borderWidth = style.value("borderWidth", out.borderWidth);
        out.image = style.value("image", out.image);
        if (style.contains("padding"))
            out.padding = ParseSides(style["padding"]).value_or(out.padding);
        if (style.contains("slice"))
            out.slice = ParseSides(style["slice"]).value_or(out.slice);
    }

    Style Skin::Get(const std::string &name, State state) const
    {
        Style out;
        const auto it = styles_.find(name);
        if (it == styles_.end())
            return out;
        Apply(it->second, out, 0);
        // 2. The state's changes, if the style (or its base) has them.
        if (state != State::Normal)
            for (const nlohmann::json *s = &it->second; s != nullptr;)
            {
                if (const char *key = stateNames[static_cast<int>(state)]; s->contains(key))
                {
                    Apply((*s)[key], out, 8);
                    break;
                }
                const auto base = s->contains("base") ? styles_.find((*s)["base"].get<std::string>()) : styles_.end();
                s = base == styles_.end() ? nullptr : &base->second;
            }
        return out;
    }
}
