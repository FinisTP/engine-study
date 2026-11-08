#include <Screen.hpp>

#include <set>

namespace umbra::ui
{
    namespace
    {
        const std::set<std::string> types{"panel", "stack", "text", "bar", "image", "button", "list", "spacer"};
        const std::set<std::string> anchors{"top-left", "top", "top-right", "left", "center", "right", "bottom-left", "bottom", "bottom-right", "fill"};
        const std::set<std::string> known{"type", "id", "anchor", "offset", "size", "style", "direction", "align", "gap", "columns", "text", "textAlign", "wrap",
                                          "visible", "disabled", "selected", "value", "max", "image", "action", "arg", "items", "as", "entry", "children", "at"};

        struct Reader final
        {
            const std::string &file;
            std::vector<core::Diagnostic> &out;
            void Error(const std::string &field, const std::string &message) { out.push_back({core::Severity::Error, "ui.screen", file, field, message}); }
            void Warning(const std::string &field, const std::string &message) { out.push_back({core::Severity::Warning, "ui.screen", file, field, message}); }

            // A text field; numbers are taken as text too ("max": 100).
            std::string Text(const nlohmann::json &j, const char *key, const std::string &where, std::string fallback = {})
            {
                if (!j.contains(key))
                    return fallback;
                if (j[key].is_string())
                    return j[key].get<std::string>();
                if (j[key].is_number())
                    return j[key].dump();
                Error(where + "." + key, "expected text");
                return fallback;
            }

            glm::vec2 Pair(const nlohmann::json &j, const char *key, const std::string &where)
            {
                if (!j.contains(key))
                    return {};
                const nlohmann::json &v = j[key];
                if (!v.is_array() || v.size() != 2 || !v[0].is_number() || !v[1].is_number())
                {
                    Error(where + "." + key, "expected [x, y] numbers");
                    return {};
                }
                return {v[0].get<float>(), v[1].get<float>()};
            }

            void Read(const nlohmann::json &j, const std::string &where, Node &node)
            {
                if (!j.is_object())
                {
                    Error(where, "expected a widget object");
                    return;
                }
                for (const auto &[key, value] : j.items())
                    if (!known.contains(key))
                        Warning(where + "." + key, "unknown field is ignored (typo?)");
                // 1. Kind and placement.
                node.type = Text(j, "type", where);
                if (!types.contains(node.type))
                    Error(where + ".type", "expected panel, stack, text, bar, image, button, list or spacer");
                node.id = Text(j, "id", where);
                node.anchor = Text(j, "anchor", where, node.anchor);
                if (!anchors.contains(node.anchor))
                    Error(where + ".anchor", "expected top-left, top, top-right, left, center, right, bottom-left, bottom, bottom-right or fill");
                node.offset = Pair(j, "offset", where);
                node.size = Pair(j, "size", where);
                node.style = Text(j, "style", where);
                node.direction = Text(j, "direction", where, node.direction);
                if (node.direction != "vertical" && node.direction != "horizontal")
                    Error(where + ".direction", "expected vertical or horizontal");
                node.align = Text(j, "align", where, node.align);
                if (node.align != "start" && node.align != "center" && node.align != "end" && node.align != "stretch")
                    Error(where + ".align", "expected start, center, end or stretch");
                node.gap = j.value("gap", 0.0F);
                node.columns = std::max(1, j.value("columns", 1));
                // 2. Content and bindings.
                node.text = Text(j, "text", where);
                node.textAlign = Text(j, "textAlign", where, node.textAlign);
                node.wrap = j.value("wrap", false);
                node.visible = Text(j, "visible", where);
                node.disabled = Text(j, "disabled", where);
                node.selected = Text(j, "selected", where);
                node.value = Text(j, "value", where);
                node.max = Text(j, "max", where, "1");
                node.image = Text(j, "image", where);
                node.action = Text(j, "action", where);
                node.arg = Text(j, "arg", where);
                node.items = Text(j, "items", where);
                node.as = Text(j, "as", where, node.as);
                node.at = Text(j, "at", where);
                if (node.type == "list")
                {
                    if (node.items.empty())
                        Error(where + ".items", "a list needs \"items\": a binding to an array, e.g. \"{inventory}\"");
                    if (!j.contains("entry"))
                        Error(where + ".entry", "a list needs \"entry\": the widget drawn for each item");
                }
                if (node.type == "bar" && node.value.empty())
                    Error(where + ".value", "a bar needs \"value\"");
                // 3. Children.
                if (j.contains("entry"))
                {
                    node.entry = std::make_unique<Node>();
                    Read(j["entry"], where + ".entry", *node.entry);
                }
                if (j.contains("children"))
                {
                    if (!j["children"].is_array())
                        Error(where + ".children", "expected an array of widgets");
                    else
                        for (std::size_t i = 0; i < j["children"].size(); ++i)
                            Read(j["children"][i], where + ".children[" + std::to_string(i) + "]", node.children.emplace_back());
                }
            }
        };
    }

    bool ParseScreen(const nlohmann::json &json, const std::string &name, const std::string &file, Screen &screen, std::vector<core::Diagnostic> &diagnostics)
    {
        const std::size_t before = diagnostics.size();
        Reader reader{file, diagnostics};
        if (!json.is_object() || json.value("format", "") != "umbra.ui")
        {
            reader.Error("format", "expected \"format\": \"umbra.ui\"");
            return false;
        }
        screen = {};
        screen.name = name;
        screen.file = file;
        screen.modal = json.value("modal", false);
        screen.dim = json.value("dim", false);
        screen.back = reader.Text(json, "back", "");
        if (!json.contains("root"))
            reader.Error("root", "a screen needs a \"root\" widget");
        else
            reader.Read(json["root"], "root", screen.root);
        for (std::size_t i = before; i < diagnostics.size(); ++i)
            if (diagnostics[i].severity == core::Severity::Error)
                return false;
        return true;
    }
}
