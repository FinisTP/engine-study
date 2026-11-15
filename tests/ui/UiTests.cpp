#include "assets/TestFiles.hpp"

#include <Raster.hpp>
#include <Ui.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <fstream>
#include <set>

using namespace umbra;
using nlohmann::json;

namespace
{
    // A project with its own screens; the skin and fonts come from the rpg-ui plugin underneath.
    assets::AssetFolders TestProject(const std::string &name, const std::map<std::string, json> &screens)
    {
        const auto root = test::TempDir("ui-" + name);
        std::filesystem::create_directories(root / "ui");
        for (const auto &[file, screen] : screens)
            test::WriteText(root / "ui" / (file + ".ui.json"), screen.dump());
        assets::AssetFolders folders(root);
        folders.AddPlugin(UMBRA_RPG_UI_ASSETS);
        return folders;
    }

    json Menu()
    {
        return {{"format", "umbra.ui"}, {"version", 1}, {"modal", true}, {"dim", true}, {"back", "close"},
                {"root", {{"type", "panel"}, {"anchor", "center"}, {"children", {
                    {{"type", "stack"}, {"gap", 4}, {"children", {
                        {{"type", "text"}, {"text", "HP {hero.health}/{hero.maxHealth}"}},
                        {{"type", "list"}, {"items", "{menu}"}, {"action", "choose"}, {"arg", "{index}"},
                         {"entry", {{"type", "text"}, {"style", "entry"}, {"text", "{item.name}"}, {"disabled", "{item.locked}"}}}},
                        {{"type", "button"}, {"text", "Resume"}, {"action", "resume"}}}}}}}}}};
    }
}

TEST_CASE("Fonts are packed into one atlas with a white block; text is measured and wrapped (S3.1)")
{
    ui::FontAtlas atlas;
    std::vector<core::Diagnostic> diagnostics;
    assets::AssetFolders plugin(UMBRA_RPG_UI_ASSETS);
    std::ifstream in(plugin / "ui/fonts/Poppins-Regular.ttf", std::ios::binary);
    const std::vector<unsigned char> ttf{std::istreambuf_iterator<char>(in), {}};
    REQUIRE(atlas.Build({{"body", ttf, 18.0F}, {"big", ttf, 32.0F}}, diagnostics));
    const ui::Face *body = atlas.Find("body");
    REQUIRE(body != nullptr);
    CHECK(body->Find(U'A') != nullptr);
    CHECK(body->Find(U'\u1EA1') != nullptr); // Vietnamese
    const glm::vec2 white = atlas.WhiteUv();
    const auto x = static_cast<std::size_t>(white.x * static_cast<float>(atlas.Image().width));
    const auto y = static_cast<std::size_t>(white.y * static_cast<float>(atlas.Image().height));
    CHECK(atlas.Image().rgba[(y * atlas.Image().width + x) * 4 + 3] == 255);

    const glm::vec2 one = ui::FontAtlas::Measure(*body, "Hello world");
    CHECK(one.x > 50.0F);
    CHECK(one.y == body->lineHeight);
    CHECK(ui::FontAtlas::Lines(*body, "Hello world again", one.x * 0.6F).size() >= 2);
    CHECK(ui::FontAtlas::Measure(*atlas.Find("big"), "Hello world").x > one.x);

    std::vector<core::Diagnostic> bad;
    ui::FontAtlas broken;
    CHECK_FALSE(broken.Build({{"x", {1, 2, 3}, 18.0F}}, bad));
    CHECK(core::HasErrors(bad));
}

TEST_CASE("Screen files are checked; problems name the field (S3.2)")
{
    ui::Screen screen;
    std::vector<core::Diagnostic> diagnostics;
    CHECK(ui::ParseScreen(Menu(), "menu", "ui/menu.ui.json", screen, diagnostics));
    CHECK(screen.modal);
    CHECK(screen.root.children[0].children[1].entry != nullptr);

    json bad = json::parse(R"({"format": "umbra.ui", "root": {"type": "panel", "children": [
        {"type": "slider"}, {"type": "text", "anchor": "middle"}, {"type": "list"}]}})");
    diagnostics.clear();
    CHECK_FALSE(ui::ParseScreen(bad, "bad", "ui/bad.ui.json", screen, diagnostics));
    const auto has = [&](const std::string &field) {
        return std::any_of(diagnostics.begin(), diagnostics.end(), [&](const core::Diagnostic &d) { return d.field == field; });
    };
    CHECK(has("root.children[0].type"));
    CHECK(has("root.children[1].anchor"));
    CHECK(has("root.children[2].items"));
    CHECK(has("root.children[2].entry"));
}

