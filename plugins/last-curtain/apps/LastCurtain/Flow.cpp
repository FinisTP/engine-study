#include "Flow.hpp"
#include "CampaignText.hpp"
#include "Localization.hpp"

#include "Analog.hpp"
#include "CafeHubApp.hpp"
#include "DialogApp.hpp"
#include "GameContext.hpp"
#include "StageApp.hpp"
#include "Typing.hpp"

#include <CampaignCafe.hpp>
#include <Log.hpp>
#include <Options.hpp>
#include <TextureTools.hpp>

#include <chrono>
#include <fstream>
#include <set>
#include <thread>

std::pair<std::string, std::string> RunScreen(GameContext &game, const std::string &screen, nlohmann::json data,
                                              const std::function<bool(const std::string &, const std::string &, nlohmann::json &)> &handle,
                                              const std::filesystem::path &capture, int frames, const std::function<void(nlohmann::json &)> &each)
{
    Screens &screens = *game.screens;
    screens.Ui().Open(screen), screens.Ui().ResetFocus(screen);
    // The place's music (sounds.csv "music:<place>"); settings, the keyboard and new game keep what plays.
    if (const std::string place = screen == "worldmap" || screen == "prestage" ? "map" : screen; place == "title" || place == "map" || place == "result" || place == "gameover")
        game.sounds->Music(place);
    std::pair<std::string, std::string> picked{"quit", ""};
    umbra::core::DrawList drawList;
    std::vector<umbra::core::Diagnostic> ignored;
    umbra::night::GameOptions options;
    (void)umbra::night::LoadOptions({}, options, ignored);
    umbra::night::AnalogScreen analog = umbra::night::LoadAnalog(umbra::night::AnalogPath(), options);
    const int lastFrame = frames > 0 ? frames : (capture.empty() ? 0 : 3);
    bool captured = false, done = false;
    for (int frame = 0; !done;)
    {
        game.input.BeginFrame();
        game.events.BeginFrame();
        game.pump.PumpEvents(game.input, game.events);
        using umbra::platform::ApplicationEventType;
        if (game.events.IsEventRequested(ApplicationEventType::Quit) || game.events.IsEventRequested(ApplicationEventType::WindowClose))
            break;
        if (game.events.IsEventRequested(ApplicationEventType::WindowResize))
            game.window.UpdateSizeFromEvent(game.events.GetWindowWidth(), game.events.GetWindowHeight()), game.renderer.RequestResize();
        game.clock.Tick();
        drawList.Clear();
        if (game.hasStill)
            drawList = game.still; // the place it was opened from stays behind it
        drawList.hud.diagnosticsWindow = false;
        const auto [width, height] = game.renderer.Extent();
        const glm::vec2 pixels{static_cast<float>(width), static_cast<float>(height)};
        umbra::ui::UiInput input = Screens::MapInput(game.input, {static_cast<float>(game.window.GetWidth()), static_cast<float>(game.window.GetHeight())}, pixels);
        game.window.SetTextInput(screens.Ui().WantsText());
        if (each)
            each(data); // e.g. the map's drag and zoom
        // 1. An action ends the screen unless the handler keeps it open (a toggle, a selection).
        for (const umbra::ui::UiAction &a : screens.Ui().Update(data, pixels, input, drawList.ui))
        {
            game.sounds->Play("click");
            if (!handle || !handle(a.action, a.arg, data))
                picked = {a.action, a.arg}, done = true;
            else // a kept action may have changed the settings (the CRT switch, say)
                analog = umbra::night::LoadAnalog(umbra::night::AnalogPath(), options);
        }
        if (++frame == lastFrame && !capture.empty())
            game.renderer.CaptureNextFrame([&](const umbra::core::TextureData &picture) {
                std::vector<std::uint8_t> png;
                captured = umbra::assets::EncodePng(picture, png) &&
                           static_cast<bool>(std::ofstream(capture, std::ios::binary).write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size())));
            });
        analog.Apply(drawList.post);
        if (!game.renderer.RenderFrame(static_cast<float>(game.clock.GetDeltaTimeSeconds()), drawList))
            break;
        if (captured || (lastFrame > 0 && frame >= lastFrame + (capture.empty() ? 0 : 5)))
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    screens.Ui().Close(screen);
    return picked;
}

