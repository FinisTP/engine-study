#include "MatchApp.hpp"

#include "CareerMatch.hpp"
#include "GameContext.hpp"

#include "MatchPlay.hpp"
#include "MatchTable.hpp"
#include "Analog.hpp"
#include "Flow.hpp"
#include "Screens.hpp"
#include "Sounds.hpp"

#include <cctype>
#include <MatchSave.hpp>

#include <ApplicationEvents.hpp>
#include <EventPump.hpp>
#include <FrameClock.hpp>
#include <Input.hpp>
#include <Log.hpp>
#include <Platform.hpp>
#include <Renderer.hpp>
#include <TextureTools.hpp>
#include <Window.hpp>

#include <glm/geometric.hpp>

#include <chrono>
#include <fstream>
#include <optional>
#include <thread>

using namespace umbra::match;

namespace
{
    // <preferences>/last-curtain/match.json ("" when the system gives no folder).
    std::filesystem::path SavePath()
    {
        const std::filesystem::path folder = umbra::platform::Platform::PreferencesFolder();
        return folder.empty() ? std::filesystem::path{} : folder / "last-curtain" / "match.json";
    }

    bool ReadSave(MatchState &out)
    {
        std::ifstream in(SavePath());
        std::string error;
        return in && LoadMatch(nlohmann::json::parse(in, nullptr, false), out, error);
    }

    // Beside the old save, then renamed over it: an interrupted write keeps the old one.
    void WriteSave(const MatchPlay &play)
    {
        const std::filesystem::path file = SavePath();
        if (file.empty())
            return;
        std::error_code error;
        std::filesystem::create_directories(file.parent_path(), error);
        const std::filesystem::path temporary = file.string() + ".tmp";
        if (std::ofstream(temporary) << play.Save().dump())
            std::filesystem::rename(temporary, file, error);
    }
}

int RunMatch(const umbra::assets::AssetFolders &assets, std::uint64_t seed, bool watch, const std::filesystem::path &capture, int frames, float zoom)
{
    GameContext game;
    if (!game.Open(assets))
        return 1;
    return RunMatch(game, assets, seed, watch, capture, frames, zoom, nullptr, 0, {});
}

