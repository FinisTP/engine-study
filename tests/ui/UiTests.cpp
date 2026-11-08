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
    CHECK(body->Find(U'ạ') != nullptr); // Vietnamese
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