namespace
{
    namespace campaign = umbra::campaign;

    std::string Fill(std::string text, const std::map<std::string, std::string> &values)
    {
        for (const auto &[key, value] : values)
            for (std::size_t at; (at = text.find("{" + key + "}")) != std::string::npos;)
                text.replace(at, key.size() + 2, value);
        return text;
    }
}

std::string RunKeyboard(GameContext &game, const umbra::assets::AssetFolders &assets, const nlohmann::json &words, std::string text)
{
    // 1. The letters (typing.json); the first letter of each word comes in capitals.
    const Typing typing = Typing::Load(assets / "typing.json");
    if (!typing.Ok())
        return text;
    nlohmann::json kb{{"rows", nlohmann::json::array()}, {"tones", nlohmann::json::array()}};
    for (const std::vector<std::string> &row : typing.Keys())
    {
        nlohmann::json keys = nlohmann::json::array();
        for (const std::string &k : row)
            keys.push_back({{"label", k}});
        kb["rows"].push_back({{"keys", keys}});
    }
    const nlohmann::json tones = words.value("tones", nlohmann::json::array());
    for (std::size_t n = 0; n < tones.size(); ++n)
        kb["tones"].push_back({{"name", tones[n]}, {"n", std::to_string(n)}});
    kb["text"] = text;
    // 2. Each key adds its letter, a tone mark goes on the word's vowel; Done (or Back) keeps the text.
    (void)RunScreen(game, "keyboard", {{"words", words}, {"kb", kb}}, [&](const std::string &a, const std::string &arg, nlohmann::json &data) {
        const bool first = text.empty() || text.back() == ' ';
        if (a == "key" && text.size() < 48)
            text += first ? typing.Upper(arg) : arg;
        else if (a == "tone")
            text = typing.AddTone(text, std::atoi(arg.c_str()));
        else if (a == "space" && !first)
            text += ' ';
        else if (a == "erase")
            text = Typing::Backspace(text);
        else if (a == "key" || a == "space")
            ;
        else
            return false;
        data["kb"]["text"] = text;
        return true;
    });
    return text;
}

umbra::night::AnalogScreen RunSettings(GameContext &game, const nlohmann::json &words, bool backdrop)
{
    // 1. The settings as saved, and the keys as the player has them (read-only, remapping later).
    umbra::night::GameOptions options;
    std::vector<umbra::core::Diagnostic> ignored;
    (void)umbra::night::LoadOptions({}, options, ignored);
    umbra::night::AnalogScreen analog = umbra::night::LoadAnalog(umbra::night::AnalogPath(), options);
    nlohmann::json keys = nlohmann::json::array();
    const nlohmann::json names = words.value("keyNames", nlohmann::json::object());
    for (const char *meaning : {"confirm", "back", "next", "endTurn", "skip", "hurry", "log", "pin", "notebook"})
        keys.push_back({{"name", names.value(meaning, std::string(meaning))}, {"key", game.keys.Name(meaning)}});
    keys.push_back({{"name", names.value("menu", std::string("menu"))}, {"key", "1-6"}});
    keys.push_back({{"name", names.value("walk", std::string("walk"))}, {"key", "WASD"}});
    keys.push_back({{"name", names.value("use", std::string("use"))}, {"key", "E"}});
    keys.push_back({{"name", names.value("switch", std::string("switch"))}, {"key", "Tab"}});
    // 2. The screens under it step aside (they would read this screen's data) and come back after;
    //    from the title, the foyer painting stays behind it (no empty sky, the user 2026-11-24).
    const std::vector<std::string> under = game.screens->Ui().OpenScreens();
    for (const std::string &name : under)
        game.screens->Ui().Close(name);
    // 3. Each change applies at once (Apply) and is saved (Toggle); Close or Back leaves.
    (void)RunScreen(game, "settings", {{"words", words}, {"analog", analog.Data()}, {"keys", keys}, {"backdrop", backdrop}},
                    [&](const std::string &action, const std::string &, nlohmann::json &data) {
                        if (!analog.Toggle(action, umbra::night::AnalogPath()))
                            return false;
                        game.Apply(analog);
                        data["analog"] = analog.Data();
                        return true;
                    });
    game.screens->Ui().Close("settings");
    for (const std::string &name : under)
        game.screens->Ui().Open(name);
    return analog;
}

