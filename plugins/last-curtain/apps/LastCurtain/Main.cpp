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
#include <optional>
#include <random>
#include <string>
#include <thread>

using namespace umbra::night;
using umbra::platform::InputKey;

namespace
{
    struct Options
    {
        std::uint64_t seed = 0;        // 0 = from the clock
        std::string player;            // a guest id: start at once as that guest
        bool watch = false;            // all seven are bots
        std::filesystem::path capture; // save the last frame here and quit
        int frames = 0;                // frames to run (0 = until closed; 3 with --capture)
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

    // The one save: <preferences>/last-curtain/night.json ("" when the system gives no folder).
    std::filesystem::path SavePath()
    {
        const std::filesystem::path folder = umbra::platform::Platform::PreferencesFolder();
        return folder.empty() ? std::filesystem::path{} : folder / "last-curtain" / "night.json";
    }

    bool ReadSave(const NightDef &def, NightState &out)
    {
        std::ifstream in(SavePath());
        return in && Play::Restore(def, nlohmann::json::parse(in, nullptr, false), out);
    }

    // Written beside the old save, then renamed over it: an interrupted write keeps the old one.
    void WriteSave(const Play &play)
    {
        const std::filesystem::path file = SavePath();
        if (file.empty())
            return;
        std::error_code error;
        std::filesystem::create_directories(file.parent_path(), error);
        const std::filesystem::path temporary = file.string() + ".tmp";
        if (std::ofstream(temporary) << play.Save().dump())
            std::filesystem::rename(temporary, file, error);
        if (error)
            umbra::diagnostics::Error("Could not save the night: " + error.message());
    }

    // Play's slide as the table draws it.
    Table::Slide ToDraw(const Play::Slide &s) { return {s.guest, s.from, s.to, s.t}; }