TEST_CASE("Screens bind the game's data, draw it, and turn input into actions (S3.2, S3.3)")
{
    ui::UiSystem uiSystem;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(uiSystem.Load(TestProject("menu", {{"menu", Menu()}}), diagnostics));
    REQUIRE(uiSystem.Open("menu"));
    CHECK(uiSystem.TakesInput());
    const json data = {{"hero", {{"health", 7}, {"maxHealth", 10}}},
                       {"menu", {{{"name", "Attack"}}, {{"name", "Magic"}, {"locked", true}}, {{"name", "Flee"}}}}};
    core::UiDrawList draw;

    // 1. First frame: drawn (dim + panel + text), nothing chosen yet.
    CHECK(uiSystem.Update(data, {1280, 720}, {}, draw).empty());
    CHECK(!draw.vertices.empty());
    CHECK(draw.batches.size() >= 1);
    CHECK(draw.indices.size() % 6 == 0);

    // 2. Keyboard: the locked entry is skipped; down, then confirm -> "choose" with that entry's index.
    ui::UiInput down;
    down.down = true;
    draw.Clear();
    uiSystem.Update(data, {1280, 720}, down, draw);
    ui::UiInput confirm;
    confirm.confirm = true;
    draw.Clear();
    auto actions = uiSystem.Update(data, {1280, 720}, confirm, draw);
    REQUIRE(actions.size() == 1);
    CHECK(actions[0].action == "choose");
    CHECK(actions[0].arg == "2"); // Attack(0), [Magic locked], Flee(2)

    // 3. Down again reaches the button; wrapping around comes back to the first entry.
    draw.Clear();
    uiSystem.Update(data, {1280, 720}, down, draw);
    draw.Clear();
    actions = uiSystem.Update(data, {1280, 720}, confirm, draw);
    REQUIRE(actions.size() == 1);
    CHECK(actions[0].action == "resume");

    // 4. Back runs the screen's "back" action; "close" closes it.
    ui::UiInput back;
    back.back = true;
    draw.Clear();
    actions = uiSystem.Update(data, {1280, 720}, back, draw);
    REQUIRE(actions.size() == 1);
    CHECK(actions[0].action == "close");
    CHECK_FALSE(uiSystem.IsOpen("menu"));
    CHECK_FALSE(uiSystem.TakesInput());
}

TEST_CASE("The reference game's screens and skin load (S3.4)")
{
    ui::UiSystem uiSystem;
    std::vector<core::Diagnostic> diagnostics;
    const assets::AssetFolders folders(UMBRA_RPG_UI_ASSETS);
    REQUIRE(uiSystem.Load(folders, diagnostics));
    CHECK(diagnostics.empty());

    // Every screen draws something with the sample data (what the editor and ui-preview show).
    std::ifstream in(folders / "ui/sample.json");
    const json sample = json::parse(in, nullptr, false);
    REQUIRE(sample.is_object());
    for (const auto &[name, screen] : uiSystem.Screens())
    {
        INFO(name);
        uiSystem.Open(name);
        core::UiDrawList draw;
        uiSystem.Update(sample, {1280, 720}, {}, draw);
        CHECK(draw.indices.size() > 30);
        uiSystem.Close(name);
    }
}

