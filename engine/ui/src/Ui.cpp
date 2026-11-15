#include <Ui.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>

namespace umbra::ui
{
    using nlohmann::json;

    // Everything one screen's layout pass needs: the data, the list entries being drawn (innermost
    // last), the painter and the screen's choices.
    struct UiSystem::Context final
    {
        const json &data;
        Painter &painter;
        bool input = false;          // this screen takes the input (focus is shown)
        int focus = 0;
        std::vector<Choice> choices; // collected while placing, in order
        std::vector<Choice> boxes;   // panels and stacks with an action: clicked anywhere, never focused
        std::vector<std::pair<std::string, const json *>> scopes; // list entries: "item" -> its value
        std::vector<int> indices;    // their positions, for "{index}"
        std::optional<State> entryState; // the state of the next list entry's root widget
        State inherited = State::Normal; // a focused or disabled entry's state, seen by the widgets inside it

        // "hero.health" -> the value: list entry names first, then the data; nullptr if missing.
        const json *Lookup(const std::string &path) const
        {
            std::size_t dot = path.find('.');
            const std::string first = path.substr(0, dot);
            const json *at = nullptr;
            for (auto it = scopes.rbegin(); it != scopes.rend() && at == nullptr; ++it)
                if (it->first == first)
                    at = it->second;
            if (at == nullptr)
                at = data.is_object() && data.contains(first) ? &data[first] : nullptr;
            while (at != nullptr && dot != std::string::npos)
            {
                const std::size_t next = path.find('.', dot + 1);
                const std::string key = path.substr(dot + 1, next == std::string::npos ? std::string::npos : next - dot - 1);
                if (at->is_object())
                    at = at->contains(key) ? &(*at)[key] : nullptr;
                else if (at->is_array() && !key.empty() && key.find_first_not_of("0123456789") == std::string::npos && std::stoul(key) < at->size())
                    at = &(*at)[std::stoul(key)];
                else
                    at = nullptr;
                dot = next;
            }
            return at;
        }

        // A single "{path}" -> its value; anything else -> the text with bindings filled in.
        json Value(const std::string &binding) const
        {
            if (binding.size() > 2 && binding.front() == '{' && binding.back() == '}' && binding.find('{', 1) == std::string::npos)
            {
                const std::string path = binding.substr(1, binding.size() - 2);
                if (path == "index")
                    return indices.empty() ? 0 : indices.back();
                const json *v = Lookup(path);
                return v ? *v : json();
            }
            return Text(binding);
        }

        static std::string ToText(const json &v)
        {
            if (v.is_string())
                return v.get<std::string>();
            if (v.is_number_integer() || v.is_boolean())
                return v.dump();
            if (v.is_number())
            {
                const double d = v.get<double>();
                char buffer[32];
                std::snprintf(buffer, sizeof(buffer), std::abs(d - std::round(d)) < 1e-6 ? "%.0f" : "%.1f", d);
                return buffer;
            }
            return {};
        }

        std::string Text(const std::string &text) const
        {
            std::string out;
            for (std::size_t at = 0; at < text.size();)
            {
                const std::size_t open = text.find('{', at), close = open == std::string::npos ? open : text.find('}', open);
                if (close == std::string::npos)
                {
                    out += text.substr(at);
                    break;
                }
                out += text.substr(at, open - at);
                out += ToText(Value(text.substr(open, close - open + 1)));
                at = close + 1;
            }
            return out;
        }

        bool Truthy(const std::string &binding) const
        {
            const json v = Value(binding);
            return !(v.is_null() || (v.is_boolean() && !v.get<bool>()) || (v.is_number() && v.get<double>() == 0.0) ||
                     (v.is_string() && v.get<std::string>().empty()) || (v.is_array() && v.empty()));
        }

        float Number(const std::string &binding) const
        {
            const json v = Value(binding);
            if (v.is_number())
                return v.get<float>();
            try
            {
                return v.is_string() ? std::stof(v.get<std::string>()) : 0.0F;
            }
            catch (const std::exception &)
            {
                return 0.0F;
            }
        }
    };

    namespace
    {
        std::string DefaultStyle(const Node &node)
        {
            if (!node.style.empty())
                return node.style;
            return node.type == "list" || node.type == "stack" || node.type == "spacer" ? "" : node.type;
        }

