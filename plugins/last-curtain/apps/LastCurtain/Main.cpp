#include "Art.hpp"
#include "CafeApp.hpp"
#include "GameContext.hpp"
#include "MatchApp.hpp"
#include "Play.hpp"
#include "Screens.hpp"
#include "Sounds.hpp"
#include "Table.hpp"

#include <Cafe.hpp>
#include <Career.hpp>

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

#if UMBRA_TOWN_LLAMA
#include <LlamaMind.hpp>
#endif

#include <glm/geometric.hpp>

#include <algorithm>
#include <utility>
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
        bool look = false;             // open Look on the player's first menu (pictures of the stage)
        std::filesystem::path capture; // save the last frame here and quit
        int frames = 0;                // frames to run (0 = until closed; 3 with --capture)
        bool night = false;            // the old night (--night); else the faction match
        bool cafe = false;             // --cafe: a capture of the café
        float zoom = 0.0F;             // the match: wheel steps at the start (--zoom N)
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

    // The built-in model (plan 3): models/night.gguf beside the program's assets folder, never
    // committed. No file or no llama.cpp in this build: null, and the night uses its written lines.
    std::unique_ptr<umbra::town::Mind> LoadModel(const umbra::assets::AssetFolders &assets)
    {
        const std::filesystem::path file = (assets / "ui/skin.json").parent_path().parent_path().parent_path() / "models/night.gguf";
#if UMBRA_TOWN_LLAMA
        umbra::town::LlamaOptions options;
        options.threads = std::max(2, static_cast<int>(std::thread::hardware_concurrency()) / 2);
        std::string error;
        if (std::unique_ptr<umbra::town::Mind> model = umbra::town::LoadLlamaMind(file, options, error))
        {
            umbra::diagnostics::Info("The Last Curtain: model " + model->Name() + " loaded (" + (umbra::town::LlamaHasGpu() ? "graphics card" : "CPU") + ")");
            return model;
        }
        umbra::diagnostics::Info("The Last Curtain: no model (" + error + "); the written lines are used");
#else
        umbra::diagnostics::Info("The Last Curtain: this build has no llama.cpp; the written lines are used (" + file.string() + ")");
#endif
        return nullptr;
    }

    // Play's slide as the table draws it.
    Table::Slide ToDraw(const Play::Slide &s) { return {s.guest, s.from, s.to, s.t}; }

    // <preferences>/last-curtain/career.json, "" when the system gives no folder.
    std::filesystem::path CareerPath()
    {
        const std::filesystem::path folder = umbra::platform::Platform::PreferencesFolder();
        return folder.empty() ? std::filesystem::path{} : folder / "last-curtain" / "career.json";
    }

    int RunCareer(const umbra::assets::AssetFolders &assets, const Options &options)
    {
        GameContext game;
        if (!game.Open(assets))
            return 1;
        // 1. The career from its file, or a new one (a bad file is reported and left alone).
        umbra::cafe::Career career;
        const std::filesystem::path file = CareerPath();
        std::string error;
        if (!file.empty() && std::filesystem::exists(file) && !umbra::cafe::LoadCareer(file, career, error))
            umbra::diagnostics::Error("The career could not be read (" + error + "); a new one starts and the file is kept.");
        if (!file.empty())
        {
            std::error_code made;
            std::filesystem::create_directories(file.parent_path(), made);
        }
        const std::filesystem::path save = error.empty() ? file : std::filesystem::path{};
        // 2. The café, then the match, until the window closes.
        umbra::cafe::CafeDef cafe;
        std::vector<umbra::core::Diagnostic> diagnostics;
        const int moneyPerEssence = umbra::cafe::LoadCafe(assets.RootOf("cafe/cafe.json") / "cafe", cafe, diagnostics) ? cafe.rules.moneyPerEssence : 1;
        while (RunCafe(game, assets, career, save, options.capture, options.frames) == 1)
            if (RunMatch(game, assets, options.seed, false, {}, 0, 0.0F, &career, moneyPerEssence, save) != 2)
                break;
        return 0;
    }

    int Run(Options options)
    {
        umbra::platform::Platform platform;
        if (!platform.Initialize())
            return 1;
        int result = 0;
        {
            // 1. The night's data and the narrator's lines.
            const umbra::assets::AssetFolders assets = FindAssets();
            if (!options.night && (options.watch || (!options.capture.empty() && !options.cafe)))
            {
                result = RunMatch(assets, options.seed, options.watch, options.capture, options.frames, options.zoom);
                platform.Shutdown();
                return result;
            }
            if (!options.night)
            {
                // The career: the café, then a match, then the café again (CafePlay.md).
                result = RunCareer(assets, options);
                platform.Shutdown();
                return result;
            }
            NightDef def;
            Captions captions;
            std::vector<umbra::core::Diagnostic> diagnostics;
            const bool loaded = LoadNight(assets.RootOf("night/rules.json") / "night", def, diagnostics);
            (void)LoadCaptions(assets / "night/captions.json", captions, diagnostics);
            nlohmann::json scenes; // the night's scenes on the board (Play.md "Cutscenes"); none if the file is bad
            if (std::ifstream in(assets / "night/cutscenes.json"); in)
                scenes = nlohmann::json::parse(in, nullptr, false);
            if (scenes.is_discarded())
                diagnostics.push_back({umbra::core::Severity::Error, "night.cutscenes", "cutscenes.json", "", "not JSON"}), scenes = {};
            Sounds sounds(assets); // the cues of sounds.csv; silent without a sound card or a file
            GameOptions gameOptions; // the pace and camera (options.csv); a bad file keeps the defaults
            if (!LoadOptions(assets / "options.csv", gameOptions, diagnostics))
                gameOptions = {};
            for (const umbra::core::Diagnostic &d : diagnostics)
                umbra::diagnostics::Error(umbra::core::Format(d));
            umbra::night::Words words(def, LoadModel(assets)); // outlives every Play below
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
            Table table(renderer, assets);
            table.SetCamera(gameOptions.cameraDistance, gameOptions.cameraMinDistance, gameOptions.cameraMaxDistance, gameOptions.cameraTilt);
            table.LoadArt(def, assets);
            // The night's mood: the blood colour table after the Turn (Art.md).
            auto bloodLut = std::make_shared<umbra::core::TextureData>();
            std::string lutError;
            if (!umbra::assets::LoadImageFile(assets / ArtPath("ui", "blood_lut"), *bloodLut, lutError))
                bloodLut.reset();
            umbra::core::DrawList drawList;
            const int frames = options.frames > 0 ? options.frames : (options.capture.empty() ? 0 : 3);
            int frame = 0, aimed = -1, savedRound = -1;
            std::uint32_t seenLog = 0; // log entries the figures have played (Table::React)
            bool wasHeld = false, dragging = false, pressTook = false; // the left button: a drag looks around, a release without one clicks
            glm::vec2 pressAt{0.0F}, lastMouse{0.0F};
            BoardView view; // what the player sees this frame
            std::string shown; // the screen on show besides the HUD
            bool captured = false, suspended = false, quit = false, fresh = true; // fresh: the camera jumps to a new night
            std::mt19937_64 random(options.seed ? options.seed : static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count()));

            // 3. A night starts (or continues); the start screen shows when none is running.
            std::optional<Play> play;
            bool canContinue = false;
            int chosen = -1; // the guest whose page is open on the start screen
            int lastShot = -1; // the scene's shot last seen (a new one starts its figure's clip)
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
                play->SetOptions(gameOptions);
                play->SetWords(&words);
                play->SetCutscenes(scenes);
                table.SetFeatured(FeaturedCells(def, play->State()));
                table.NewNight(), seenLog = play->State().logCount;
                if (screens.Ready())
                    screens.Ui().Open("dread"), screens.Ui().Open("labels"); // under the HUD
                if (screens.Ready() && play->State().player >= 0)
                    screens.Ui().Open("hud");
                aimed = -1, savedRound = -1, fresh = true;
            };
            const auto backToStart = [&] {
                play.reset();
                if (screens.Ready())
                    screens.Ui().Close("hud"), screens.Ui().Close("labels"), screens.Ui().Close("dread");
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
                const std::string screen = play ? play->Screen() : chosen >= 0 ? "bio" : "start";
                const bool board = screen.empty() || screen == "menu" || screen == "picking" || screen == "confirm" || screen == "caption"; // else a dialog has the keys
                // Esc quits only from the start screen; in a night it means what the screen says (Play::Key).
                if (events.IsEventRequested(ApplicationEventType::Quit) || events.IsEventRequested(ApplicationEventType::WindowClose) ||
                    (input.IsKeyPressed(InputKey::Escape) && screen == "start"))
                    break;
                if (input.IsKeyPressed(InputKey::Escape) && screen == "bio")
                    chosen = -1; // Esc on a guest's page goes back to the list
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
                    for (const std::string &cue : play->TakeCues())
                        sounds.Play(cue);
                    if (options.look && play->Screen() == "menu")
                        (void)play->HandleUi("menu", "look", ""), options.look = false;
                    const NightState &state = play->State();
                    // The camera eases to a moment's room always, else (while following) to whoever acts; a drag frees it.
                    const Play::CameraShot shot = play->Shot(); // a scene steers the camera itself
                    table.SetShot(shot.cell >= 0, shot.distance, shot.yaw, shot.tilt);
                    if (shot.index >= 0 && shot.index != lastShot && !shot.clip.empty())
                        table.Gesture(shot.clipGuest, shot.clip);
                    lastShot = shot.index;
                    const int moment = shot.cell >= 0 ? shot.cell : play->StageCell();
                    const int look = moment >= 0 ? moment : play->CameraCell();
                    if (fresh && look >= 0)
                        table.Aim(CellCenter(def, look), 0.0F), aimed = look, fresh = false;
                    if ((play->Following() || moment >= 0) && look >= 0 && look != aimed)
                        table.Aim(CellCenter(def, look), gameOptions.cameraEaseSeconds), aimed = look;
                    if (!play->Following())
                        aimed = -1; // Focus aims again at once
                    const bool held = input.IsMouseButtonHeld(0);
                    if (held && !wasHeld)
                        pressAt = uiInput.mouse, dragging = false;
                    if (held && !dragging && glm::length(uiInput.mouse - pressAt) > 4.0F)
                        dragging = true, play->Drag();
                    if (held && dragging)
                        table.Pan(uiInput.mouse - lastMouse);
                    table.Zoom(input.Wheel());
                    table.Update(dt);
                    table.SetHunt(state.taken.present);
                    table.SetActing(play->Acting());
                    table.SetBanner(play->TurnBanner().guest, play->TurnBanner().t);
                    // The mood: the post grade per hour, the blood table and stains after the Turn.
                    const Mood mood = NightMood(def, state);
                    drawList.post = mood.post;
                    drawList.postLut = mood.bloodGrade && bloodLut ? bloodLut : nullptr;
                    table.SetMood(mood.blood, mood.flicker, Readability(mood));
                    // On the board the arrows, Enter and I move and end the turn; a dialog takes them otherwise.
                    if (board)
                    {
                        uiInput.up = uiInput.down = uiInput.left = uiInput.right = uiInput.confirm = false;
                        for (const InputKey key : {InputKey::Up, InputKey::Right, InputKey::Down, InputKey::Left, InputKey::Enter, InputKey::I})
                            if (input.IsKeyPressed(key))
                                play->Key(key);
                    }
                    view = play->View();
                    // The figures play what the log says: blows, powers, falls.
                    table.React(NewLog(state, seenLog)), seenLog = state.logCount;
                    table.Animate(def, view, ToDraw(play->Sliding()), dt);
                    // The table or the room view (the player's switch, or a moment); a battle in sight stands apart.
                    table.SetRoomView(play->RoomView() && shot.cell < 0); // a scene plays on the table
                    std::vector<Table::StageActor> actors;
                    const bool fall = play->Screen() == "fall"; // the Taken's fall is staged like a battle's end
                    if (fall || (state.battle.active && play->StageCell() == state.battle.cell))
                        for (const Play::Actor &a : play->StageActors())
                            actors.push_back({a.guest, a.monsterKind, a.side, a.taken});
                    table.SetBattle(actors.empty() ? -1 : play->StageCell(), actors);
                    table.SetGoalCells(play->GoalCells());
                    play->Hover(table.Pick(def, uiInput.mouse, pixels));
                    table.Draw(def, view, play->Previewed().path, play->Previewed().affordable, ToDraw(play->Sliding()), play->Rings(), drawList);
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
                    table.Draw(def, BoardView{}, {}, 0, {}, {}, drawList); // the dark room behind the start screen
                show(play ? play->Screen() : chosen >= 0 ? "bio" : "start");

                // 6. The screens; their actions; a click no screen took goes to the board.
                bool uiTook = false;
                if (screens.Ready())
                {
                    nlohmann::json data = play ? play->UiData() : Play::StartData(def, canContinue, chosen);
                    data["warnings"] = screens.Problems();
                    // Labels over pawns in view: "thinking..." and the group numbers (Table.md).
                    data["labels"] = nlohmann::json::array();
                    if (play)
                    {
                        const auto label = [&](int guest, const std::string &text, float up) {
                            for (const PawnView &pawn : view.pawns)
                                if (pawn.guest == guest)
                                    if (const auto at = table.ScreenPoint(table.Above(def, view, guest, ToDraw(play->Sliding())) + glm::vec3{0.0F, up, 0.0F}, pixels))
                                        data["labels"].push_back({{"text", text}, {"at", {at->x, at->y}}});
                        };
                        for (const auto &[guest, number] : play->Rings())
                            label(guest, std::to_string(number), 0.0F);
                        if (play->ThinkingGuest() >= 0)
                            label(play->ThinkingGuest(), "thinking...", 0.5F);
                        for (const nlohmann::json &p : data["popups"])
                            label(p["guest"].get<int>(), p["text"].get<std::string>(), 0.9F);
                        // Each figure's portrait and name, above its arrow (Table.md "Labels").
                        data["names"] = nlohmann::json::array();
                        const float lift = 0.0F; // just over the head; the arrow floats above it
                        for (const PawnView &pawn : view.pawns)
                            if (const auto at = table.ScreenPoint(table.Above(def, view, pawn.guest, ToDraw(play->Sliding())) + glm::vec3{0.0F, lift, 0.0F}, pixels))
                                data["names"].push_back({{"text", ShownName(def, play->State(), pawn.guest)},
                                                         {"portrait", ArtPath("portraits", def.guests[static_cast<std::size_t>(pawn.guest)].id).generic_string()},
                                                         {"at", {at->x - 14.0F, at->y - 28.0F}}});
                    }
                    for (const umbra::ui::UiAction &action : screens.Ui().Update(data, pixels, uiInput, drawList.ui))
                    {
                        sounds.Play("click");
                        uiTook = true;
                        NightState restored;
                        if (action.action == "quit")
                            quit = true;
                        else if (action.screen == "start" && action.action == "pick") // a guest's page first (Play.md)
                            chosen = NightDef::Find(def.guests, action.arg);
                        else if (action.screen == "start" && action.action == "random")
                            chosen = static_cast<int>(random() % def.guests.size());
                        else if (action.screen == "bio" && action.action == "back")
                            chosen = -1;
                        else if (action.screen == "bio" && action.action == "begin")
                            begin(std::exchange(chosen, -1), nullptr);
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
                // A board click is the left button let go without a drag, pressed where no screen took it.
                if (uiInput.click)
                    pressTook = uiTook;
                if (play && wasHeld && !input.IsMouseButtonHeld(0) && !dragging && !pressTook)
                {
                    // In the room view a figure or an item takes the click; else the room under it (walk there).
                    // A waiting bot beat: any click on the board goes on (mouse first, user).
                    const std::optional<Table::Thing> thing = table.PickThing(uiInput.mouse, pixels);
                    bool took = false; // a thing that cannot be engaged lets the click walk to its room
                    if (play->WaitingForTap())
                        (void)play->HandleUi("caption", "next", ""), took = true;
                    else if (thing && thing->kind == Table::ThingKind::Item)
                        took = play->ClickItem(thing->id);
                    else if (thing && thing->kind == Table::ThingKind::Guest && thing->id != play->State().player)
                        took = play->ClickGuest(thing->id);
                    else if (thing && thing->kind == Table::ThingKind::Monster)
                        took = play->ClickMonster(thing->id);
                    if (!took)
                        play->Click(table.Pick(def, uiInput.mouse, pixels));
                }
                wasHeld = input.IsMouseButtonHeld(0), lastMouse = uiInput.mouse;
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
    // --night (the old night; else the faction match), --seed N, --player <guest id>, --watch, --look, --capture <png>, --frames N.
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
        else if (arg == "--look")
            options.look = true;
        else if (arg == "--capture" && more)
            options.capture = argv[++i];
        else if (arg == "--frames" && more)
            options.frames = std::atoi(argv[++i]);
        else if (arg == "--night")
            options.night = true;
        else if (arg == "--cafe")
            options.cafe = true;
        else if (arg == "--zoom" && more)
            options.zoom = static_cast<float>(std::atof(argv[++i]));
    }
    umbra::diagnostics::Initialize();
    umbra::diagnostics::InstallCrashHandlers();
    const int result = Run(options);
    umbra::diagnostics::UninstallCrashHandlers();
    umbra::diagnostics::Shutdown();
    return result;
}
