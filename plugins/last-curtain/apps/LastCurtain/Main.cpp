#include "Play.hpp"
#include "Screens.hpp"
#include "Table.hpp"

#include <BoardView.hpp>
#include <Narrate.hpp>

#include <ApplicationEvents.hpp>
#include <CrashHandler.hpp>
#include <EventPump.hpp>
#include <FrameClock.hpp>
#include <Input.hpp>
#include <Log.hpp>
#include <Platform.hpp>
#include <Renderer.hpp>
#include <TextureTools.hpp>
#include <Window.hpp>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>

using namespace umbra::night;

namespace
{
    struct Options
    {
        std::uint64_t seed = 1;
        std::string player = "";          // a guest id; "" = the first guest
        bool watch = false;               // all seven are bots
        std::filesystem::path capture;    // save the last frame here and quit
        int frames = 0;                   // frames to run (0 = until closed; 3 with --capture)
    };

    // The program's assets: assets/ next to it (a package), else the source tree above the build:
    // this program's screens first, then the plugin's night data.
    umbra::assets::AssetFolders FindAssets()
    {
        const std::filesystem::path program = umbra::platform::Platform::ProgramFolder().lexically_normal();
        if (std::filesystem::exists(program / "assets/ui/skin.json"))
            return {program / "assets"};
        // A build: walk up from the program, then from the working folder, to the source tree.
        for (std::filesystem::path start : {program, std::filesystem::current_path()})
            for (std::filesystem::path dir = start; !dir.empty(); dir = dir.parent_path())
            {
                if (const auto ui = dir / "plugins/last-curtain/apps/LastCurtain/assets"; std::filesystem::exists(ui / "ui/skin.json"))
                {
                    umbra::assets::AssetFolders found{ui};
                    found.AddPlugin(dir / "plugins/last-curtain/assets");
                    return found;
                }
                if (dir == dir.parent_path())
                    break;
            }
        return {program / "assets"};
    }