        // Where a widget of `size` goes inside `parent` (UI.md, "Anchors").
        Rect AnchorRect(Rect parent, glm::vec2 size, const std::string &anchor, glm::vec2 offset)
        {
            if (anchor == "fill")
                return {parent.x + offset.x, parent.y + offset.y, parent.w - 2 * offset.x, parent.h - 2 * offset.y};
            const float ax = anchor.find("left") != std::string::npos ? 0.0F : anchor.find("right") != std::string::npos ? 1.0F : 0.5F;
            const float ay = anchor.find("top") != std::string::npos ? 0.0F : anchor.find("bottom") != std::string::npos ? 1.0F : 0.5F;
            return {std::round(parent.x + (parent.w - size.x) * ax + offset.x), std::round(parent.y + (parent.h - size.y) * ay + offset.y), size.x, size.y};
        }

        Rect Inset(Rect r, glm::vec4 padding)
        {
            return {r.x + padding.x, r.y + padding.y, std::max(0.0F, r.w - padding.x - padding.z), std::max(0.0F, r.h - padding.y - padding.w)};
        }

        std::vector<unsigned char> ReadBytes(const std::filesystem::path &file)
        {
            std::ifstream in(file, std::ios::binary);
            return {std::istreambuf_iterator<char>(in), {}};
        }
    }

    bool UiSystem::Load(const assets::AssetFolders &assets, std::vector<core::Diagnostic> &diagnostics)
    {
        // 1. The skin, then its fonts packed into one atlas.
        const std::filesystem::path skinFile = assets / "ui/skin.json";
        std::ifstream in(skinFile);
        if (!in || !skin_.Load(json::parse(in, nullptr, false), skinFile.generic_string(), diagnostics))
        {
            if (!in)
                diagnostics.push_back({core::Severity::Error, "ui.skin", skinFile.generic_string(), "", "no ui/skin.json in the project or its plugins"});
            return false;
        }
        std::vector<FontSource> sources;
        for (const auto &[name, font] : skin_.Fonts())
            sources.push_back({name, ReadBytes(assets / font.file), font.size});
        if (!fonts_.Build(sources, diagnostics))
            return false;
        // 2. Every screen file.
        screens_.clear();
        for (const std::string &path : assets.ListFiles())
        {
            if (!path.starts_with("ui/") || !path.ends_with(".ui.json"))
                continue;
            std::ifstream file(assets / path);
            const std::string name = path.substr(path.rfind('/') + 1, path.size() - path.rfind('/') - 1 - 8);
            Screen screen;
            if (ParseScreen(json::parse(file, nullptr, false), name, path, screen, diagnostics))
                screens_[name] = std::move(screen);
        }
        return !core::HasErrors(diagnostics);
    }

    bool UiSystem::Open(const std::string &screen)
    {
        if (!screens_.contains(screen))
            return false;
        Close(screen);
        stack_.push_back(screen);
        return true;
    }

    void UiSystem::Close(const std::string &screen)
    {
        stack_.erase(std::remove(stack_.begin(), stack_.end(), screen), stack_.end());
    }

    bool UiSystem::IsOpen(const std::string &screen) const
    {
        return std::find(stack_.begin(), stack_.end(), screen) != stack_.end();
    }

    bool UiSystem::TakesInput() const
    {
        for (auto it = stack_.rbegin(); it != stack_.rend(); ++it)
            if (screens_.at(*it).modal)
                return true;
        return false;
    }

    void UiSystem::ResetFocus(const std::string &screen)
    {
        focus_[screen] = 0;
    }

