#pragma once

#include <Diagnostic.hpp>

#include <glm/vec2.hpp>
#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <vector>

namespace umbra::ui
{
    // One widget of a screen file (ui/<name>.ui.json, UI.md). Text fields may hold bindings like
    // "HP {hero.health}"; `visible`, `items`, `value`... are a single binding like "{menu.items}".
    struct Node final
    {
        std::string type;                 // panel, stack, text, bar, image, button, list, spacer, field
        std::string id;
        std::string anchor = "top-left";  // top-left, top, top-right, left, center, right, bottom-left, bottom, bottom-right, fill
        glm::vec2 offset{0.0F};           // pixels from the anchor (fill: margins)
        glm::vec2 size{0.0F};             // pixels; 0 = from the content
        std::string style;                // skin style ("" = the type's default)
        std::string direction = "vertical"; // stack, list
        std::string align = "start";      // children across a stack: start, center, end, stretch
        float gap = 0.0F;                 // stack, list
        int columns = 1;                  // list: a grid when > 1
        std::string text, textAlign = "left";
        bool wrap = false;
        std::string visible, disabled, selected; // bindings; empty = always visible, never disabled/selected
        std::string tip;                         // text told while the mouse is over it (UiSystem::Tip)
        std::string fit;                         // an image: "" stretch, "contain", "cover" or "top" (Painter.hpp FitPicture)
        std::string value, max;           // bar; field: max = most letters (default 24)
        std::string placeholder;          // field: shown dimmed while it is empty
        std::string image;                // asset path (may contain bindings)
        std::string action, arg;          // button or list entry: what the game gets when it is chosen
        std::string items, as = "item";   // list: an array binding and the name of each entry
        std::string at;                   // list entry: "{target.at}" = [x, y] pixels; the entry sits centered above that point
        std::unique_ptr<Node> entry;      // list: the widget drawn for each entry
        std::vector<Node> children;
    };

    struct Screen final
    {
        std::string name;    // file name without .ui.json
        std::string file;
        bool modal = false;  // takes the input and blocks screens below it
        bool dim = false;    // darkens what is behind it
        std::string back;    // action for Esc / gamepad B ("close" closes the screen)
        Node root;
    };

    // Checks and reads a screen file; problems name the field, e.g. "root.children[2].anchor".
    bool ParseScreen(const nlohmann::json &json, const std::string &name, const std::string &file, Screen &screen,
                     std::vector<core::Diagnostic> &diagnostics);
}
