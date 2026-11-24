#include "CafeHubApp.hpp"

#include "Analog.hpp"
#include "CafeHub.hpp"
#include "CafeRoom.hpp"
#include "CampaignRoom.hpp"
#include "DialogApp.hpp"
#include "Flow.hpp"
#include "GameContext.hpp"
#include "NotebookView.hpp"

#include <Log.hpp>
#include <Options.hpp>
#include <TextureTools.hpp>

#include <chrono>
#include <fstream>
#include <thread>

using umbra::platform::InputKey;

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
    nlohmann::json words, room, spots;
    if (std::ifstream in(assets / "campaign-words.json"); in)
        words = nlohmann::json::parse(in, nullptr, false);
    if (std::ifstream in(assets.RootOf("cafe/room.json") / "cafe/room.json"); in)
        room = nlohmann::json::parse(in, nullptr, false);
    if (std::ifstream in(folder / "cafe-room.json"); in)
        spots = nlohmann::json::parse(in, nullptr, false);
    umbra::night::GameOptions options;
    if (!umbra::night::LoadOptions(assets / "options.csv", options, d))
        options = {};
    for (const umbra::core::Diagnostic &e : d)
        umbra::diagnostics::Error(umbra::core::Format(e));
    Screens &screens = *game.screens;
    if (!loaded || words.is_discarded() || room.is_discarded() || spots.is_discarded() || !screens.Ready())
        return 0;
    umbra::night::AnalogScreen analog = umbra::night::LoadAnalog(umbra::night::AnalogPath(), options);
    game.Apply(analog);

    // 2. The room: the player and the allies walk (Tab), Le Thanh keeps the counter; each by its cast sprite.
    CafeHub hub(def, cast, recipes, memories, talks, stages, save, static_cast<int>(options.boostCapPerStat));
    Notebook book(def, cast, memories, save);
    std::vector<std::string> party{"player"};
    std::map<std::string, std::string> sprites{{"player", player.gender == "male" ? "player_male" : "player_female"}};
    const nlohmann::json people = hub.Data()["people"]; // kept: a temporary's items would dangle
    for (const nlohmann::json &p : people)
    {
        const std::string who = p["who"];
        if (who != "lethanh")
            party.push_back(who);
        if (const campaign::CastMember *m = cast.Find(who); m && !m->sprite.empty())
            sprites[who] = m->sprite;
    }
    room["campaign"] = spots;
    CampaignRoom walk(room, party, {"lethanh"});
    const umbra::cafe::CafeDef noGuests; // the campaign's café has no ghost guests
    CafeRoom view(game.renderer, assets, noGuests, def);
    for (const auto &[who, sprite] : sprites)
        view.Load(sprite);
    const glm::vec2 roomSize{room["size"][0].get<float>(), room["size"][1].get<float>()};
    screens.Ui().Open("cafe-hub");
    game.sounds->Music("cafe");
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
        // The talk plays in the room itself: the camera close on whoever speaks (no backdrop).
        const SceneBehind room = [&](umbra::core::DrawList &out, float dt, const std::string &speaker) {
            glm::vec2 at = walk.Position();
            const nlohmann::json people = hub.Data()["people"]; // kept: a temporary's items would dangle
            for (const nlohmann::json &p : people)
                if (p.value("name", "") == speaker)
                    for (const CampaignRoom::Person &person : walk.People())
                        if (person.who == p["who"].get<std::string>())
                            at = person.at;
            view.Look(at, 1.0F, dt);
            view.DrawCampaign(walk, sprites, "", out);
        };
        if (RunScene(game, assets, scene, "", player, chosen, {}, 0, room) == 0)
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
        umbra::ui::UiInput uiInput = Screens::MapInput(game.input, {static_cast<float>(game.window.GetWidth()), static_cast<float>(game.window.GetHeight())}, pixels);

        // 3. Walking (WASD, the arrows, a click on the floor), E and Tab, while no screen is over the room.
        const bool free = !brewing && !screens.Ui().IsOpen("notebook");
        std::string act; // what E (or the HUD's button) asks for this frame
        if (free)
        {
            const auto held = [&](InputKey a, InputKey b) { return game.input.IsKeyHeld(a) || game.input.IsKeyHeld(b); };
            // The left stick walks too (P15); A uses what is near, Y switches who walks.
            const glm::vec2 stick{game.input.Axis(0), game.input.Axis(1)};
            walk.Move(glm::vec2{(held(InputKey::D, InputKey::Right) ? 1.0F : 0.0F) - (held(InputKey::A, InputKey::Left) ? 1.0F : 0.0F),
                                (held(InputKey::S, InputKey::Down) ? 1.0F : 0.0F) - (held(InputKey::W, InputKey::Up) ? 1.0F : 0.0F)} +
                          stick,
                      dt);
            if (game.input.IsKeyPressed(InputKey::E) || game.input.IsKeyPressed(InputKey::Space) || game.input.IsKeyPressed(InputKey::PadA))
                act = "use", uiInput.confirm = false; // A walks up to things here, not the HUD's focus
            if (game.input.IsKeyPressed(InputKey::Tab) || game.input.IsKeyPressed(InputKey::PadY))
                act = "switch";
        }
        walk.Update(dt);
        view.Follow(walk.Position(), roomSize, dt);
        const std::string near = walk.Interact();
        view.DrawCampaign(walk, sprites, near, drawList);

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
        data["room"]["who"] = walk.Active() == "player" ? campaign::FillTokens("{player}", player, cast) : cast.Find(walk.Active()) ? cast.Find(walk.Active())->name : walk.Active();
        data["room"]["labels"] = nlohmann::json::array();
        for (const nlohmann::json &p : data["hub"]["people"])
            for (const CampaignRoom::Person &person : walk.People())
                if (person.who == p["who"].get<std::string>())
                    if (const auto at = view.Above(person.at, 2.05F, pixels))
                        data["room"]["labels"].push_back({{"at", {at->x, at->y}}, {"who", person.who}, {"name", p["name"]}, {"talk", p["talk"]}, {"near", near == "talk:" + person.who}});
        data["room"]["prompt"] = near.empty() ? "" : words.value("press", std::string()) + " " +
                                                      (near.starts_with("talk:") ? words.value("talkTo", std::string()) : words.value(near, std::string()));
        bool uiTook = false;
        for (const umbra::ui::UiAction &a : screens.Ui().Update(data, pixels, uiInput, drawList.ui))
        {
            game.sounds->Play("click");
            uiTook = true;
            if (a.action == "switch")
                act = "switch";
            else if (a.action == "settings")
                analog = RunSettings(game, words);
            else if (a.action == "talk")
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
        // 4. E at a person talks (a toast when they have nothing new), at the counter brews, at the board
        // opens the notebook, at the door leaves; Tab hands the walk to the next ally.
        if (act == "switch")
            walk.Switch(), game.sounds->Play("click");
        else if (act == "use" && near.starts_with("talk:"))
        {
            const std::string who = near.substr(5), scene = hub.Talk(who);
            if (scene.empty())
                hub.Toast((cast.Find(who) ? cast.Find(who)->name : who) + " " + words.value("noTalk", std::string()));
            play(scene);
        }
        else if (act == "use" && near == "brew")
            brewing = true, screens.Ui().Open("brew");
        else if (act == "use" && near == "notebook")
            screens.Ui().Open("notebook");
        else if (act == "use" && near == "door")
            result = 1, done = true;
        if (uiInput.click && !uiTook && free)
            if (const auto at = view.Floor(uiInput.mouse, pixels, roomSize))
                walk.WalkTo(*at);
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
    screens.Ui().Close("brew"), screens.Ui().Close("notebook"), screens.Ui().Close("cafe-hub");
    return result;
}