    glm::vec2 UiSystem::Measure(const Node &node, Context &c, float width)
    {
        // The size a widget wants: its content plus padding, unless the file gives a size.
        if (!node.visible.empty() && !c.Truthy(node.visible))
            return {0.0F, 0.0F};
        const Style style = skin_.Get(DefaultStyle(node), State::Normal);
        const glm::vec2 pad{style.padding.x + style.padding.z, style.padding.y + style.padding.w};
        glm::vec2 content{0.0F};
        if (node.type == "text" || node.type == "button")
        {
            if (const Face *face = fonts_.Find(style.font))
            {
                const float wrap = node.wrap ? (node.size.x > 0.0F ? node.size.x : width) - pad.x : 0.0F;
                content = FontAtlas::Measure(*face, c.Text(node.text), wrap);
            }
        }
        else if (node.type == "bar")
            content = {200.0F, 16.0F};
        else if (node.type == "image")
            content = images_ ? images_(c.Text(node.image)).size : glm::vec2{0.0F};
        else if (node.type == "panel")
            for (const Node &child : node.children)
            {
                if (child.anchor == "fill")
                    continue;
                const glm::vec2 s = Measure(child, c, width - pad.x);
                content = glm::max(content, s + glm::abs(child.offset));
            }
        else if (node.type == "stack")
        {
            const bool vertical = node.direction == "vertical";
            int shown = 0;
            for (const Node &child : node.children)
            {
                const glm::vec2 s = Measure(child, c, width - pad.x);
                if (s.x <= 0.0F && s.y <= 0.0F)
                    continue;
                ++shown;
                content = vertical ? glm::vec2{std::max(content.x, s.x), content.y + s.y} : glm::vec2{content.x + s.x, std::max(content.y, s.y)};
            }
            (vertical ? content.y : content.x) += node.gap * static_cast<float>(std::max(0, shown - 1));
        }
        else if (node.type == "list" && node.entry && !node.entry->at.empty())
            content = {0.0F, 0.0F}; // entries placed at points on the screen take no room here
        else if (node.type == "list" && node.entry)
        {
            // Cells are as big as the biggest entry; a grid has `columns` per row.
            const json items = c.Value(node.items);
            const std::size_t count = items.is_array() ? items.size() : 0;
            glm::vec2 cell{0.0F};
            for (std::size_t i = 0; i < count; ++i)
            {
                c.scopes.push_back({node.as, &items[i]});
                c.indices.push_back(static_cast<int>(i));
                cell = glm::max(cell, Measure(*node.entry, c, width - pad.x));
                c.scopes.pop_back();
                c.indices.pop_back();
            }
            const std::size_t columns = node.direction == "horizontal" ? std::max<std::size_t>(count, 1) : static_cast<std::size_t>(node.columns);
            const std::size_t rows = (count + columns - 1) / columns;
            const std::size_t shownColumns = std::min(columns, std::max<std::size_t>(count, 1));
            content = {cell.x * static_cast<float>(shownColumns) + node.gap * static_cast<float>(shownColumns - 1),
                       cell.y * static_cast<float>(rows) + node.gap * static_cast<float>(rows > 0 ? rows - 1 : 0)};
        }
        glm::vec2 size = content + pad;
        if (node.size.x > 0.0F)
            size.x = node.size.x;
        if (node.size.y > 0.0F)
            size.y = node.size.y;
        return size;
    }

    void UiSystem::DrawBox(const Style &style, Rect rect, Context &c)
    {
        c.painter.Fill(rect, style.background);
        if (!style.image.empty() && images_)
            if (const ImageRef image = images_(style.image); image.texture)
                c.painter.NineSlice(rect, image.texture, image.size, style.slice, style.tint);
        c.painter.Frame(rect, style.border, style.borderWidth);
    }