int RunCampaign(GameContext &game, const umbra::assets::AssetFolders &assets, const std::filesystem::path &saveFile, const std::filesystem::path &capture, int frames)
{
    // 1. The data and the words.
    const std::filesystem::path folder = assets.RootOf("campaign/cast.csv") / "campaign";
    umbra::match::MatchDef def;
    campaign::Cast cast;
    campaign::Growth growth;
    campaign::TheatreMap map;
    std::vector<campaign::StageDef> stages;
    std::vector<umbra::core::Diagnostic> d;
    bool loaded = umbra::match::LoadMatch(assets.RootOf("match/rules.json") / "match", def, d) && campaign::LoadCast(folder / "cast.csv", cast, d) &&
                  campaign::LoadGrowth(folder / "growth.csv", growth, d);
    for (const auto &f : std::filesystem::directory_iterator(folder / "stages"))
        if (campaign::StageDef s; loaded && campaign::LoadStage(f.path(), cast, def, folder / "scenes", s, d))
            stages.push_back(s);
    loaded = loaded && campaign::LoadMap(folder / "map.json", stages, map, d);
    if (CampaignText text; text.Load(assets / "localization.csv")) // display text (Localization.md "Data")
        text.Fill(cast), text.Fill(stages), text.Fill(map);
    nlohmann::json words;
    words = umbra::night::LoadWords(assets / "localization.csv", "campaign", umbra::night::EnglishChosen());
    for (const umbra::core::Diagnostic &e : d)
        umbra::diagnostics::Error(umbra::core::Format(e));
    if (!loaded || words.is_discarded() || !game.screens->Ready())
        return 1;
    const auto stageOf = [&](const std::string &id) { return std::ranges::find(stages, id, &campaign::StageDef::id); };
    const auto name = [&](const std::string &id) { return id == "player" ? std::string() : cast.Find(id) ? cast.Find(id)->name : id; };

    // 2. The title: continue (with a save), new game, settings, quit.
    campaign::Save save;
    std::string error;
    bool has = campaign::LoadCampaign(saveFile, save, error);
    if (!has && std::filesystem::exists(saveFile))
        umbra::diagnostics::Error(error);
    umbra::night::GameOptions options;
    (void)umbra::night::LoadOptions(assets / "options.csv", options, d);
    game.Apply(umbra::night::LoadAnalog(umbra::night::AnalogPath(), options)); // full screen, volume, text size as left
    for (bool started = false; !started;)
    {
        const auto [action, arg] = RunScreen(game, "title", {{"words", words}, {"canContinue", has}}, {}, capture, frames);
        if (action == "quit" || !capture.empty())
            return 0;
        if (action == "settings")
            (void)RunSettings(game, words, true);
        else if (action == "continue" && has)
            started = true;
        else if (action == "new")
        {
            // A name and a form of address, then the prologue and the first fight.
            std::string gender = "female";
            const auto [made, unused] = RunScreen(game, "newgame", {{"words", words}, {"female", true}, {"placeholder", words.value("namePlaceholder", "")}}, [&](const std::string &a, const std::string &g, nlohmann::json &data) {
                if (a == "keyboard") // a gamepad's way to type the name (P17)
                    return game.screens->Ui().SetFieldText("newgame", "name", RunKeyboard(game, assets, words, game.screens->Ui().FieldText("newgame", "name"))), true;
                if (a != "gender")
                    return false;
                gender = g, data["female"] = g == "female", data["male"] = g == "male";
                data["placeholder"] = words.value(g == "male" ? "namePlaceholderMale" : "namePlaceholder", ""); // Hoang for a man (the user)
                return true;
            });
            std::string player = game.screens->Ui().FieldText("newgame", "name");
            if (made != "start")
                continue;
            if (player.empty())
                player = gender == "male" ? words.value("namePlaceholderMale", std::string("Hoang")) : cast.Find("nameless") ? cast.Find("nameless")->name : "Linh";
            save = {};
            save.player = {player, gender};
            save.roster = {{"player", 1, 0}};
            campaign::Facts facts;
            if (RunScene(game, assets, "prologue", "foyer_today", save.player, facts) == 0)
                return 0;
            save.facts.insert(facts.begin(), facts.end());
            has = campaign::SaveCampaign(saveFile, save, error);
            started = true;
        }
    }

    // 3. One fight with its result or game over; the café and the map between.
    const auto fight = [&](const std::string &id, const std::vector<std::string> &party) -> std::string {
        const campaign::Save before = save;
        campaign::BuiltStage out;
        const int ran = RunStage(game, assets, id, save.rooms[id].refill, 0, save.player, {}, 0, &save, party, &out);
        if (ran == 4)
            return save = before, "map"; // retreat: nothing from the stage is kept
        if (ran != 3)
            return "quit";
        const campaign::StageDef &stage = *stageOf(id);
        if (out.state.winner == 0)
        {
            campaign::WinStage(def, save, stage, out, stages);
            (void)campaign::SaveCampaign(saveFile, save, error);
            const campaign::Result r = campaign::ResultOf(before, save);
            nlohmann::json lines = nlohmann::json::array();
            for (const auto &[who, n] : r.xp)
                lines.push_back({{"text", Fill(words.value("lineXp", ""), {{"name", who == "player" ? save.player.name : name(who)}, {"n", std::to_string(n)}})}});
            for (const std::string &who : r.levels)
                lines.push_back({{"text", Fill(words.value("lineLevel", ""), {{"name", who == "player" ? save.player.name : name(who)}, {"n", std::to_string(std::ranges::find(save.roster, who, &campaign::Member::id)->level)}})}});
            for (const auto &[item, n] : r.found)
                lines.push_back({{"text", Fill(words.value("lineFound", ""), {{"item", words["items"].value(item, item)}, {"n", std::to_string(n)}})}});
            for (const std::string &who : r.freed)
                lines.push_back({{"text", Fill(words.value("lineFreed", ""), {{"name", name(who)}})}});
            const auto [next, unused] = RunScreen(game, "result", {{"words", words}, {"result", {{"title", Fill(words.value("won", ""), {{"name", stage.title}})}, {"lines", lines}}}});
            return next == "quit" ? "quit" : next == "map" ? "map" : "cafe";
        }
        save = campaign::LoseStage(before, out);
        const campaign::Over o = campaign::OverFor(save, stage, out.state);
        const std::string reason = Fill(words.value(o.reason == "trapped" ? "reasonTrapped" : o.reason == "playerFell" ? "reasonPlayerFell" : "reasonSideDown", ""),
                                        {{"player", save.player.name}, {"name", name("ti")}});
        const auto [next, unused] = RunScreen(game, "gameover", {{"words", words}, {"over", {{"reasonText", reason}, {"rite", false}, {"notRetryOnly", !o.retryOnly}}}});
        return next == "retry" ? "retry" : next == "quit" ? "quit" : "map"; // a retry goes back to who fights first
    };
    std::string where = save.facts.contains("won:t1") ? "cafe" : "t1";
    std::string selected;
    campaign::MapLens lens; // the map's zoom and pan, kept between visits
    glm::vec2 lensMouse{0.0F};
    bool lensHeld = false;
    std::set<std::string> seenOpen; // tickets open when the map was last shown (world-map.md 6: new ones get a toast)
    bool mapSeen = false;
    while (where != "quit")
    {
        // Recruits who joined in the café enter the roster; Ba Mai comes after T1, Bao after T2, at level 2.
        for (const std::string &r : save.joined)
            if (std::ranges::none_of(save.roster, [&](const campaign::Member &m) { return m.id == r; }))
                save.roster.push_back({r, 3, 0});
        for (const auto &[stage, ally] : {std::pair{"won:t1", "mai"}, std::pair{"won:t2", "bao"}})
            if (save.facts.contains(stage) && std::ranges::none_of(save.roster, [&](const campaign::Member &m) { return m.id == ally; }))
                save.roster.push_back({ally, 2, 0});
        (void)campaign::SaveCampaign(saveFile, save, error);
        if (where == "t1")
            while ((where = fight("t1", {"player"})) == "retry") // the first fight has no roster to pick
            {
            }
        else if (where == "cafe")
            where = RunCafeHub(game, assets, save, save.player) == 0 ? "quit" : "map";
        else if (where == "map")
        {
            // The map: select a ticket (the panel), enter an open one, or back to the café.
            const auto view = [&](nlohmann::json &data) {
                data["map"]["tickets"] = nlohmann::json::array();
                for (const campaign::TicketView &t : campaign::MapView(map, save, stages))
                {
                    const std::string mark = words.value(t.mark == "cleared" ? "markCleared" : t.mark == "next" ? "markNext" : t.mark == "skirmish" ? "markSkirmish" : "markLocked", "?");
                    data["map"]["tickets"].push_back({{"id", t.id}, {"name", t.mark == "unknown" ? "?" : t.name}, {"markText", mark}, {"icon", "art/ui/campaign/icon/mark_" + (t.mark == "locked" ? std::string("unknown") : t.mark) + ".png"},
                                                      {"u", t.x}, {"v", t.y}, {"selected", t.id == selected}, {"closed", !t.open}});
                    if (t.id == selected)
                    {
                        const auto s = stageOf(t.id);
                        data["map"]["panel"] = {{"id", t.id}, {"name", t.name}, {"detail", t.detail}, {"open", t.open},
                                                {"level", s == stages.end() ? std::string() : Fill(words.value("levelLine", ""), {{"n", std::to_string(s->level)}})}};
                    }
                }
            };
            // The map area (worldmap.ui.json: 840 x 540 at 36, 96, in the screen's 1280 x 720 units): a drag
            // pans, the wheel zooms; the plan and each ticket are placed from the lens in window pixels.
            const glm::vec2 area{840.0F, 540.0F}, corner{36.0F, 96.0F};
            const float aspect = 1344.0F / 768.0F;
            const auto lensFrame = [&](nlohmann::json &data) {
                const float k = game.screens->Ui().UiScale();
                const auto [width, height] = game.renderer.Extent();
                const glm::vec2 scale = glm::vec2{static_cast<float>(width), static_cast<float>(height)} /
                                        glm::max(glm::vec2{static_cast<float>(game.window.GetWidth()), static_cast<float>(game.window.GetHeight())}, glm::vec2{1.0F});
                const glm::vec2 mouse = glm::vec2{game.input.GetMouseX(), game.input.GetMouseY()} * scale / k - corner; // in the area
                const bool inside = mouse.x >= 0.0F && mouse.y >= 0.0F && mouse.x < area.x && mouse.y < area.y;
                if (inside && game.input.Wheel() != 0.0F)
                    campaign::Zoom(lens, game.input.Wheel(), mouse, area.x, area.y, aspect);
                if (game.input.IsMouseButtonHeld(0) && inside && lensHeld)
                    campaign::Drag(lens, mouse - lensMouse, area.x, area.y, aspect);
                lensHeld = game.input.IsMouseButtonHeld(0) && inside, lensMouse = mouse;
                const campaign::PlanBox b = campaign::Plan(lens, area.x, area.y, aspect);
                // "at" is in window pixels: the bottom middle of what stands there.
                data["map"]["plans"] = nlohmann::json::array();
                if (!data["map"]["plan"].get<std::string>().empty())
                    data["map"]["plans"].push_back({{"file", data["map"]["plan"]}, {"size", {b.w, b.h}}, {"at", {(corner.x + b.x + b.w * 0.5F) * k, (corner.y + b.y + b.h) * k}}});
                // A ticket near the edge is kept whole inside the area (about 100 units each side of its middle).
                for (nlohmann::json &t : data["map"]["tickets"])
                    t["at"] = {(corner.x + glm::clamp(b.x + t["u"].get<float>() * b.w, 100.0F, area.x - 100.0F)) * k,
                               (corner.y + b.y + t["v"].get<float>() * b.h + 20.0F) * k};
            };
            nlohmann::json data{{"words", words}};
            // New since the map was last shown: a toast for each opened room, one for the new skirmishes; focus the first.
            data["map"]["toasts"] = nlohmann::json::array();
            int newSkirmishes = 0;
            for (const campaign::TicketView &t : campaign::MapView(map, save, stages))
                if (t.open && seenOpen.insert(t.id).second && mapSeen)
                {
                    if (t.mark == "skirmish")
                        ++newSkirmishes;
                    else
                        data["map"]["toasts"].push_back({{"text", Fill(words.value("opened", ""), {{"name", t.name}})}});
                    if (selected.empty() || t.mark == "next")
                        selected = t.id;
                }
            if (newSkirmishes > 0)
                data["map"]["toasts"].push_back({{"text", Fill(words.value("newSkirmishes", ""), {{"n", std::to_string(newSkirmishes)}})}});
            mapSeen = true;
            data["map"]["plan"] = std::filesystem::exists(assets.RootOf(map.plan) / map.plan) ? map.plan : "";
            view(data);
            const auto [action, arg] = RunScreen(
                game, "worldmap", data,
                [&](const std::string &a, const std::string &id, nlohmann::json &shown) {
                    if (a == "settings")
                        return (void)RunSettings(game, words), true;
                    if (a != "select")
                        return false;
                    selected = id;
                    view(shown);
                    return true;
                },
                {}, 0, lensFrame);
            if (action == "cafe")
                where = "cafe";
            else if (action != "enter")
                where = "quit";
            else
            {
                // Pre-stage: who fights, then the fight.
                const campaign::StageDef &stage = *stageOf(arg);
                std::vector<std::string> wanted = save.party.empty() ? std::vector<std::string>{"player", "bao", "mai"} : save.party;
                const auto pre = [&](nlohmann::json &shown) {
                    const campaign::PreStage p = campaign::PreStageFor(def, cast, growth, save, stage, wanted);
                    shown["pre"] = {{"title", stage.title}, {"levelLine", Fill(words.value("levelLine", ""), {{"n", std::to_string(stage.level)}})}, {"reviveWarning", p.reviveWarning}, {"roster", nlohmann::json::array()}};
                    // Each row says plainly whether that person fights; the player always does.
                    for (const campaign::Member &m : save.roster)
                    {
                        const bool in = std::ranges::find(p.party, m.id) != p.party.end();
                        shown["pre"]["roster"].push_back({{"id", m.id}, {"name", m.id == "player" ? save.player.name : name(m.id)},
                                                          {"levelText", Fill(words.value("levelShort", "Lv {n}"), {{"n", std::to_string(m.level)}})}, {"in", in},
                                                          {"stateText", words.value(m.id == "player" ? "alwaysPicked" : in ? "picked" : "notPicked", "")}});
                    }
                    wanted = p.party;
                };
                do
                {
                    nlohmann::json shown{{"words", words}};
                    pre(shown);
                    const auto [go, unused] = RunScreen(game, "prestage", shown, [&](const std::string &a, const std::string &id, nlohmann::json &s) {
                        if (a != "toggle")
                            return false;
                        if (id == "player")
                            return true; // the player always fights
                        if (const auto at = std::ranges::find(wanted, id); at != wanted.end())
                            wanted.erase(at);
                        else
                            wanted.push_back(id);
                        pre(s);
                        return true;
                    });
                    save.party = wanted;
                    where = go == "fight" ? fight(arg, wanted) : go == "quit" ? "quit" : "map";
                } while (where == "retry"); // a retry starts again from who fights
            }
        }
    }
    (void)campaign::SaveCampaign(saveFile, save, error);
    return 0;
}
