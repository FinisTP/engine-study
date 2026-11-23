#include "CafeHubApp.hpp"

#include "Analog.hpp"
#include "CafeHub.hpp"
#include "DialogApp.hpp"
#include "GameContext.hpp"
#include "NotebookView.hpp"

#include <Log.hpp>
#include <Options.hpp>
#include <TextureTools.hpp>

#include <chrono>
#include <fstream>
#include <thread>

int RunCafeHub(GameContext &game, const umbra::assets::AssetFolders &assets, umbra::campaign::Save &save, const umbra::campaign::Speaker &player,
               const std::filesystem::path &capture, int frames)
{
    namespace campaign = umbra::campaign;
    // 1. The data: rules, cast, recipes, memories, talks, the stages (where things are found), words.
    const std::filesystem::path folder = assets.RootOf("campaign/cast.csv") / "campaign";
    umbra::match::MatchDef def;
    campaign::Cast cast;
    campaign::Recipes recipes;
    campaign::Memories memories;
    campaign::Talks talks;
    std::vector<campaign::StageDef> stages;
    std::vector<umbra::core::Diagnostic> d;
    bool loaded = umbra::match::LoadMatch(assets.RootOf("match/rules.json") / "match", def, d) && campaign::LoadCast(folder / "cast.csv", cast, d) &&
                  campaign::LoadRecipes(folder / "recipes.json", def, recipes, d) && campaign::LoadMemories(folder / "memories.json", folder / "scenes", recipes, def, memories, d) &&
                  campaign::LoadTalks(folder / "talks", folder / "scenes", cast, talks, d);
    for (const auto &f : std::filesystem::directory_iterator(folder / "stages"))
        if (campaign::StageDef s; loaded && campaign::LoadStage(f.path(), cast, def, folder / "scenes", s, d))
            stages.push_back(s);
    nlohmann::json words;
    if (std::ifstream in(assets / "campaign-words.json"); in)
        words = nlohmann::json::parse(in, nullptr, false);
    umbra::night::GameOptions options;
    if (!umbra::night::LoadOptions(assets / "options.csv", options, d))
        options = {};
    for (const umbra::core::Diagnostic &e : d)
        umbra::diagnostics::Error(umbra::core::Format(e));
    Screens &screens = *game.screens;
    if (!loaded || words.is_discarded() || !screens.Ready())
        return 0;
    const umbra::night::AnalogScreen analog = umbra::night::LoadAnalog(umbra::night::AnalogPath(), options);
    screens.SetTextScale(analog.TextScale());

    // 2. The loop: the hub, the brew screen over it, scenes between.
    CafeHub hub(def, cast, recipes, memories, talks, stages, save, static_cast<int>(options.boostCapPerStat));
    Notebook book(def, cast, memories, save);
    screens.Ui().Open("cafe-hub");
    bool brewing = false;
    std::string picked; // a general drink waiting for whom
    umbra::core::DrawList drawList;
    umbra::core::FrameClock &clock = game.clock;
    const int lastFrame = frames > 0 ? frames : (capture.empty() ? 0 : 3);
    int frame = 0, result = 0;
    bool captured = false, done = false;
    const auto play = [&](const std::string &scene) {
        if (scene.empty())
            return;
        campaign::Facts chosen;
        screens.Ui().Close("cafe-hub");
        if (RunScene(game, assets, scene, "", player, chosen) == 0)
            done = true;
        hub.SceneDone(chosen);
        screens.Ui().Open("cafe-hub");
    };
    while (!done)
    {
        game.input.BeginFrame();
        game.events.BeginFrame();
        game.pump.PumpEvents(game.input, game.events);
        using umbra::platform::ApplicationEventType;
        if (game.events.IsEventRequested(ApplicationEventType::Quit) || game.events.IsEventRequested(ApplicationEventType::WindowClose))
            break;
        if (game.events.IsEventRequested(ApplicationEventType::WindowResize))
            game.window.UpdateSizeFromEvent(game.events.GetWindowWidth(), game.events.GetWindowHeight()), game.renderer.RequestResize();
        clock.Tick();
        const auto dt = static_cast<float>(clock.GetDeltaTimeSeconds());
        hub.Update(dt);
        drawList.Clear();
        drawList.hud.diagnosticsWindow = false;
        const auto [width, height] = game.renderer.Extent();
        const glm::vec2 pixels{static_cast<float>(width), static_cast<float>(height)};
        const umbra::ui::UiInput uiInput = Screens::MapInput(game.input, {static_cast<float>(game.window.GetWidth()), static_cast<float>(game.window.GetHeight())}, pixels);

        // 3. The screens' data: lack lines in words, who a general drink can go to.
        nlohmann::json data;
        data["hub"] = hub.Data();
        data["hub"]["backdrop"] = "art/cafe/back_wall.jpg";
        for (auto &r : data["hub"]["recipes"])
        {
            r["off"] = !r["ok"].get<bool>();
            for (auto &l : r["lacks"])
            {
                const auto item = std::ranges::find(def.items, l["item"].get<std::string>(), &umbra::match::ItemDef::id);
                const std::string name = words.value("items", nlohmann::json::object()).value(l["item"].get<std::string>(), item == def.items.end() ? l["item"].get<std::string>() : item->name);
                std::string text = words.value("lack", std::string());
                for (const auto &[key, value] : std::map<std::string, std::string>{{"{item}", name}, {"{n}", std::to_string(l["need"].get<int>() - l["have"].get<int>())}, {"{where}", l["where"]}})
                    if (const auto at = text.find(key); at != std::string::npos)
                        text.replace(at, key.size(), value);
                l["label"] = text;
            }
        }
        for (auto &k : data["hub"]["keepsakes"]) // item names in Vietnamese from campaign-words.json
            k["name"] = words.value("items", nlohmann::json::object()).value(k["item"].get<std::string>(), k["name"].get<std::string>());
        data["hub"]["forWhom"] = picked.empty() ? nlohmann::json::array() : data["hub"]["people"];
        data["book"] = book.Data();
        for (auto &slot : data["book"]["detail"]["slots"]) // a slot's item by its Vietnamese name
            slot["label"] = slot["item"] == "" ? words.value("empty", std::string()) : words.value("items", nlohmann::json::object()).value(slot["item"].get<std::string>(), slot["item"].get<std::string>());
        data["words"] = words;
        for (const umbra::ui::UiAction &a : screens.Ui().Update(data, pixels, uiInput, drawList.ui))
        {
            game.sounds->Play("click");
            if (a.action == "talk")
                play(hub.Talk(a.arg));
            else if (a.action == "notebook" || (a.screen == "notebook" && a.action == "close"))
            {
                if (a.action == "notebook")
                    screens.Ui().Open("notebook");
                else
                    screens.Ui().Close("notebook");
            }
            else if (a.action == "tab")
                book.Tab(a.arg);
            else if (a.action == "select")
                book.Select(a.arg);
            else if (a.action == "slot")
            {
                // The slot takes the next carriable item in storage, then empty.
                const nlohmann::json now = book.Data()["detail"]["slots"][std::stoi(a.arg)]["item"];
                std::vector<std::string> choices{""};
                for (const auto &[item, count] : save.storage)
                    if (count > 0 && campaign::SlotOk(def, item))
                        choices.push_back(item);
                const auto at = std::ranges::find(choices, now.get<std::string>());
                const std::string next = at == choices.end() || at + 1 == choices.end() ? choices.front() : *(at + 1);
                (void)book.SetSlot(book.Data()["selected"], std::stoi(a.arg), next);
            }
            else if (a.action == "give")
                play(hub.Give(a.arg));
            else if (a.action == "brew")
                brewing = true, screens.Ui().Open("brew");
            else if (a.action == "close")
                brewing = false, picked.clear(), screens.Ui().Close("brew");
            else if (a.action == "pick" && recipes.Find(a.arg) && recipes.Find(a.arg)->kind == "memory")
            {
                screens.Ui().Close("brew"), brewing = false;
                play(hub.Brew(a.arg, recipes.Find(a.arg)->forWho));
            }
            else if (a.action == "pick")
                picked = a.arg; // a general drink: whom next
            else if (a.action == "for" && !picked.empty())
                (void)hub.Brew(picked, a.arg), picked.clear();
            else if (a.action == "leave")
                result = 1, done = true;
        }
        if (++frame == lastFrame && !capture.empty())
            game.renderer.CaptureNextFrame([&](const umbra::core::TextureData &picture) {
                std::vector<std::uint8_t> png;
                captured = umbra::assets::EncodePng(picture, png) &&
                           static_cast<bool>(std::ofstream(capture, std::ios::binary).write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size())));
            });
        analog.Apply(drawList.post);
        if (!game.renderer.RenderFrame(dt, drawList))
            break;
        if (captured || (lastFrame > 0 && frame >= lastFrame + (capture.empty() ? 0 : 5)))
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    (void)brewing;
    screens.Ui().Close("brew"), screens.Ui().Close("notebook"), screens.Ui().Close("cafe-hub");
    return result;
}