    void UiSystem::Place(const Node &node, Rect rect, Context &c)
    {
        const std::optional<State> entryState = std::exchange(c.entryState, std::nullopt);
        if (!node.visible.empty() && !c.Truthy(node.visible))
            return;
        // 1. State: disabled, focused (a button the player is on), selected, a list entry's, or
        //    the state of the entry this widget is inside (so an arrow can light up with its entry).
        const bool disabled = !node.disabled.empty() && c.Truthy(node.disabled);
        State state = entryState.value_or(!node.selected.empty() && c.Truthy(node.selected) ? State::Selected : c.inherited);
        if (node.type == "button" && !disabled)
        {
            if (c.input && static_cast<int>(c.choices.size()) == c.focus)
                state = State::Hover;
            c.choices.push_back({rect, c.Text(node.action), c.Text(node.arg)});
        }
        if ((node.type == "panel" || node.type == "stack") && !node.action.empty() && !disabled)
            c.boxes.push_back({rect, c.Text(node.action), c.Text(node.arg)});
        if (disabled)
            state = State::Disabled;
        const Style style = skin_.Get(DefaultStyle(node), state);
        const State outer = std::exchange(c.inherited, state);
        struct Restore { Context &c; State state; ~Restore() { c.inherited = state; } } restore{c, outer};
        DrawBox(style, rect, c);
        const Rect inner = Inset(rect, style.padding);

        // 2. The content of each kind.
        if (node.type == "text" || node.type == "button")
        {
            if (const Face *face = fonts_.Find(style.font))
            {
                const std::string text = c.Text(node.text);
                const float align = node.textAlign == "center" ? 0.5F : node.textAlign == "right" ? 1.0F : 0.0F;
                Rect area = inner;
                if (node.type == "button") // centered vertically in the button
                    area.y += std::round((inner.h - FontAtlas::Measure(*face, text, node.wrap ? inner.w : 0.0F).y) * 0.5F);
                c.painter.Text(*face, area, text, style.color, node.wrap, align);
            }
        }
        else if (node.type == "bar")
        {
            const float max = c.Number(node.max);
            const float fraction = max > 0.0F ? std::clamp(c.Number(node.value) / max, 0.0F, 1.0F) : 0.0F;
            c.painter.Fill({inner.x, inner.y, std::round(inner.w * fraction), inner.h}, style.fill);
            if (!node.text.empty())
                if (const Face *face = fonts_.Find(style.font))
                {
                    const std::string text = c.Text(node.text);
                    const float h = FontAtlas::Measure(*face, text).y;
                    c.painter.Text(*face, {inner.x, inner.y + std::round((inner.h - h) * 0.5F), inner.w, h}, text, style.color, false, 0.5F);
                }
        }
        else if (node.type == "image" && images_)
        {
            if (const ImageRef image = images_(c.Text(node.image)); image.texture)
                c.painter.Image(inner, image.texture, style.tint);
        }
        else if (node.type == "panel")
        {
            for (const Node &child : node.children)
                Place(child, AnchorRect(inner, Measure(child, c, inner.w), child.anchor, child.offset), c);
        }
        else if (node.type == "stack")
        {
            // One after another; across the stack each child goes to start, center, end, or fills.
            const bool vertical = node.direction == "vertical";
            float along = 0.0F;
            for (const Node &child : node.children)
            {
                glm::vec2 s = Measure(child, c, inner.w);
                if (s.x <= 0.0F && s.y <= 0.0F)
                    continue;
                const float space = vertical ? inner.w : inner.h, size = vertical ? s.x : s.y;
                const float k = node.align == "center" ? 0.5F : node.align == "end" ? 1.0F : 0.0F;
                const float across = node.align == "stretch" ? 0.0F : std::round((space - size) * k);
                if (node.align == "stretch")
                    (vertical ? s.x : s.y) = space;
                const Rect r = vertical ? Rect{inner.x + across, inner.y + along, s.x, s.y} : Rect{inner.x + along, inner.y + across, s.x, s.y};
                Place(child, {r.x + child.offset.x, r.y + child.offset.y, r.w, r.h}, c);
                along += (vertical ? s.y : s.x) + node.gap;
            }
        }
        else if (node.type == "list" && node.entry)
        {
            // Entries in cells (one column, one row, or a grid); each is a choice when the list has an action.
            const json items = c.Value(node.items);
            const std::size_t count = items.is_array() ? items.size() : 0;
            glm::vec2 cell{0.0F};
            for (std::size_t i = 0; i < count; ++i)
            {
                c.scopes.push_back({node.as, &items[i]});
                c.indices.push_back(static_cast<int>(i));
                cell = glm::max(cell, Measure(*node.entry, c, inner.w));
                c.scopes.pop_back();
                c.indices.pop_back();
            }
            const bool atPoints = !node.entry->at.empty(); // e.g. arrows over enemies in the scene
            const std::size_t columns = node.direction == "horizontal" ? std::max<std::size_t>(count, 1) : static_cast<std::size_t>(node.columns);
            if (node.columns == 1 && node.direction == "vertical")
                cell.x = inner.w; // a single column fills the width
            if (!atPoints)
                c.painter.PushClip(rect);
            for (std::size_t i = 0; i < count; ++i)
            {
                c.scopes.push_back({node.as, &items[i]});
                c.indices.push_back(static_cast<int>(i));
                Rect r{inner.x + static_cast<float>(i % columns) * (cell.x + node.gap), inner.y + static_cast<float>(i / columns) * (cell.y + node.gap), cell.x, cell.y};
                if (atPoints)
                {
                    // Centered above the point: its own size, bottom edge on the point.
                    const json point = c.Value(node.entry->at);
                    const glm::vec2 size = Measure(*node.entry, c, inner.w);
                    const bool ok = point.is_array() && point.size() == 2 && point[0].is_number() && point[1].is_number();
                    const glm::vec2 p = ok ? glm::vec2{point[0].get<float>(), point[1].get<float>()} : glm::vec2{-10000.0F};
                    r = {std::round(p.x - size.x * 0.5F), std::round(p.y - size.y), size.x, size.y};
                }
                const bool entryDisabled = !node.entry->disabled.empty() && c.Truthy(node.entry->disabled);
                if (!node.action.empty() && !entryDisabled)
                {
                    const bool focused = c.input && static_cast<int>(c.choices.size()) == c.focus;
                    c.choices.push_back({r, c.Text(node.action), c.Text(node.arg)});
                    if (focused)
                        c.entryState = State::Hover;
                }
                Place(*node.entry, r, c);
                c.entryState.reset();
                c.scopes.pop_back();
                c.indices.pop_back();
            }
            if (!atPoints)
                c.painter.PopClip();
        }
    }