    int Run(const Options &options)
    {
        umbra::platform::Platform platform;
        if (!platform.Initialize())
            return 1;
        int result = 0;
        {
            // 1. The night's data and a new night.
            const umbra::assets::AssetFolders assets = FindAssets();
            NightDef def;
            std::vector<umbra::core::Diagnostic> diagnostics;
            if (!LoadNight(assets.RootOf("night/rules.json") / "night", def, diagnostics))
            {
                for (const umbra::core::Diagnostic &d : diagnostics)
                    umbra::diagnostics::Error(umbra::core::Format(d));
                platform.Shutdown();
                return 1;
            }
            int player = -1;
            if (!options.watch)
                player = options.player.empty() ? 0 : NightDef::Find(def.guests, options.player);
            if (!options.watch && player < 0)
            {
                umbra::diagnostics::Error("No guest " + options.player + " in guests.csv");
                platform.Shutdown();
                return 1;
            }
            Play play(def, options.seed, player);
            {
                Captions captions;
                if (LoadCaptions(assets / "night/captions.json", captions, diagnostics))
                    play.SetCaptions(std::move(captions));
                for (const umbra::core::Diagnostic &d : diagnostics)
                    umbra::diagnostics::Error(umbra::core::Format(d));
            }

            // 2. Window, renderer, screens and the table.
            umbra::core::FrameClock clock;
            umbra::platform::Input input;
            umbra::platform::ApplicationEvents events;
            umbra::platform::EventPump pump;
            umbra::platform::Window window;
            if (!window.Create("The Last Curtain", 1280, 720))
            {
                platform.Shutdown();
                return 1;
            }
            umbra::render::Renderer renderer;
            if (!renderer.Initialize(window))
                return 1;
            Screens screens(renderer, assets);
            Table table(renderer);
            umbra::core::DrawList drawList;
            const int frames = options.frames > 0 ? options.frames : (options.capture.empty() ? 0 : 3);
            int frame = 0, aimed = -1, beats = 0;
            std::string shown; // the game screen Play asks for
            if (screens.Ready() && player >= 0)
                screens.Ui().Open("hud");
            bool captured = false, suspended = false;

            while (true)
            {
                // 3. Events: quit, resize, suspend.
                input.BeginFrame();
                events.BeginFrame();
                pump.PumpEvents(input, events);
                using umbra::platform::ApplicationEventType;
                if (events.IsEventRequested(ApplicationEventType::Quit) || events.IsEventRequested(ApplicationEventType::WindowClose) ||
                    (input.IsKeyPressed(umbra::platform::InputKey::Escape) && (play.Screen().empty() || play.Screen() == "actions" || play.Screen() == "caption")))
                    break;
                if (events.IsEventRequested(ApplicationEventType::WindowResize))
                {
                    window.UpdateSizeFromEvent(events.GetWindowWidth(), events.GetWindowHeight());
                    renderer.RequestResize();
                }
                if (events.IsEventRequested(ApplicationEventType::WindowPixelSizeChanged))
                    renderer.RequestResize();
                if (events.IsEventRequested(ApplicationEventType::WindowSuspended))
                    suspended = true;
                if (events.IsEventRequested(ApplicationEventType::WindowResumed))
                    suspended = false, clock.ResetClock(), renderer.RequestResize();
                if (suspended)
                {
                    umbra::platform::Platform::Idle(100);
                    continue;
                }
                clock.Tick();
                const auto dt = static_cast<float>(clock.GetDeltaTimeSeconds());

                // 4. Bots play; the camera goes to each beat the player sees, and to the player's pawn on their turn.
                if (input.IsKeyPressed(umbra::platform::InputKey::Space))
                    play.Key(umbra::platform::InputKey::Space);
                play.Update(dt);
                const NightState &state = play.State();
                const int now = Current(state);
                if (frame == 0 && now >= 0)
                    table.Aim(CellCenter(def, state.guests[static_cast<std::size_t>(now)].cell), 0.0F);
                if (play.Beats() != beats && play.LastBeat().camera >= 0)
                    table.Aim(CellCenter(def, play.LastBeat().camera), 0.6F), aimed = -1;
                beats = play.Beats();
                if (play.PlayerTurn() && now != aimed)
                    table.Aim(CellCenter(def, state.guests[static_cast<std::size_t>(now)].cell), 0.6F), aimed = now;
                table.Zoom(input.Wheel());
                table.Update(dt);
                table.SetHunt(state.taken.present);

                // 5. The screen Play asks for. On the actions panel the arrows, Enter and I belong to the board.
                if (const std::string want = play.Screen(); screens.Ready() && want != shown)
                {
                    if (!shown.empty())
                        screens.Ui().Close(shown);
                    if (!want.empty())
                        screens.Ui().Open(want), screens.Ui().ResetFocus(want);
                    shown = want;
                }
                const auto [width, height] = renderer.Extent();
                const glm::vec2 pixels{static_cast<float>(width), static_cast<float>(height)};
                umbra::ui::UiInput uiInput = Screens::MapInput(input, {static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight())}, pixels);
                const bool board = shown.empty() || shown == "actions" || shown == "caption"; // else a dialog has the keys
                if (board)
                {
                    using umbra::platform::InputKey;
                    uiInput.up = uiInput.down = uiInput.left = uiInput.right = uiInput.confirm = false;
                    for (const InputKey key : {InputKey::Up, InputKey::Right, InputKey::Down, InputKey::Left, InputKey::Enter, InputKey::I})
                        if (input.IsKeyPressed(key))
                            play.Key(key);
                }

                // 6. Draw the table and the screens; a click no screen took goes to the board.
                drawList.Clear();
                drawList.hud.diagnosticsWindow = false;
                table.Draw(def, MakeView(def, state, state.player), drawList);
                bool uiTook = false;
                if (screens.Ready())
                {
                    nlohmann::json data = play.UiData();
                    data["warnings"] = screens.Problems();
                    for (const umbra::ui::UiAction &action : screens.Ui().Update(data, pixels, uiInput, drawList.ui))
                    {
                        (void)play.HandleUi(action.screen, action.action, action.arg);
                        uiTook = true;
                    }
                }
                else
                    drawList.hud.warnings = screens.Problems();
                if (uiInput.click && !uiTook)
                    play.Click(table.Pick(def, uiInput.mouse, pixels));

                // 7. A capture: save the last frame, then stop.
                if (++frame == frames && !options.capture.empty())
                    renderer.CaptureNextFrame([&](const umbra::core::TextureData &picture) {
                        std::vector<std::uint8_t> png;
                        captured = umbra::assets::EncodePng(picture, png) &&
                                   static_cast<bool>(std::ofstream(options.capture, std::ios::binary).write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size())));
                        umbra::diagnostics::Info((captured ? "Captured " : "Could not write ") + options.capture.string());
                    });
                if (!renderer.RenderFrame(dt, drawList))
                {
                    result = 1;
                    break;
                }
                if (captured || (frames > 0 && frame >= frames + (options.capture.empty() ? 0 : 5)))
                {
                    result = options.capture.empty() || captured ? 0 : 1;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            }
        }
        platform.Shutdown();
        return result;
    }
}

int main(int argc, char **argv)
{
    // --seed N, --player <guest id>, --watch, --capture <png>, --frames N.
    Options options;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        const bool more = i + 1 < argc;
        if (arg == "--seed" && more)
            options.seed = std::strtoull(argv[++i], nullptr, 10);
        else if (arg == "--player" && more)
            options.player = argv[++i];
        else if (arg == "--watch")
            options.watch = true;
        else if (arg == "--capture" && more)
            options.capture = argv[++i];
        else if (arg == "--frames" && more)
            options.frames = std::atoi(argv[++i]);
    }
    umbra::diagnostics::Initialize();
    umbra::diagnostics::InstallCrashHandlers();
    const int result = Run(options);
    umbra::diagnostics::UninstallCrashHandlers();
    umbra::diagnostics::Shutdown();
    return result;
}
