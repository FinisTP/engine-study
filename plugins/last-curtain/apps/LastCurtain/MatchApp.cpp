#include "MatchApp.hpp"

#include "MatchPlay.hpp"
#include "MatchTable.hpp"
#include "Screens.hpp"
#include "Sounds.hpp"

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
using umbra::platform::InputKey;

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

int RunMatch(const umbra::assets::AssetFolders &assets, std::uint64_t seed, bool watch, const std::filesystem::path &capture, int frames)
{
    // 1. The match's data, its words and the pace.
    MatchDef def;
    std::vector<umbra::core::Diagnostic> diagnostics;
    const bool loaded = LoadMatch(assets.RootOf("match/rules.json") / "match", def, diagnostics);
    nlohmann::json words;
    if (std::ifstream in(assets / "match-words.json"); in)
        words = nlohmann::json::parse(in, nullptr, false);
    umbra::night::GameOptions pace;
    if (!umbra::night::LoadOptions(assets / "options.csv", pace, diagnostics))
        pace = {};
    for (const umbra::core::Diagnostic &d : diagnostics)
        umbra::diagnostics::Error(umbra::core::Format(d));
    if (!loaded || words.is_discarded())
        return 1;

    // 2. Window, renderer, screens, the table.
    umbra::core::FrameClock clock;
    umbra::platform::Input input;
    umbra::platform::ApplicationEvents events;
    umbra::platform::EventPump pump;
    umbra::platform::Window window;
    if (!window.Create("The Last Curtain", 1280, 720))
        return 1;
    umbra::render::Renderer renderer;
    if (!renderer.Initialize(window))
        return 1;
    Screens screens(renderer, assets);
    Sounds sounds(assets); // sounds.csv; silent without a sound card or a file
    MatchTable table(renderer, assets, def);
    if (std::ifstream in(assets / "match-look.json"); in)
        table.SetLook(nlohmann::json::parse(in, nullptr, false));
    table.SetCamera(pace.matchCameraDistance, pace.matchCameraDistance * 0.5F, pace.matchCameraDistance * 1.8F, pace.matchCameraTilt);
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
    int savedRound = -1;
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
        if (restored)
            play.emplace(def, *restored, level);
        else
            play.emplace(def, first + matches++, picks, level);
        play->SetWords(words);
        play->SetOptions(pace);
        play->SetWatch(watch);
        if (screens.Ready())
            screens.Ui().Open("match-labels"), screens.Ui().Open("match-hud");
        savedRound = -1;
    };
    const auto backToPick = [&] {
        play.reset();
        if (screens.Ready())
            screens.Ui().Close("match-hud"), screens.Ui().Close("match-labels");
        canContinue = ReadSave(saved);
    };
    if (watch)
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
        if (input.IsKeyPressed(InputKey::Escape) && play)
            paused = !paused, uiInput.back = false;

        // 5. The match: bots and monsters play on beats; the board follows the mouse.
        int hover = -1;
        if (play)
        {
            if (!paused)
                play->Update(dt);
            // What was just told: figures move, sounds play.
            for (const auto &[unit, motion] : play->TakeMoves())
                table.Play(unit, motion);
            for (const std::string &cue : play->TakeSounds())
                sounds.Play(cue);
            table.Animate(play->State(), dt);
            hover = table.Pick(play->State().board, uiInput.mouse, pixels);
            play->Hover(hover);
            const bool held = input.IsMouseButtonHeld(0);
            if (held && !wasHeld)
                pressAt = uiInput.mouse, dragging = false;
            if (held && !dragging && glm::length(uiInput.mouse - pressAt) > 4.0F)
                dragging = true;
            if (held && dragging)
                table.Pan(uiInput.mouse - lastMouse);
            table.Zoom(input.Wheel());
            if (!play->State().over && play->State().round != savedRound && !watch)
                WriteSave(*play), savedRound = play->State().round;
        }
        table.Update(dt);
        nlohmann::json ui = play ? play->UiData() : nlohmann::json::object();
        if (play)
            table.Draw(play->State(), ui, hover, drawList);
        const std::string screen = paused ? "pause" : play ? (play->Screen().empty() ? "" : "match-" + play->Screen()) : "match-pick";
        show(screen);

        // 6. The screens and their actions; a click no screen took goes to the board.
        bool uiTook = false;
        if (screens.Ready())
        {
            nlohmann::json data = ui;
            if (!play)
                data["pick"] = MatchPlay::PickData(def, picks, canContinue, level);
            else
                data["labels"] = table.Labels(play->State(), pixels);
            data["tip"] = screens.Ui().Tip();
            for (const umbra::ui::UiAction &a : screens.Ui().Update(data, pixels, uiInput, drawList.ui))
            {
                uiTook = true;
                sounds.Play("click");
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
                else if (a.screen == "match-end" && a.action == "again")
                    backToPick();
                else if (a.screen == "pause" && a.action == "resume")
                    paused = false;
                else if (a.screen == "pause" && (a.action == "save" || a.action == "savequit") && play)
                {
                    WriteSave(*play);
                    if (a.action == "savequit")
                        paused = false, backToPick();
                }
                else if (play)
                    (void)play->HandleUi(a.screen == "match-hud" ? "" : a.screen.substr(a.screen.starts_with("match-") ? 6 : 0), a.action, a.arg);
            }
        }
        else
            drawList.hud.warnings = screens.Problems();
        if (uiInput.click)
            pressTook = uiTook;
        if (play && !paused && wasHeld && !input.IsMouseButtonHeld(0) && !dragging && !pressTook)
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
    // 8. Quitting mid-match saves it for Continue.
    if (play && !play->State().over && !watch)
        WriteSave(*play);
    return result;
}