    std::vector<UiAction> UiSystem::Update(const json &data, glm::vec2 viewport, const UiInput &input, core::UiDrawList &out)
    {
        std::vector<UiAction> actions;
        // 1. Input goes to the top modal screen, using last frame's layout of its choices.
        std::string target;
        for (auto it = stack_.rbegin(); it != stack_.rend() && target.empty(); ++it)
            if (screens_.at(*it).modal)
                target = *it;
        if (!target.empty())
        {
            const std::vector<Choice> &choices = choices_[target];
            int &focus = focus_[target];
            const int count = static_cast<int>(choices.size());
            focus = count == 0 ? 0 : std::clamp(focus, 0, count - 1);
            if (count > 0)
            {
                if (input.up || input.left)
                    focus = (focus + count - 1) % count;
                if (input.down || input.right)
                    focus = (focus + 1) % count;
                for (int i = 0; i < count; ++i)
                    if (choices[static_cast<std::size_t>(i)].rect.Contains(input.mouse))
                    {
                        if (input.mouseMoved)
                            focus = i;
                    }
                if (input.confirm)
                    actions.push_back({target, choices[static_cast<std::size_t>(focus)].action, choices[static_cast<std::size_t>(focus)].arg});
            }
            if (input.back && !screens_.at(target).back.empty())
                actions.push_back({target, screens_.at(target).back, ""});
        }
        // 1b. A click: the topmost screen with a choice under the mouse takes it; a dimmed screen blocks those below.
        if (input.click)
            for (auto it = stack_.rbegin(); it != stack_.rend(); ++it)
            {
                const auto hit = std::find_if(choices_[*it].begin(), choices_[*it].end(), [&](const Choice &c) { return c.rect.Contains(input.mouse); });
                if (hit != choices_[*it].end())
                {
                    actions.push_back({*it, hit->action, hit->arg});
                    break;
                }
                // ... else the innermost clickable box under it (the last placed).
                const auto box = std::find_if(boxes_[*it].rbegin(), boxes_[*it].rend(), [&](const Choice &c) { return c.rect.Contains(input.mouse); });
                if (box != boxes_[*it].rend())
                {
                    actions.push_back({*it, box->action, box->arg});
                    break;
                }
                if (screens_.at(*it).dim)
                    break;
            }
        // 2. The UI's own actions: "close" closes the screen, "open:<screen>" opens one.
        for (const UiAction &action : actions)
        {
            if (action.action == "close")
                Close(action.screen);
            else if (action.action.starts_with("open:"))
                Open(action.action.substr(5));
        }

        // 3. Every open screen, bottom to top: dim, lay out, draw, remember its choices.
        Painter painter(out, fonts_, atlasTexture_);
        const Rect view{0.0F, 0.0F, viewport.x, viewport.y};
        for (const std::string &name : std::vector<std::string>(stack_))
        {
            const Screen &screen = screens_.at(name);
            if (screen.dim)
                painter.Fill(view, skin_.Dim());
            Context c{data, painter, name == target, focus_[name], {}, {}, {}, {}, State::Normal};
            Place(screen.root, AnchorRect(view, Measure(screen.root, c, view.w), screen.root.anchor, screen.root.offset), c);
            choices_[name] = std::move(c.choices);
            boxes_[name] = std::move(c.boxes);
        }
        return actions;
    }
}
