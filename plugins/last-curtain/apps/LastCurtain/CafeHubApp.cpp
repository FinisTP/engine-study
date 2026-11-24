#include "CafeHubApp.hpp"
#include "ItemArt.hpp"
#include "CampaignText.hpp"
#include "Localization.hpp"

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
#include <set>
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
    CampaignText text; // display text from localization.csv (Localization.md "Data")
    if (text.Load(assets / "localization.csv"))
        text.Fill(cast), text.Fill(recipes), text.Fill(memories), text.Fill(stages);
    nlohmann::json words, room, spots;
    words = umbra::night::LoadWords(assets / "localization.csv", "campaign", umbra::night::EnglishChosen());
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
    campaign::Growth growth;
    (void)campaign::LoadGrowth(folder / "growth.csv", growth, d);
    for (umbra::match::AbilityDef &a : def.abilities) // ability names in Vietnamese, as in a fight
        if (words.contains("abilities") && words["abilities"].contains(a.id))
            a.name = words["abilities"][a.id].get<std::string>();

    // 2. The room: the player and the allies walk (Tab), Le Thanh keeps the counter; each by its cast sprite.
    CafeHub hub(def, cast, recipes, memories, talks, stages, save, static_cast<int>(options.boostCapPerStat));
    // A picture's path, or "" when it is not painted yet (the screen then shows the name alone).
    const auto Icon = [&](const std::string &file) { return std::filesystem::exists(assets.RootOf(file) / file) ? file : std::string(); };
    Notebook book(def, cast, memories, save);
    campaign::Skills skills;
    const bool haveSkills = campaign::LoadSkills(folder / "skills.json", def, skills, d);
    text.Fill(skills);
    campaign::Quests quests; // the quest line (note 25): its target marked in the room
    (void)campaign::LoadQuests(folder / "quests.json", stages, cast, quests, d);
    text.Fill(quests);
    book.SetGrowth(&growth), book.SetWords(words), book.SetSkills(haveSkills ? &skills : nullptr);
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
    std::string useAt; // someone or a spot clicked: talk:<who> or brew/notebook/door, used once near
    std::set<std::string> given; // talks whose gifts were shown this visit
    bool lesson = false;         // the notebook shows how to fill a slot (after Le Thanh's gift)
    std::string picked; // a general drink waiting for whom
    umbra::core::DrawList drawList;
    umbra::core::FrameClock &clock = game.clock;
    const int lastFrame = frames > 0 ? frames : (capture.empty() ? 0 : 3);
    int frame = 0, result = 0;
    bool captured = false, done = false;
    const auto play = [&](const std::string &scene) {
        if (scene.empty())
            return;
        campaign::Facts chosen = save.facts; // earlier choices: lines may answer them (note 28)
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
            view.DrawCampaign(walk, sprites, "", out, at); // anyone in front of the speaker fades (note 15)
        };
        const campaign::Talk *talk = hub.LastTalk();
        const bool first = talk && talk->scene == scene && !talk->gives.empty() && given.insert(talk->id).second;
        if (RunScene(game, assets, scene, "", player, chosen, {}, 0, room) == 0)
            done = true;
        hub.SceneDone(chosen);
        screens.Ui().Open("cafe-hub");
        // A gift: a toast per item, then (when the talk says so) the notebook opens with the slot lesson.
        if (first)
            for (const auto &[item, count] : talk->gives)
                hub.Toast(words.value("received", std::string()) + " " + words["items"].value(item, item) + " x" + std::to_string(count));
        if (first && talk->then == "notebook")
            screens.Ui().Open("notebook"), lesson = true;
        if (talk && talk->scene == scene && talk->then == "brew" && given.insert(talk->id).second) // the brewing lesson (note 13)
            brewing = true, screens.Ui().Open("brew");
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
        if (!useAt.empty() && near == useAt) // walked up to what was clicked: as if E was pressed
            act = "use", useAt.clear();
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
        for (auto &g : data["book"]["detail"]["gear"]) // worn gear by its Vietnamese name and picture (note 30)
        {
            g["label"] = g["item"] == "" ? words.value("empty", std::string()) : words.value("items", nlohmann::json::object()).value(g["item"].get<std::string>(), g["item"].get<std::string>());
            g["icon"] = g["item"] == "" ? "" : Icon(ItemPicture(def, g["item"].get<std::string>()));
        }
        for (auto &slot : data["book"]["detail"]["slots"]) // a slot's item by its Vietnamese name
        {
            slot["label"] = slot["item"] == "" ? words.value("empty", std::string()) : words.value("items", nlohmann::json::object()).value(slot["item"].get<std::string>(), slot["item"].get<std::string>());
            slot["icon"] = slot["item"] == "" ? "" : Icon(ItemPicture(def, slot["item"].get<std::string>()));
        }
        // Storage and the card: items by their Vietnamese names, icons and texts.
        const nlohmann::json itemNames = words.value("items", nlohmann::json::object()), itemTexts = words.value("itemText", nlohmann::json::object());
        if (nlohmann::json &page = data["book"]["detail"]; page.is_object())
        {
            for (auto &it : page["store"])
                it["name"] = itemNames.value(it["id"].get<std::string>(), it["id"].get<std::string>()) + "  \xC3\x97" + std::to_string(it["count"].get<int>()),
                it["icon"] = Icon(ItemPicture(def, it["id"].get<std::string>()));
            if (page.contains("card") && page["card"].value("kind", "") == "item")
            {
                const std::string id = page["card"]["id"];
                page["card"]["name"] = itemNames.value(id, page["card"]["name"].get<std::string>());
                page["card"]["text"] = itemTexts.value(id, page["card"]["text"].get<std::string>());
            }
            std::string title = words.value("carried", std::string());
            if (const auto at = title.find("{n}"); at != std::string::npos)
                title.replace(at, 3, std::to_string(std::ranges::count_if(page["loadout"], [](const nlohmann::json &x) { return !x["empty"].get<bool>(); })));
            page["carriedTitle"] = title;
        }
        data["book"]["lesson"] = lesson ? words.value("slotLesson", std::string()) : "";
        if (nlohmann::json &page = data["book"]["detail"]; page.contains("stats")) // the page's lines in words
        {
            std::string stats = words.value("statsLine", std::string());
            for (const char *k : {"hp", "attack", "defence", "speed", "move"})
                if (const std::string key = std::string("{") + k + "}"; stats.find(key) != std::string::npos)
                    stats.replace(stats.find(key), key.size(), std::to_string(page["stats"][k].get<int>()));
            std::string level = words.value("levelShort", std::string());
            if (const auto at = level.find("{n}"); at != std::string::npos)
                level.replace(at, 3, std::to_string(page["level"].get<int>()));
            page["statsText"] = stats, page["levelText"] = level;
            page["fightText"] = words.value(page["fights"].get<bool>() ? "fightsNext" : "staysNext", std::string());
        }
        data["words"] = words;
        data["room"]["who"] = walk.Active() == "player" ? campaign::FillTokens("{player}", player, cast) : cast.Find(walk.Active()) ? cast.Find(walk.Active())->name : walk.Active();
        const campaign::Quest *quest = campaign::CurrentQuest(quests, save);
        data["hub"]["quest"] = quest ? quest->text : "";
        data["room"]["labels"] = nlohmann::json::array();
        for (const nlohmann::json &p : data["hub"]["people"])
            for (const CampaignRoom::Person &person : walk.People())
                if (person.who == p["who"].get<std::string>())
                    if (const auto at = view.Above(person.at, 2.05F, pixels))
                        data["room"]["labels"].push_back({{"at", {at->x, at->y}}, {"who", person.who}, {"name", p["name"]}, {"talk", p["talk"]}, {"near", near == "talk:" + person.who},
                                                                  {"quest", quest && quest->talk == person.who}});
        data["room"]["spots"] = nlohmann::json::array(); // the door, the counter and the notebook, labelled to click
        for (const CampaignRoom::Spot &spot : walk.Spots())
            if (const auto at = view.Above(spot.at, 1.4F, pixels))
                data["room"]["spots"].push_back({{"at", {at->x, at->y}}, {"kind", spot.kind}, {"name", words.value(spot.kind, spot.kind)},
                                                         {"quest", quest && ((quest->door && spot.kind == "door") || (quest->brew && spot.kind == "brew"))}});
        data["room"]["prompt"] = near.empty() ? "" : words.value("press", std::string()) + " " +
                                                      (near.starts_with("talk:") ? words.value("talkTo", std::string()) : words.value(near, std::string()));
        bool uiTook = false;
        game.Keep(drawList); // the place, for a screen opened from it (settings, the result)
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
                    screens.Ui().Close("notebook"), lesson = false;
            }
            else if (a.action == "tab")
                book.Tab(a.arg);
            else if (a.action == "select")
                book.Select(a.arg);
            else if (a.action == "skill")
                book.Skill(a.arg);
            // Who fights is chosen only before each stage (prestage), not here (note 27).
            else if (a.action == "pick") // pick then place: a slot or something in stock shows its card
                book.Pick(a.arg);
            else if (a.action == "take")
                (void)book.Take();
            else if (a.action == "drop")
                (void)book.Drop();
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
            else if (a.action == "leave" || (a.action == "use" && a.arg == "door"))
                result = 1, done = true;
            else if (a.action == "use" && a.arg == "brew")
                brewing = true, screens.Ui().Open("brew");
            else if (a.action == "use" && a.arg == "notebook")
                screens.Ui().Open("notebook");
            else if (a.action == "use")
                act = "use"; // the prompt clicked: as if E was pressed
        }
        // 4. E at a person talks (a toast when they have nothing new), at the counter brews, at the board
        // opens the notebook, at the door leaves; Tab hands the walk to the next ally.
        if (act == "switch")
            walk.Switch(), game.sounds->Play("click");
        else if (act == "use" && near.starts_with("talk:"))
        {
            const std::string who = near.substr(5), scene = hub.Talk(who);
            if (scene.empty())
                hub.Toast((cast.Find(who) ? campaign::FillTokens(cast.Find(who)->name, player, cast) : who) + " " + words.value("noTalk", std::string()));
            play(scene);
        }
        else if (act == "use" && near == "brew")
            brewing = true, screens.Ui().Open("brew");
        else if (act == "use" && near == "notebook")
            screens.Ui().Open("notebook");
        else if (act == "use" && near == "door")
            result = 1, done = true;
        // A click on someone walks up to them and talks (once near); elsewhere on the floor, walks there.
        if (uiInput.click && !uiTook && free)
        {
            std::string clicked;
            for (const CampaignRoom::Person &p : walk.People())
            {
                const auto feet = view.Above(p.at, 0.0F, pixels), head = view.Above(p.at, 1.9F, pixels);
                if (feet && head && std::abs(uiInput.mouse.x - feet->x) < (feet->y - head->y) * 0.3F && uiInput.mouse.y > head->y && uiInput.mouse.y < feet->y)
                    clicked = p.who, walk.WalkTo(p.at);
            }
            if (!clicked.empty())
                useAt = "talk:" + clicked;
            else if (const auto at = view.Floor(uiInput.mouse, pixels, roomSize))
            {
                const auto spot = walk.SpotAt(*at); // near the door, the counter or the notebook: walk there and use it
                walk.WalkTo(spot ? spot->at : *at), useAt = spot ? spot->kind : "";
            }
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
    screens.Ui().Close("brew"), screens.Ui().Close("notebook"), screens.Ui().Close("cafe-hub");
    return result;
}