TEST_CASE("Rasterize draws textured, blended, clipped triangles (UI.md Raster)")
{
    // One white quad, half transparent red, clipped to its left half.
    core::UiDrawList list;
    const std::uint32_t red = 0x800000FFU; // ImGui order: A B G R from the top byte
    list.vertices = {{{0, 0}, {0, 0}, red}, {{8, 0}, {1, 0}, red}, {{8, 4}, {1, 1}, red}, {{0, 4}, {0, 1}, red}};
    list.indices = {0, 1, 2, 0, 2, 3};
    list.batches.push_back({core::TextureHandle{}, {0, 0, 4, 4}, 0, 0, 6});
    const core::TextureData image = ui::Rasterize(list, 8, 4, nullptr, {0, 0, 1, 1});
    const auto pixel = [&](int x, int y) { return &image.rgba[static_cast<std::size_t>(y * 8 + x) * 4]; };
    CHECK(pixel(1, 1)[0] > 120); // red over blue, about half each
    CHECK(pixel(1, 1)[2] > 120);
    CHECK(pixel(6, 1)[0] == 0);  // outside the clip: still the background
    CHECK(pixel(6, 1)[2] == 255);
}

TEST_CASE("Inventory and skill screens send the game's actions (S3.5)")
{
    // Walk the focus through a screen with the sample data and collect what each confirm sends.
    ui::UiSystem uiSystem;
    std::vector<core::Diagnostic> diagnostics;
    const assets::AssetFolders folders(UMBRA_RPG_UI_ASSETS);
    REQUIRE(uiSystem.Load(folders, diagnostics));
    std::ifstream in(folders / "ui/sample.json");
    const json sample = json::parse(in, nullptr, false);
    const auto choices = [&](const std::string &screen) {
        std::set<std::string> seen;
        uiSystem.Open(screen);
        uiSystem.ResetFocus(screen);
        core::UiDrawList draw;
        uiSystem.Update(sample, {1280, 720}, {}, draw); // lay out once
        for (int step = 0; step < 12; ++step)
        {
            ui::UiInput confirm;
            confirm.confirm = true;
            draw.Clear();
            for (const ui::UiAction &action : uiSystem.Update(sample, {1280, 720}, confirm, draw))
                seen.insert(action.action + " " + action.arg);
            ui::UiInput next;
            next.right = true; // grids move right, then on to the next row and list
            draw.Clear();
            uiSystem.Update(sample, {1280, 720}, next, draw);
        }
        uiSystem.Close(screen);
        return seen;
    };
    const std::set<std::string> inventory = choices("inventory");
    CHECK(inventory.contains("unequip weapon"));
    CHECK(inventory.contains("equip iron_sword"));
    CHECK(inventory.contains("equip leather_armor"));
    CHECK_FALSE(inventory.contains("equip healing_potion")); // cannotEquip: disabled, never focused
    const std::set<std::string> skills = choices("skills");
    CHECK(skills.contains("unlock fireball"));
    CHECK_FALSE(skills.contains("unlock meteor")); // locked
}

TEST_CASE("Turn menu: Attack -> move -> target arrow at its point; hotkeys screen (S4 UI)")
{
    ui::UiSystem uiSystem;
    std::vector<core::Diagnostic> diagnostics;
    const assets::AssetFolders folders(UMBRA_RPG_UI_ASSETS);
    REQUIRE(uiSystem.Load(folders, diagnostics));
    std::ifstream in(folders / "ui/sample.json");
    const json sample = json::parse(in, nullptr, false);
    core::UiDrawList draw;
    // One frame of input on a screen (after one frame of layout); returns "action arg" strings.
    const auto press = [&](const std::string &screen, const ui::UiInput &input) {
        uiSystem.Open(screen);
        uiSystem.ResetFocus(screen);
        draw.Clear();
        uiSystem.Update(sample, {1280, 720}, {}, draw);
        draw.Clear();
        std::vector<std::string> out;
        for (const ui::UiAction &a : uiSystem.Update(sample, {1280, 720}, input, draw))
            out.push_back(a.action + " " + a.arg);
        uiSystem.Close(screen);
        return out;
    };
    ui::UiInput confirm;
    confirm.confirm = true;
    CHECK(press("fight", confirm) == std::vector<std::string>{"attack "});
    CHECK(press("fight-moves", confirm) == std::vector<std::string>{"move strike"});
    ui::UiInput back;
    back.back = true;
    CHECK(press("fight-target", back) == std::vector<std::string>{"back "});

    // The second arrow stands above its point (760, 320): a click just above the point picks it.
    ui::UiInput click;
    click.mouse = {760.0F, 305.0F};
    click.mouseMoved = click.click = true;
    CHECK(press("fight-target", click) == std::vector<std::string>{"target 2"});

    // Hotkeys: the first choice is key 1.
    CHECK(press("hotkeys", confirm) == std::vector<std::string>{"slot 0"});
}

