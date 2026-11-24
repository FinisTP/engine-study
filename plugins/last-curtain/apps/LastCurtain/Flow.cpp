#include "Flow.hpp"

#include "Analog.hpp"
#include "CafeHubApp.hpp"
#include "DialogApp.hpp"
#include "GameContext.hpp"
#include "StageApp.hpp"

#include <CampaignCafe.hpp>
#include <Log.hpp>
#include <Options.hpp>
#include <TextureTools.hpp>

#include <chrono>
#include <fstream>
#include <thread>

std::pair<std::string, std::string> RunScreen(GameContext &game, const std::string &screen, nlohmann::json data,
                                              const std::function<bool(const std::string &, const std::string &, nlohmann::json &)> &handle,
                                              const std::filesystem::path &capture, int frames)
{
    Screens &screens = *game.screens;
    screens.Ui().Open(screen), screens.Ui().ResetFocus(screen);
    std::pair<std::string, std::string> picked{"quit", ""};
    umbra::core::DrawList drawList;
    std::vector<umbra::core::Diagnostic> ignored;
    umbra::night::GameOptions options;
    (void)umbra::night::LoadOptions({}, options, ignored);
    const umbra::night::AnalogScreen analog = umbra::night::LoadAnalog(umbra::night::AnalogPath(), options);
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
        drawList.hud.diagnosticsWindow = false;
        const auto [width, height] = game.renderer.Extent();
        const glm::vec2 pixels{static_cast<float>(width), static_cast<float>(height)};
        umbra::ui::UiInput input = Screens::MapInput(game.input, {static_cast<float>(game.window.GetWidth()), static_cast<float>(game.window.GetHeight())}, pixels);
        game.window.SetTextInput(screens.Ui().WantsText());
        // 1. An action ends the screen unless the handler keeps it open (a toggle, a selection).
        for (const umbra::ui::UiAction &a : screens.Ui().Update(data, pixels, input, drawList.ui))
        {
            game.sounds->Play("click");
            if (!handle || !handle(a.action, a.arg, data))
                picked = {a.action, a.arg}, done = true;
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
    nlohmann::json words;
    if (std::ifstream in(assets / "campaign-words.json"); in)
        words = nlohmann::json::parse(in, nullptr, false);
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
    umbra::night::AnalogScreen analog = umbra::night::LoadAnalog(umbra::night::AnalogPath(), options);
    const auto settings = [&](const std::string &action, const std::string &, nlohmann::json &data) {
        if (!analog.Toggle(action, umbra::night::AnalogPath()))
            return false;
        game.screens->SetTextScale(analog.TextScale());
        data["analog"] = analog.Data();
        return true; // a switch: the screen stays
    };
    for (bool started = false; !started;)
    {
        const auto [action, arg] = RunScreen(game, "title", {{"words", words}, {"canContinue", has}}, {}, capture, frames);
        if (action == "quit" || !capture.empty())
            return 0;
        if (action == "settings")
            (void)RunScreen(game, "settings", {{"words", words}, {"analog", analog.Data()}}, settings);
        else if (action == "continue" && has)
            started = true;
        else if (action == "new")
        {
            // A name and a form of address, then the prologue and the first fight.
            std::string gender = "female";
            const auto [made, unused] = RunScreen(game, "newgame", {{"words", words}, {"female", true}}, [&](const std::string &a, const std::string &g, nlohmann::json &data) {
                if (a != "gender")
                    return false;
                gender = g, data["female"] = g == "female", data["male"] = g == "male";
                return true;
            });
            std::string player = game.screens->Ui().FieldText("newgame", "name");
            if (made != "start")
                continue;
            if (player.empty())
                player = cast.Find("nameless") ? cast.Find("nameless")->name : "Linh";
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
        for (;;)
        {
            const campaign::Save before = save;
            campaign::BuiltStage out;
            if (RunStage(game, assets, id, save.rooms[id].refill, 0, save.player, {}, 0, &save, party, &out) != 3)
                return "quit";
            const campaign::StageDef &stage = *stageOf(id);
            if (out.state.winner == 0)
            {
                campaign::WinStage(def, save, stage, out, stages);
                (void)campaign::SaveCampaign(saveFile, save, error);
                const campaign::Result r = campaign::ResultOf(before, save);
                nlohmann::json lines = nlohmann::json::array();
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
            if (next != "retry")
                return next == "quit" ? "quit" : "map";
        }
    };
    std::string where = save.facts.contains("won:t1") ? "cafe" : "t1";
    std::string selected;
    while (where != "quit")
    {
        // Recruits who joined in the café enter the roster; Bao and Ba Mai come before T3 at level 2.
        for (const std::string &r : save.joined)
            if (std::ranges::none_of(save.roster, [&](const campaign::Member &m) { return m.id == r; }))
                save.roster.push_back({r, 3, 0});
        if (save.facts.contains("won:t1"))
            for (const char *ally : {"bao", "mai"})
                if (std::ranges::none_of(save.roster, [&](const campaign::Member &m) { return m.id == ally; }))
                    save.roster.push_back({ally, 2, 0});
        (void)campaign::SaveCampaign(saveFile, save, error);
        if (where == "t1")
            where = fight("t1", {"player"});
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
                    data["map"]["tickets"].push_back({{"id", t.id}, {"name", t.mark == "unknown" ? "?" : t.name}, {"markText", mark}, {"at", {80.0F + t.x * 1100.0F - 60.0F, 60.0F + t.y * 560.0F - 24.0F}},
                                                      {"selected", t.id == selected}, {"closed", !t.open}});
                    if (t.id == selected)
                    {
                        const auto s = stageOf(t.id);
                        data["map"]["panel"] = {{"id", t.id}, {"name", t.name}, {"detail", t.detail}, {"open", t.open},
                                                {"level", s == stages.end() ? std::string() : Fill(words.value("levelLine", ""), {{"n", std::to_string(s->level)}})}};
                    }
                }
            };
            nlohmann::json data{{"words", words}};
            data["map"]["plan"] = std::filesystem::exists(assets.RootOf(map.plan) / map.plan) ? map.plan : "";
            view(data);
            const auto [action, arg] = RunScreen(game, "worldmap", data, [&](const std::string &a, const std::string &id, nlohmann::json &shown) {
                if (a != "select")
                    return false;
                selected = id;
                view(shown);
                return true;
            });
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
                    for (const campaign::Member &m : save.roster)
                        shown["pre"]["roster"].push_back({{"id", m.id}, {"name", m.id == "player" ? save.player.name : name(m.id)}, {"levelText", "Lv " + std::to_string(m.level)},
                                                          {"in", std::ranges::find(p.party, m.id) != p.party.end()}, {"locked", m.id == "player"}});
                    wanted = p.party;
                };
                nlohmann::json shown{{"words", words}};
                pre(shown);
                const auto [go, unused] = RunScreen(game, "prestage", shown, [&](const std::string &a, const std::string &id, nlohmann::json &s) {
                    if (a != "toggle")
                        return false;
                    if (const auto at = std::ranges::find(wanted, id); at != wanted.end())
                        wanted.erase(at);
                    else
                        wanted.push_back(id);
                    pre(s);
                    return true;
                });
                save.party = wanted;
                where = go == "fight" ? fight(arg, wanted) : go == "quit" ? "quit" : "map";
            }
        }
    }
    (void)campaign::SaveCampaign(saveFile, save, error);
    return 0;
}
