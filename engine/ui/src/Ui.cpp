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
        std::vector<Rect> covers;    // painted boxes: the pointer over them belongs to this screen
        std::vector<std::pair<std::string, const json *>> scopes; // list entries: "item" -> its value
        std::vector<int> indices;    // their positions, for "{index}"
        std::optional<State> entryState; // the state of the next list entry's root widget
        State inherited = State::Normal; // a focused or disabled entry's state, seen by the widgets inside it
        glm::vec2 mouse{-1.0F};          // for tips and the pointer's hover
        std::string tip;                 // the last tipped widget placed under the mouse
        bool held = false;               // the mouse button is down: what is under it looks pressed
        std::string screen;              // whose choices these are (scales_ keys)
        std::optional<Rect> view;        // inside a scroll box: what can be seen (choices outside it can't be clicked)
        std::vector<ScrollBox> scrolls;  // the scroll boxes placed this frame

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

        // The field widget with this id, anywhere in a screen (list entries included); nullptr if none.
        const Node *FindField(const Node &node, const std::string &id)
        {
            if (node.type == "field" && node.id == id)
                return &node;
            for (const Node &child : node.children)
                if (const Node *found = FindField(child, id))
                    return found;
            return node.entry ? FindField(*node.entry, id) : nullptr;
        }

        bool Continuation(char ch) { return (static_cast<unsigned char>(ch) & 0xC0) == 0x80; } // a UTF-8 letter's later bytes

        // Typed text into a field (UI.md "Text fields"): '\b' erases the last letter with all its
        // bytes, other control characters are dropped, and the field keeps at most `max` letters.
        void Type(std::string &field, const std::string &typed, std::size_t max)
        {
            for (std::size_t i = 0; i < typed.size(); ++i)
            {
                const auto ch = static_cast<unsigned char>(typed[i]);
                if (ch == '\b')
                {
                    while (!field.empty() && Continuation(field.back()))
                        field.pop_back();
                    if (!field.empty())
                        field.pop_back();
                }
                else if (ch >= 0x20 && ch != 0x7F && !Continuation(typed[i]))
                {
                    std::size_t end = i + 1;
                    while (end < typed.size() && Continuation(typed[end]))
                        ++end;
                    if (static_cast<std::size_t>(std::count_if(field.begin(), field.end(), [](char c) { return !Continuation(c); })) < max)
                        field.append(typed, i, end - i);
                    i = end - 1;
                }
            }
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
        sources_.clear();
        for (const auto &[name, font] : skin_.Fonts())
            sources_.push_back({name, ReadBytes(assets / font.file), font.size});
        textScale_ = 1.0F;
        if (!fonts_.Build(sources_, diagnostics))
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

    bool UiSystem::SetTextScale(float scale, std::vector<core::Diagnostic> &diagnostics)
    {
        if (scale < 0.5F || scale > 2.0F)
            return false;
        const float old = textScale_;
        textScale_ = scale;
        if (!BuildFonts(diagnostics))
            return textScale_ = old, false;
        return true;
    }

    bool UiSystem::BuildFonts(std::vector<core::Diagnostic> &diagnostics)
    {
        std::vector<FontSource> sized = sources_;
        for (FontSource &f : sized)
            f.size *= textScale_ * uiScale_;
        return fonts_.Build(sized, diagnostics);
    }

    glm::vec2 UiSystem::TextSize(const Face &face, std::string_view text, float wrap) const
    {
        // Measured in font pixels (fonts are uiScale_ larger), returned in layout units.
        return FontAtlas::Measure(face, text, wrap * uiScale_) / uiScale_;
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
        if (node.type == "field")
        {
            if (const Face *face = fonts_.Find(style.font))
            {
                const std::string &typed = fields_[c.screen + "#" + node.id];
                content = TextSize(*face, typed.empty() ? c.Text(node.placeholder) : typed);
                content.y = face->lineHeight;
            }
        }
        else if (node.type == "text" || node.type == "button")
        {
            if (const Face *face = fonts_.Find(style.font))
            {
                const float wrap = node.wrap ? (node.size.x > 0.0F ? node.size.x : width) - pad.x : 0.0F;
                content = TextSize(*face, c.Text(node.text), wrap);
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
        glm::vec2 fixed = node.size;
        if (!node.sizeBind.empty()) // a bound size: [w, h] from the data
            if (const json v = c.Value(node.sizeBind); v.is_array() && v.size() == 2 && v[0].is_number() && v[1].is_number())
                fixed = {v[0].get<float>(), v[1].get<float>()};
        if (fixed.x > 0.0F)
            size.x = fixed.x;
        if (fixed.y > 0.0F)
            size.y = fixed.y;
        return size;
    }

    void UiSystem::DrawBox(const Style &style, Rect rect, Context &c)
    {
        if (style.background.a > 0.0F || !style.image.empty())
            c.covers.push_back(rect);
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
        const bool choice = (node.type == "button" || node.type == "field") && !disabled;
        const bool focused = choice && c.input && static_cast<int>(c.choices.size()) == c.focus;
        const Rect hit = !c.view ? rect : [&] { // only the part seen inside a scroll box can be clicked
            const float x0 = std::max(rect.x, c.view->x), y0 = std::max(rect.y, c.view->y);
            const float x1 = std::min(rect.x + rect.w, c.view->x + c.view->w), y1 = std::min(rect.y + rect.h, c.view->y + c.view->h);
            return Rect{x0, y0, std::max(0.0F, x1 - x0), std::max(0.0F, y1 - y0)};
        }();
        if (choice)
        {
            if (focused || rect.Contains(c.mouse))
                state = rect.Contains(c.mouse) && c.held ? State::Pressed : State::Hover; // focused, or under the pointer
            if (node.type == "field")
                c.choices.push_back({hit, c.Text(node.action), "", node.id});
            else
                c.choices.push_back({hit, c.Text(node.action), c.Text(node.arg)});
        }
        if ((node.type == "panel" || node.type == "stack") && !node.action.empty() && !disabled)
            c.boxes.push_back({hit, c.Text(node.action), c.Text(node.arg)});
        if (state == State::Hover && !node.selected.empty() && c.Truthy(node.selected))
            state = State::Selected; // chosen stays lit under the pointer (its scale still answers below)
        if (disabled)
            state = State::Disabled;
        if (!node.tip.empty() && rect.Contains(c.mouse))
            if (std::string t = c.Text(node.tip); !t.empty())
                c.tip = std::move(t); // placed later = drawn on top
        const Style style = skin_.Get(DefaultStyle(node), state);
        // 1b. A choice (a button, a list entry with an action) eases toward its style's scale: a bounce in and out.
        if (choice || entryState)
        {
            float &scale = scales_.try_emplace(c.screen + "#" + std::to_string(c.choices.size() - 1), 1.0F).first->second;
            scale += (style.scale - scale) * 0.35F;
            if (std::abs(scale - 1.0F) > 0.001F)
                rect = {rect.x + rect.w * (1.0F - scale) * 0.5F, rect.y + rect.h * (1.0F - scale) * 0.5F, rect.w * scale, rect.h * scale};
        }
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
                    area.y += std::round((inner.h - TextSize(*face, text, node.wrap ? inner.w : 0.0F).y) * 0.5F);
                c.painter.Text(*face, area, text, style.color, node.wrap, align);
            }
        }
        else if (node.type == "field")
        {
            // The typed text, or the placeholder dimmed; a caret after the text while it has the focus.
            if (const Face *face = fonts_.Find(style.font))
            {
                const std::string &typed = fields_[c.screen + "#" + node.id];
                const std::string shown = typed.empty() ? c.Text(node.placeholder) : typed;
                const float top = inner.y + std::round((inner.h - face->lineHeight) * 0.5F);
                const glm::vec4 color = typed.empty() ? glm::vec4(glm::vec3(style.color), style.color.a * 0.5F) : style.color;
                c.painter.Text(*face, {inner.x, top, inner.w, face->lineHeight}, shown, color, false, 0.0F);
                if (focused)
                {
                    const float x = typed.empty() ? 0.0F : TextSize(*face, typed).x;
                    c.painter.Fill({inner.x + std::min(x, inner.w - 2.0F), top, 2.0F, face->lineHeight}, style.color);
                }
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
                    const float h = TextSize(*face, text).y;
                    c.painter.Text(*face, {inner.x, inner.y + std::round((inner.h - h) * 0.5F), inner.w, h}, text, style.color, false, 0.5F);
                }
        }
        else if (node.type == "image" && images_)
        {
            if (const ImageRef image = images_(c.Text(node.image)); image.texture)
            {
                Fitted f = FitPicture(inner, image.size, node.fit);
                // 1. Mirrored: the picture's left and right swap.
                if (!node.flip.empty() && c.Truthy(node.flip))
                    std::swap(f.uv0.x, f.uv1.x);
                // 2. Tinted: the node's colour times the style's.
                glm::vec4 tint = style.tint;
                if (!node.tint.empty())
                    tint *= ParseColor(c.Text(node.tint)).value_or(glm::vec4{1.0F});
                c.painter.Image(f.rect, image.texture, tint, f.uv0, f.uv1);
            }
        }
        else if (node.type == "panel")
        {
            if (node.clip)
                c.painter.PushClip(rect);
            for (const Node &child : node.children)
                Place(child, AnchorRect(inner, Measure(child, c, inner.w), child.anchor, child.offset), c);
            if (node.clip)
                c.painter.PopClip();
        }
        else if (node.type == "scroll")
        {
            // A column seen through its box: moved up by the wheel's offset, kept between its ends (UI.md "Scroll").
            float height = 0.0F;
            for (const Node &child : node.children)
                if (const glm::vec2 s = Measure(child, c, inner.w); s.y > 0.0F)
                    height += s.y + node.gap;
            const std::string key = c.screen + "#" + node.id;
            const float max = std::max(0.0F, height - node.gap - inner.h);
            float &offset = scroll_[key];
            offset = std::clamp(offset, 0.0F, max);
            c.scrolls.push_back({key, rect, max});
            const std::optional<Rect> outer = std::exchange(c.view, inner);
            c.painter.PushClip(inner);
            float along = -offset;
            for (const Node &child : node.children)
            {
                const glm::vec2 s = Measure(child, c, inner.w);
                if (s.x <= 0.0F && s.y <= 0.0F)
                    continue;
                Place(child, {inner.x + child.offset.x, inner.y + along + child.offset.y, child.size.x > 0.0F ? s.x : inner.w, s.y}, c);
                along += s.y + node.gap;
            }
            c.painter.PopClip();
            c.view = outer;
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
                    const glm::vec2 p = ok ? glm::vec2{point[0].get<float>(), point[1].get<float>()} / uiScale_ : glm::vec2{-10000.0F}; // given in window pixels
                    r = {std::round(p.x - size.x * 0.5F), std::round(p.y - size.y), size.x, size.y};
                }
                const bool entryDisabled = !node.entry->disabled.empty() && c.Truthy(node.entry->disabled);
                if (!node.action.empty() && !entryDisabled)
                {
                    const bool focused = (c.input && static_cast<int>(c.choices.size()) == c.focus) || r.Contains(c.mouse);
                    c.choices.push_back({r, c.Text(node.action), c.Text(node.arg)});
                    if (focused)
                        c.entryState = r.Contains(c.mouse) && c.held ? State::Pressed : State::Hover;
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

    std::vector<UiAction> UiSystem::Update(const json &data, glm::vec2 windowPixels, const UiInput &windowInput, core::UiDrawList &out)
    {
        // 0. The UI scale: lay out for the design height, draw at the window's (fonts rebuilt at its size).
        if (const float k = designHeight_ > 0.0F && windowPixels.y > 0.0F ? std::max(0.5F, windowPixels.y / designHeight_) : 1.0F; std::abs(k - uiScale_) > 0.001F)
        {
            const float old = uiScale_;
            uiScale_ = k;
            std::vector<core::Diagnostic> ignored;
            if (!BuildFonts(ignored))
                uiScale_ = old, (void)BuildFonts(ignored);
            else if (uploader_)
                atlasTexture_ = uploader_(fonts_.Image());
        }
        const glm::vec2 viewport = windowPixels / uiScale_;
        UiInput input = windowInput;
        input.mouse /= uiScale_;
        const std::size_t firstVertex = out.vertices.size(), firstBatch = out.batches.size();
        // 0b. The wheel moves the topmost scroll box under the mouse (last frame's boxes), 40 units a step.
        if (input.wheel != 0.0F)
            for (auto it = stack_.rbegin(); it != stack_.rend(); ++it)
                if (const auto &boxes = scrollBoxes_[*it]; std::ranges::any_of(boxes, [&](const ScrollBox &b) { return b.rect.Contains(input.mouse); }))
                {
                    for (auto b = boxes.rbegin(); b != boxes.rend(); ++b)
                        if (b->rect.Contains(input.mouse))
                        {
                            scroll_[b->key] = std::clamp(scroll_[b->key] - input.wheel * 40.0F, 0.0F, b->max);
                            break;
                        }
                    break;
                }
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
                // 1a. A focused text field takes the typed text and keeps left and right for itself;
                //     a key that typed something (W/A/S/D in some games) doesn't move the focus either.
                const std::string &field = choices[static_cast<std::size_t>(focus)].field;
                const bool typing = !field.empty() && !input.text.empty();
                if (!field.empty())
                {
                    const Node *node = FindField(screens_.at(target).root, field);
                    Type(fields_[target + "#" + field], input.text, node ? static_cast<std::size_t>(std::max(1, std::atoi(node->max.c_str()))) : 24);
                }
                if (!typing && (input.up || (input.left && field.empty())))
                    focus = (focus + count - 1) % count;
                if (!typing && (input.down || (input.right && field.empty())))
                    focus = (focus + 1) % count;
                for (int i = 0; i < count; ++i)
                    if (choices[static_cast<std::size_t>(i)].rect.Contains(input.mouse))
                    {
                        if (input.mouseMoved)
                            focus = i;
                    }
                const Choice &chosen = choices[static_cast<std::size_t>(focus)];
                if (input.confirm && !chosen.action.empty())
                    actions.push_back({target, chosen.action, chosen.field.empty() ? chosen.arg : fields_[target + "#" + chosen.field]});
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
                    if (hit->field.empty())
                        actions.push_back({*it, hit->action, hit->arg});
                    else // a click on a text field gives it the focus
                        focus_[*it] = static_cast<int>(hit - choices_[*it].begin());
                    break;
                }
                // ... else the innermost clickable box under it (the last placed).
                const auto box = std::find_if(boxes_[*it].rbegin(), boxes_[*it].rend(), [&](const Choice &c) { return c.rect.Contains(input.mouse); });
                if (box != boxes_[*it].rend())
                {
                    actions.push_back({*it, box->action, box->arg});
                    break;
                }
                if (screens_.at(*it).dim || std::ranges::any_of(covers_[*it], [&](const Rect &r) { return r.Contains(input.mouse); }))
                    break; // a dimmed screen, or a painted part of this one, keeps the click
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
        painter.SetFontScale(uiScale_);
        tip_.clear();
        const Rect view{0.0F, 0.0F, viewport.x, viewport.y};
        const std::vector<std::string> order(stack_);
        for (std::size_t i = 0; i < order.size(); ++i)
        {
            const std::string &name = order[i];
            const Screen &screen = screens_.at(name);
            if (screen.dim)
                painter.Fill(view, skin_.Dim());
            // A screen under another's painted part (or under a dimmed or modal one) does not see the pointer.
            const glm::vec2 mouse = Covered(i, input.mouse) ? glm::vec2{-1e9F} : input.mouse;
            Context c{data, painter, name == target, focus_[name], {}, {}, {}, {}, {}, State::Normal, State::Normal, mouse, {}, input.held, name};
            Place(screen.root, AnchorRect(view, Measure(screen.root, c, view.w), screen.root.anchor, screen.root.offset), c);
            choices_[name] = std::move(c.choices);
            boxes_[name] = std::move(c.boxes);
            scrollBoxes_[name] = std::move(c.scrolls);
            covers_[name] = std::move(c.covers);
            if (!c.tip.empty())
                tip_ = std::move(c.tip);
        }
        // 4. Layout units to window pixels.
        for (std::size_t i = firstVertex; i < out.vertices.size(); ++i)
            out.vertices[i].position *= uiScale_;
        for (std::size_t i = firstBatch; i < out.batches.size(); ++i)
            if (out.batches[i].clip.z < 1e8F)
                out.batches[i].clip *= uiScale_;
        return actions;
    }

    std::string UiSystem::FieldText(const std::string &screen, const std::string &id) const
    {
        const auto it = fields_.find(screen + "#" + id);
        return it == fields_.end() ? std::string() : it->second;
    }

    bool UiSystem::WantsText() const
    {
        // The top modal screen's focused choice (last frame's layout) is a text field.
        for (auto it = stack_.rbegin(); it != stack_.rend(); ++it)
            if (screens_.at(*it).modal)
            {
                const auto choices = choices_.find(*it);
                const auto focus = focus_.find(*it);
                const int at = focus == focus_.end() ? 0 : focus->second;
                return choices != choices_.end() && at >= 0 && at < static_cast<int>(choices->second.size()) &&
                       !choices->second[static_cast<std::size_t>(at)].field.empty();
            }
        return false;
    }

    bool UiSystem::Covered(std::size_t below, glm::vec2 mouse) const
    {
        // Last frame's layout of every screen above: dimmed or modal ones take it all; else their painted boxes and choices.
        const auto inside = [&](const auto &list, const std::string &name) {
            const auto it = list.find(name);
            return it != list.end() && std::ranges::any_of(it->second, [&](const auto &x) {
                       if constexpr (std::is_same_v<std::decay_t<decltype(x)>, Rect>)
                           return x.Contains(mouse);
                       else
                           return x.rect.Contains(mouse);
                   });
        };
        for (std::size_t j = below + 1; j < stack_.size(); ++j)
        {
            const std::string &name = stack_[j];
            if (screens_.at(name).dim || screens_.at(name).modal || inside(covers_, name) || inside(choices_, name) || inside(boxes_, name))
                return true;
        }
        return false;
    }

    float UiSystem::ChoiceScale(const std::string &screen, int choice) const
    {
        const auto it = scales_.find(screen + "#" + std::to_string(choice));
        return it == scales_.end() ? 1.0F : it->second;
    }
}