TEST_CASE("A click goes to the topmost screen with a choice under it; a dimmed screen blocks those below (UI.md)")
{
    const auto screen = [](bool modal, bool dim, const char *anchor, const char *action) {
        return json{{"format", "umbra.ui"}, {"version", 1}, {"modal", modal}, {"dim", dim},
                    {"root", {{"type", "button"}, {"anchor", anchor}, {"size", {200, 50}}, {"text", action}, {"action", action}}}};
    };
    ui::UiSystem uiSystem;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(uiSystem.Load(TestProject("click", {{"hud", screen(false, false, "top-left", "focus")}, {"menu", screen(true, false, "center", "move")},
                                                 {"dialog", screen(true, true, "bottom", "roll")}}),
                          diagnostics));
    core::UiDrawList draw;
    const auto press = [&](glm::vec2 at) {
        ui::UiInput click;
        click.mouse = at;
        click.mouseMoved = click.click = true;
        draw.Clear();
        uiSystem.Update({}, {1280, 720}, {}, draw); // lay out first
        draw.Clear();
        std::vector<std::string> got;
        for (const ui::UiAction &a : uiSystem.Update({}, {1280, 720}, click, draw))
            got.push_back(a.screen + " " + a.action);
        return got;
    };
    // 1. No modal screen: the HUD's button takes the click.
    REQUIRE(uiSystem.Open("hud"));
    CHECK(press({100, 25}) == std::vector<std::string>{"hud focus"});
    // 2. A modal menu on top takes the keys, but a click on the HUD still reaches it; the menu's own works.
    REQUIRE(uiSystem.Open("menu"));
    CHECK(press({100, 25}) == std::vector<std::string>{"hud focus"});
    CHECK(press({640, 360}) == std::vector<std::string>{"menu move"});
    ui::UiInput confirm;
    confirm.confirm = true;
    draw.Clear();
    CHECK(uiSystem.Update({}, {1280, 720}, confirm, draw).at(0).action == "move");
    // 3. A dimmed dialog blocks every click below it.
    REQUIRE(uiSystem.Open("dialog"));
    CHECK(press({100, 25}).empty());
    CHECK(press({640, 695}) == std::vector<std::string>{"dialog roll"});
}

TEST_CASE("A panel or stack with an action is clicked anywhere in it; its own buttons still win; keys skip it (UI.md)")
{
    const json box = {{"format", "umbra.ui"}, {"version", 1}, {"modal", true},
                      {"root", {{"type", "stack"}, {"anchor", "center"}, {"size", {400, 200}}, {"action", "next"},
                                {"children", json::array({{{"type", "text"}, {"text", "A line"}},
                                                          {{"type", "button"}, {"size", {120, 40}}, {"text", "Skip"}, {"action", "skip"}}})}}}};
    ui::UiSystem uiSystem;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(uiSystem.Load(TestProject("box", {{"caption", box}}), diagnostics));
    REQUIRE(uiSystem.Open("caption"));
    core::UiDrawList draw;
    const auto send = [&](const ui::UiInput &input) {
        draw.Clear();
        uiSystem.Update({}, {1280, 720}, {}, draw); // lay out first
        draw.Clear();
        std::vector<std::string> got;
        for (const ui::UiAction &a : uiSystem.Update({}, {1280, 720}, input, draw))
            got.push_back(a.action);
        return got;
    };
    ui::UiInput click;
    click.mouseMoved = click.click = true;
    // 1. Anywhere in the box (not on the button): the box's action.
    click.mouse = {800, 440};
    CHECK(send(click) == std::vector<std::string>{"next"});
    // 2. Outside it: nothing.
    click.mouse = {50, 50};
    CHECK(send(click).empty());
    // 3. On its button: the button's (the button sits at the stack's top left, under its line).
    click.mouse = {500, 300};
    const std::vector<std::string> onButton = send(click);
    CHECK(onButton == std::vector<std::string>{"skip"});
    // 4. Confirm goes to the focused button, never to the box.
    ui::UiInput confirm;
    confirm.confirm = true;
    CHECK(send(confirm) == std::vector<std::string>{"skip"});
}