int RunMatch(GameContext &game, const umbra::assets::AssetFolders &assets, std::uint64_t seed, bool watch, const std::filesystem::path &capture,
             int frames, float zoom, umbra::cafe::Career *career, int moneyPerEssence, const std::filesystem::path &careerFile,
             const MatchHooks *hooks)
{
    // 1. The match's data, its words and the pace.
    MatchDef def;
    std::vector<umbra::core::Diagnostic> diagnostics;
    const bool loaded = LoadMatch(assets.RootOf("match/rules.json") / "match", def, diagnostics);
    umbra::night::GameOptions pace;
    if (!umbra::night::LoadOptions(assets / "options.csv", pace, diagnostics))
        pace = {};
    umbra::night::AnalogScreen analog = umbra::night::LoadAnalog(umbra::night::AnalogPath(), pace); // CRT, VHS and the settings (Analog.hpp)
    nlohmann::json words, settingsWords; // a campaign stage speaks Vietnamese (match-words.vi.json) unless set to English; the career English
    if (std::ifstream in(assets / (hooks && !analog.english ? "match-words.vi.json" : "match-words.json")); in)
        words = nlohmann::json::parse(in, nullptr, false);
    if (std::ifstream in(assets / "campaign-words.json"); in && hooks)
        settingsWords = nlohmann::json::parse(in, nullptr, false);
    for (const umbra::core::Diagnostic &d : diagnostics)
        umbra::diagnostics::Error(umbra::core::Format(d));
    if (!loaded || words.is_discarded())
        return 1;

    // 2. The shared window, renderer, screens and sounds; the table.
    umbra::core::FrameClock &clock = game.clock;
    umbra::platform::Input &input = game.input;
    umbra::platform::ApplicationEvents &events = game.events;
    umbra::platform::EventPump &pump = game.pump;
    umbra::platform::Window &window = game.window;
    umbra::render::Renderer &renderer = game.renderer;
    Screens &screens = *game.screens;
    Sounds &sounds = *game.sounds; // sounds.csv; silent without a sound card or a file
    screens.SetTextScale(analog.TextScale()); // large text, as the player left it
    MatchTable table(renderer, assets, def);
    if (std::ifstream in(assets / "match-look.json"); in)
        table.SetLook(nlohmann::json::parse(in, nullptr, false));
    table.SetCamera(pace.matchCameraDistance, pace.matchCameraDistance * 0.3F, pace.matchCameraDistance * 1.6F, pace.matchCameraTilt); // the wheel zooms down into the rooms
    umbra::core::DrawList drawList;

    // 3. A match starts (or continues); the pick screen shows when none is running.
    std::optional<MatchPlay> play;
    std::vector<int> picks;
    BotLevel level = def.options.bots;
    MatchState saved;
    bool canContinue = ReadSave(saved);
    const std::uint64_t first = seed ? seed : static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
    std::uint64_t matches = 0;
    std::string shown;
    const std::string hudName = hooks ? "campaign-hud" : "match-hud"; // a stage has the campaign's HUD (plan 5)
    int savedRound = -1;
    bool ended = false;  // the end scene has started
    bool hudOpen = false; // the HUD is open (closed while a fight is staged)
    const auto show = [&](const std::string &want) {
        if (!screens.Ready() || want == shown)
            return;
        if (!shown.empty())
            screens.Ui().Close(shown);
        if (!want.empty())
            screens.Ui().Open(want), screens.Ui().ResetFocus(want);
        shown = want;
    };
    const auto begin = [&](const MatchState *restored) {
        if (hooks)
            play.emplace(def, hooks->start(), level), play->SetBrain(hooks->brain);
        else if (restored)
            play.emplace(def, *restored, level);
        else
            play.emplace(def, first + matches++, picks, level);
        play->SetWords(words);
        play->SetOptions(pace);
        play->SetWatch(watch);
        play->SetKeyNames(game.keys.Name("hurry"), game.keys.Name("skip"));
        if (hooks && hooks->setup)
            hooks->setup(*play);
        if (career && !restored)
            play->StartFrom(*career); // the player's side as the career left it
        if (screens.Ready())
            screens.Ui().Open("match-labels"), screens.Ui().Open(hudName);
        savedRound = -1, ended = false, hudOpen = true;
        if (!restored)
            table.StartScene("open", -1);
    };
    const auto backToPick = [&] {
        play.reset();
        if (screens.Ready())
            screens.Ui().Close(hudName), screens.Ui().Close("match-labels");
        hudOpen = false;
        canContinue = ReadSave(saved);
    };
    if (hooks)
        begin(nullptr);
    else if (watch)
        picks = RiggedPick(def, first ^ 0x5EEDULL, def.options.side), begin(nullptr);

    // 4. The loop.
    const int lastFrame = frames > 0 ? frames : (capture.empty() ? 0 : 3);
    int frame = 0, result = 0;
    bool wasHeld = false, dragging = false, pressTook = false, captured = false, quit = false, paused = false;
    glm::vec2 pressAt{0.0F}, lastMouse{0.0F};
    while (!quit)
    {
        input.BeginFrame();
        events.BeginFrame();
        pump.PumpEvents(input, events);
        using umbra::platform::ApplicationEventType;
        if (events.IsEventRequested(ApplicationEventType::Quit) || events.IsEventRequested(ApplicationEventType::WindowClose))
            break;
        if (events.IsEventRequested(ApplicationEventType::WindowResize))
            window.UpdateSizeFromEvent(events.GetWindowWidth(), events.GetWindowHeight()), renderer.RequestResize();
        if (events.IsEventRequested(ApplicationEventType::WindowPixelSizeChanged))
            renderer.RequestResize();
        clock.Tick();
        const auto dt = static_cast<float>(clock.GetDeltaTimeSeconds());
        const auto [width, height] = renderer.Extent();
        const glm::vec2 pixels{static_cast<float>(width), static_cast<float>(height)};
        umbra::ui::UiInput uiInput = Screens::MapInput(input, {static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight())}, pixels);
        drawList.Clear();
        drawList.hud.diagnosticsWindow = false;
        // Back (Esc, right-click) steps back one level in a stage's menu first, else pauses (P2).
        const bool back = game.keys.Pressed(input, "back") || (hooks && input.IsMouseButtonPressed(2));
        if (back && play && (paused || !hooks || !play->Back()))
            paused = !paused;
        if (back)
            uiInput.back = false;
        for (int n = 1; n <= 6 && play && hooks && !paused; ++n)
            if (game.keys.Pressed(input, "menu" + std::to_string(n)))
                (void)play->MenuKey(n);
        const bool ours = play && play->PlayerTurn(); // F ends our turn, or skips theirs: never both in one press
        if (play && hooks && !paused && game.keys.Pressed(input, "endTurn") && ours)
            (void)play->MenuPick("end");
        if (play && hooks && game.keys.Pressed(input, "notebook"))
            paused = !paused; // the notebook stops the beats (its pages come with the café, plan 6)
        if (play && hooks && !paused && game.keys.Pressed(input, "pin"))
            (void)play->Pin(); // I: the hovered unit's card stays
        if (play && hooks && !paused && game.keys.Pressed(input, "skip") && !ours && !play->State().over)
            (void)play->HandleUi("", "skip", ""); // their turn: F skips (P16)

        // 5. The match: bots and monsters play on beats; the board follows the mouse.
        int hover = -1;
        if (play)
        {
            if (!paused) // hold Space on their turn to hurry the beats (P16)
                play->Update(hooks && !play->PlayerTurn() ? dt * analog.EnemyScale() * (game.keys.Held(input, "hurry") ? 3.0F : 1.0F) : dt);
            if (hooks && hooks->beat && !paused)
                hooks->beat(*play); // a stage's scenes, between beats
            // What was just told: figures move, sounds play.
            for (const auto &[unit, motion] : play->TakeMoves())
                table.Play(unit, motion);
            for (const std::string &cue : play->TakeSounds())
                sounds.Play(cue);
            {
                const nlohmann::json told = play->UiData();
                table.SetFight(told.value("fight", std::vector<int>{}), told.value("picking", -1));
                table.SetTold(told.value("standing", std::vector<int>{}), told.value("hp", std::vector<int>{}));
            }
            table.SetFocus(play->UiData().value("focus", std::vector<int>{}));
            if (hooks)
                table.SetCast(play->Names(), play->Heights()); // a stage's cast names and ghost heights
            table.Animate(play->State(), dt);
            // A screen over the board (level, treasure, event, battle) takes the mouse: nothing falls through to the board.
            hover = play->Screen().empty() && !paused ? table.Pick(play->State().board, uiInput.mouse, pixels) : -1;
            play->Hover(hover);
            const bool held = input.IsMouseButtonHeld(0);
            if (held && !wasHeld)
                pressAt = uiInput.mouse, dragging = false;
            if (held && !dragging && glm::length(uiInput.mouse - pressAt) > 4.0F)
                dragging = true;
            if (held && dragging)
                table.Pan(uiInput.mouse - lastMouse), table.StopFollow();
            if (play->State().over && !ended)
                ended = true, table.StartScene("end", play->State().board.hub[static_cast<std::size_t>(std::max(0, play->State().winner))]);
            table.Zoom(input.Wheel() + std::exchange(zoom, 0.0F));
            if (!play->State().over && play->State().round != savedRound && !watch && !hooks)
                WriteSave(*play), savedRound = play->State().round;
        }
        if (hooks && hooks->done && hooks->done())
        {
            result = 3; // a campaign stage ended: back to the flow
            break;
        }
        table.Update(dt);
        nlohmann::json ui = play ? play->UiData() : nlohmann::json::object();
        if (play)
            table.Draw(play->State(), ui, hover, drawList);
        const std::string screen = paused ? (hooks ? "campaign-pause" : "pause") : play ? (play->Screen().empty() ? "" : "match-" + play->Screen()) : "match-pick";
        show(screen);
        // A staged fight has the screen to itself: the HUD steps away (the user, 2026-11-20).
        if (const bool hud = play && !ui.contains("fight"); screens.Ready() && play && hud != hudOpen)
        {
            if (hud)
                screens.Ui().Open(hudName);
            else
                screens.Ui().Close(hudName);
            hudOpen = hud;
        }

        // 6. The screens and their actions; a click no screen took goes to the board.
        bool uiTook = false;
        if (screens.Ready())
        {
            nlohmann::json data = ui;
            data["words"] = words.value("screens", nlohmann::json::object()); // the screens' labels
            if (play && hooks)
                data["hud"] = play->HudData(), data["menu"] = play->Menu();
            data["career"] = career != nullptr; // the end screen leads back to the café
            data["single"] = career == nullptr;
            if (!play)
                data["pick"] = MatchPlay::PickData(def, picks, canContinue, level);
            else
            {
                // Spoken lines ride on their speaker's card as a bubble.
                data["labels"] = table.Labels(play->State(), pixels);
                // The actions dock beside the one acting (to the right of its head).
                if (ui.contains("actions") || (hooks && !data["menu"]["rows"].empty()))
                    if (const auto at = table.HeadOnScreen(play->State(), Current(play->State()), pixels))
                        data["dock"] = {{{"at", {at->x + 190.0F, at->y + 150.0F}}}};
                for (auto &l : data["labels"])
                    for (const auto &b : ui["bubbles"])
                        if (b["unit"] == l["unit"])
                            l["say"] = b["text"];
            }
            data["tip"] = screens.Ui().Tip();
            data["analog"] = analog.Data();
            window.SetTextInput(screens.Ui().WantsText()); // typed text only while a text field has the focus
            for (const umbra::ui::UiAction &a : screens.Ui().Update(data, pixels, uiInput, drawList.ui))
            {
                uiTook = true;
                sounds.Play("click");
                if ((a.screen == "pause" || a.screen == "campaign-pause") && analog.Toggle(a.action, umbra::night::AnalogPath()))
                {
                    screens.SetTextScale(analog.TextScale());
                    continue; // the pause menu's screen switches
                }
                if (a.action == "quit")
                    quit = true;
                else if (a.screen == "match-pick" && a.action == "pick")
                    (void)MatchPlay::TogglePick(def, picks, std::stoi(a.arg));
                else if (a.screen == "match-pick" && a.action == "level")
                    level = a.arg == "easy" ? BotLevel::Easy : a.arg == "hard" ? BotLevel::Hard : BotLevel::Medium;
                else if (a.screen == "match-pick" && a.action == "start" && static_cast<int>(picks.size()) == def.options.side)
                    begin(nullptr);
                else if (a.screen == "match-pick" && a.action == "continue" && canContinue)
                    begin(&saved);
                else if (a.screen == "match-end" && a.action == "again" && career && play)
                {
                    // The career's match is over: its progress and takings kept, back to the café.
                    umbra::night::RecordMatch(def, play->State(), *career, moneyPerEssence);
                    std::string error;
                    if (!careerFile.empty() && !umbra::cafe::SaveCareer(careerFile, *career, error))
                        umbra::diagnostics::Error("The career was not saved: " + error);
                    std::error_code removed;
                    std::filesystem::remove(SavePath(), removed); // a finished match is not continued
                    result = 2, quit = true;
                }
                else if (a.action == "cafe" && career)
                    result = 2, quit = true; // back to the café without playing
                else if (a.screen == "match-end" && a.action == "again")
                    hooks ? begin(nullptr) : backToPick();
                else if (a.screen == "campaign-pause" && a.action == "settings")
                    analog = RunSettings(game, settingsWords); // over the paused fight; applies at once
                else if ((a.screen == "pause" || a.screen == "campaign-pause") && a.action == "resume")
                    paused = false;
                else if (hooks && (a.action == "retreat" || (a.screen == "pause" && a.action == "savequit")))
                    screens.Ui().Open("retreat"), paused = true; // a stage has no save: leaving is a retreat (asked first)
                else if (hooks && a.screen == "retreat")
                {
                    screens.Ui().Close("retreat"), screens.Ui().Close("campaign-pause"), paused = false;
                    if (a.action == "yes")
                        result = 4, quit = true; // back to the map; the flow drops all the stage's progress
                }
                else if (a.screen == "pause" && (a.action == "save" || a.action == "savequit") && play && !hooks)
                {
                    WriteSave(*play);
                    if (a.action == "savequit")
                        paused = false, backToPick();
                }
                else if (play && a.action == "focus" && !a.arg.empty() && std::isdigit(static_cast<unsigned char>(a.arg[0]))) // an order portrait: the camera goes to that one
                    table.LookAt(play->State(), std::stoi(a.arg));
                else if (play)
                    (void)play->HandleUi(a.screen == hudName ? "" : a.screen.substr(a.screen.starts_with("match-") ? 6 : 0), a.action, a.arg);
            }
        }
        else
            drawList.hud.warnings = screens.Problems();
        if (uiInput.click)
            pressTook = uiTook;
        if (play && !paused && wasHeld && !input.IsMouseButtonHeld(0) && !dragging && !pressTook && play->Screen().empty())
            play->Click(hover);
        wasHeld = input.IsMouseButtonHeld(0), lastMouse = uiInput.mouse;

        // 7. A capture: save the last frame, then stop.
        if (++frame == lastFrame && !capture.empty())
            renderer.CaptureNextFrame([&](const umbra::core::TextureData &picture) {
                std::vector<std::uint8_t> png;
                captured = umbra::assets::EncodePng(picture, png) &&
                           static_cast<bool>(std::ofstream(capture, std::ios::binary).write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size())));
                umbra::diagnostics::Info((captured ? "Captured " : "Could not write ") + capture.string());
            });
        analog.Apply(drawList.post);
        if (!renderer.RenderFrame(dt, drawList))
        {
            result = 1;
            break;
        }
        if (captured || (lastFrame > 0 && frame >= lastFrame + (capture.empty() ? 0 : 5)))
        {
            result = capture.empty() || captured ? 0 : 1;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    // 8. Quitting mid-match saves it for Continue; the match's screens close for whoever comes next.
    if (play && !play->State().over && !watch && !hooks)
        WriteSave(*play);
    if (screens.Ready())
        for (const char *name : {"match-hud", "campaign-hud", "retreat", "match-labels", "match-pick", "match-end", "match-battle", "match-level", "match-event", "match-treasure", "match-banner", "pause", "campaign-pause"})
            screens.Ui().Close(name);
    return result;
}