    int Run(const Options &options)
    {
        umbra::platform::Platform platform;
        if (!platform.Initialize())
            return 1;
        int result = 0;
        {
            // 1. The night's data and the narrator's lines.
            const umbra::assets::AssetFolders assets = FindAssets();
            NightDef def;
            Captions captions;
            std::vector<umbra::core::Diagnostic> diagnostics;
            const bool loaded = LoadNight(assets.RootOf("night/rules.json") / "night", def, diagnostics);
            (void)LoadCaptions(assets / "night/captions.json", captions, diagnostics);
            for (const umbra::core::Diagnostic &d : diagnostics)
                umbra::diagnostics::Error(umbra::core::Format(d));
            const int named = options.player.empty() ? -1 : NightDef::Find(def.guests, options.player);
            if (!loaded || (!options.player.empty() && named < 0))
            {
                if (loaded)
                    umbra::diagnostics::Error("No guest " + options.player + " in guests.csv");
                platform.Shutdown();
                return 1;
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
            int frame = 0, aimed = -1, beats = 0, savedRound = -1;
            std::string shown; // the screen on show besides the HUD
            bool captured = false, suspended = false, quit = false, fresh = true; // fresh: the camera jumps to a new night
            std::mt19937_64 random(options.seed ? options.seed : static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count()));

            // 3. A night starts (or continues); the start screen shows when none is running.
            std::optional<Play> play;
            bool canContinue = false;
            const auto show = [&](const std::string &want) {
                if (!screens.Ready() || want == shown)
                    return;
                if (!shown.empty())
                    screens.Ui().Close(shown);
                if (!want.empty())
                    screens.Ui().Open(want), screens.Ui().ResetFocus(want);
                shown = want;
            };
            const auto begin = [&](int player, const NightState *restored) {
                if (restored)
                    play.emplace(def, *restored);
                else
                    play.emplace(def, options.seed ? options.seed : random(), player);
                play->SetCaptions(captions);
                if (screens.Ready() && play->State().player >= 0)
                    screens.Ui().Open("hud");
                aimed = -1, beats = 0, savedRound = -1, fresh = true;
            };
            const auto backToStart = [&] {
                play.reset();
                if (screens.Ready())
                    screens.Ui().Close("hud");
                NightState unused;
                canContinue = ReadSave(def, unused);
            };
            if (options.watch)
                begin(-1, nullptr);
            else if (named >= 0)
                begin(named, nullptr);
            else
                backToStart();

            while (!quit)
            {
                // 4. Events: quit, resize, suspend.
                input.BeginFrame();
                events.BeginFrame();
                pump.PumpEvents(input, events);
                using umbra::platform::ApplicationEventType;
                const std::string screen = play ? play->Screen() : "start";
                const bool board = screen.empty() || screen == "menu" || screen == "picking" || screen == "confirm" || screen == "caption"; // else a dialog has the keys
                // Esc quits only from the start screen; in a night it means what the screen says (Play::Key).
                if (events.IsEventRequested(ApplicationEventType::Quit) || events.IsEventRequested(ApplicationEventType::WindowClose) ||
                    (input.IsKeyPressed(InputKey::Escape) && screen == "start"))
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
                const auto [width, height] = renderer.Extent();
                const glm::vec2 pixels{static_cast<float>(width), static_cast<float>(height)};
                umbra::ui::UiInput uiInput = Screens::MapInput(input, {static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight())}, pixels);
                drawList.Clear();
                drawList.hud.diagnosticsWindow = false;

                // 5. The night: bots play; the camera goes to each beat the player sees, and to the player's pawn on their turn.
                if (play)
                {
                    if (input.IsKeyPressed(InputKey::Space))
                        play->Key(InputKey::Space);
                    if (input.IsKeyPressed(InputKey::Escape))
                        play->Key(InputKey::Escape), uiInput.back = false;
                    play->Update(dt);
                    const NightState &state = play->State();
                    const int now = Current(state);
                    if (fresh && now >= 0)
                        table.Aim(CellCenter(def, state.guests[static_cast<std::size_t>(now)].cell), 0.0F), fresh = false;
                    if (play->Beats() != beats && play->LastBeat().camera >= 0)
                        table.Aim(CellCenter(def, play->LastBeat().camera), 0.6F), aimed = -1;
                    beats = play->Beats();
                    if (play->PlayerTurn() && now != aimed)
                        table.Aim(CellCenter(def, state.guests[static_cast<std::size_t>(now)].cell), 0.6F), aimed = now;
                    table.Zoom(input.Wheel());
                    table.Update(dt);
                    table.SetHunt(state.taken.present);
                    // On the board the arrows, Enter and I move and end the turn; a dialog takes them otherwise.
                    if (board)
                    {
                        uiInput.up = uiInput.down = uiInput.left = uiInput.right = uiInput.confirm = false;
                        for (const InputKey key : {InputKey::Up, InputKey::Right, InputKey::Down, InputKey::Left, InputKey::Enter, InputKey::I})
                            if (input.IsKeyPressed(key))
                                play->Key(key);
                    }
                    play->Hover(table.Pick(def, uiInput.mouse, pixels));
                    table.Draw(def, MakeView(def, state, state.player), play->Previewed().path, play->Previewed().affordable, ToDraw(play->Sliding()), drawList);
                    // Saves: at each new round, and gone once the night is over.
                    if (state.player >= 0 && !state.over && state.round != savedRound)
                        WriteSave(*play), savedRound = state.round;
                    if (state.player >= 0 && state.over && savedRound >= 0)
                    {
                        std::error_code error;
                        std::filesystem::remove(SavePath(), error);
                        savedRound = -1;
                    }
                }
                else
                    table.Draw(def, BoardView{}, {}, 0, {}, drawList); // the dark room behind the start screen
                show(play ? play->Screen() : "start");

                // 6. The screens; their actions; a click no screen took goes to the board.
                bool uiTook = false;
                if (screens.Ready())
                {
                    nlohmann::json data = play ? play->UiData() : Play::StartData(def, canContinue);
                    data["warnings"] = screens.Problems();
                    for (const umbra::ui::UiAction &action : screens.Ui().Update(data, pixels, uiInput, drawList.ui))
                    {
                        uiTook = true;
                        NightState restored;
                        if (action.action == "quit")
                            quit = true;
                        else if (action.screen == "start" && action.action == "pick")
                            begin(NightDef::Find(def.guests, action.arg), nullptr);
                        else if (action.screen == "start" && action.action == "random")
                            begin(static_cast<int>(random() % def.guests.size()), nullptr);
                        else if (action.screen == "start" && action.action == "continue" && ReadSave(def, restored))
                            begin(-1, &restored);
                        else if (action.screen == "end" && action.action == "new")
                            backToStart();
                        else if (play && action.action == "save")
                            WriteSave(*play);
                        else if (play && action.action == "savequit")
                        {
                            WriteSave(*play);
                            backToStart();
                        }
                        else if (play)
                            (void)play->HandleUi(action.screen, action.action, action.arg);
                    }
                }
                else
                    drawList.hud.warnings = screens.Problems();
                if (play && uiInput.click && !uiTook)
                    play->Click(table.Pick(def, uiInput.mouse, pixels));
                if (play && input.IsMouseButtonPressed(2) && !uiTook) // the right button inspects
                    play->Click(table.Pick(def, uiInput.mouse, pixels), true);

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
            // 8. Quitting mid-night saves it for Continue.
            if (play && play->State().player >= 0 && !play->State().over)
                WriteSave(*play);
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
