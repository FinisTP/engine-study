#include "MatchApp.hpp"
#include "PadLegend.hpp"

#include "CareerMatch.hpp"
#include "GameContext.hpp"

#include "MatchPlay.hpp"
#include "MatchTable.hpp"
#include "Effects.hpp"
#include "Localization.hpp"
#include "Unease.hpp"
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
    words = umbra::night::LoadWords(assets / "localization.csv", "match", !hooks || analog.english); // localization.csv (the user, 2026-11-24 19:40)
    if (hooks)
        settingsWords = umbra::night::LoadWords(assets / "localization.csv", "campaign", analog.english);
    for (const umbra::core::Diagnostic &d : diagnostics)
        umbra::diagnostics::Error(umbra::core::Format(d));
    if (!loaded || words.is_discarded())
        return 1;
    // A stage: ability names in Vietnamese and each one's story (campaign-words.json "abilities", "abilityLore").
    if (hooks && settingsWords.is_object())
    {
        for (umbra::match::AbilityDef &a : def.abilities)
            if (settingsWords.contains("abilities") && settingsWords["abilities"].contains(a.id))
                a.name = settingsWords["abilities"][a.id].get<std::string>();
        words["lore"] = settingsWords.value("abilityLore", nlohmann::json::object());
    }

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
    if (const float across = pace.stageTilesAcross; hooks && across > 0.0F) // a stage opens on about this many tiles across (note 5)
        table.SetCamera(table.DistanceAcross(across), table.DistanceAcross(across) * 0.7F, pace.matchCameraDistance * 1.6F, pace.matchCameraTilt);
    umbra::core::DrawList drawList;
    // Effects and how told lines play out (Sequences.md): looks only; "Giam" halves the particles.
    Effects effects;
    effects.Load(assets, renderer, analog.reducedFx);
    bool effectsReduced = analog.reducedFx;
    Unease unease;          // the tear, the lamps, a portrait's twitch (point 4)
    int lastActor = -1;
    bool heavy = false;
    Sequences sequences;
    if (std::ifstream in(assets / "vfx" / "sequences.json"); in)
        if (std::vector<std::string> errors; !LoadSequences(nlohmann::json::parse(in, nullptr, false), sequences, errors))
            for (const std::string &e : errors)
                umbra::diagnostics::Error("vfx/sequences.json: " + e);

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
    // A stage's theme on the board, its bass-heavy cut in the battle view (note 17); the fight's when not made yet.
    const std::string boardMusic = hooks ? game.sounds->MusicOr(hooks->music, "fight") : "";
    const std::string battleMusic = hooks ? game.sounds->MusicOr(hooks->music + "_battle", boardMusic) : "";
    if (hooks && battleMusic == boardMusic) // no battle cut yet: the battle view keeps the board's music
        umbra::diagnostics::Info("no audio/music/" + hooks->music + "_battle.ogg: the battle view plays " + boardMusic);
    if (hooks)
        game.sounds->Music(boardMusic);
    int savedRound = -1;
    bool ended = false;  // the end scene has started
    int padLook = 0;     // L1/R1: which of the turn order the camera last looked at
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
            screens.Ui().Open("match-labels"), screens.Ui().Open(hudName), screens.Ui().Open("pad-legend");
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
    BoardCursor cursor; // keys and the gamepad (P4); the mouse takes over when it moves
    bool cursorOn = false;
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
        const bool back = game.keys.Pressed(input, "back") || game.keys.Pressed(input, "pause") || (hooks && input.IsMouseButtonPressed(2));
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
        // The pad (gamepad plan): Square opens the skills, L1/R1 look along the turn order, the right
        // stick pans, L2/R2 zoom.
        if (play && hooks && !paused && ours && input.IsKeyPressed(umbra::platform::InputKey::PadX))
            (void)play->MenuPick("skills");
        if (play && hooks && !paused && (input.IsKeyPressed(umbra::platform::InputKey::PadL1) || input.IsKeyPressed(umbra::platform::InputKey::PadR1)))
            if (const nlohmann::json strip = play->HudData().value("strip", nlohmann::json::array()); !strip.empty())
            {
                padLook = (padLook + (input.IsKeyPressed(umbra::platform::InputKey::PadR1) ? 1 : static_cast<int>(strip.size()) - 1)) % static_cast<int>(strip.size());
                table.LookAt(play->State(), strip[static_cast<std::size_t>(padLook)].value("unit", -1));
            }
        if (const glm::vec2 stick{input.Axis(2), input.Axis(3)}; play && glm::length(stick) > 0.0F)
            table.Pan(-stick * 900.0F * dt), table.StopFollow();
        zoom += (input.Axis(5) - input.Axis(4)) * 6.0F * dt;

        // 5. The match: bots and monsters play on beats; the board follows the mouse.
        int hover = -1;
        if (play)
        {
            // A stage's board first finishes its opening shot: nothing plays, no scene starts, until it has (note 4).
            const bool opening = hooks && table.InScene();
            // Hold Space on their turn to hurry the beats (P16); effects keep the same pace.
            const float pdt = paused || opening || table.Easing() ? 0.0F : hooks && !play->PlayerTurn() ? dt * analog.EnemyScale() * (game.keys.Held(input, "hurry") ? 3.0F : 1.0F) : dt;
            play->SetSequences(&sequences);
            if (pdt > 0.0F)
                play->Update(pdt);
            if (hooks && hooks->beat && !paused && !opening)
            {
                // A stage's scenes, between beats, over the board: the camera close on whoever speaks; a narrated
                // line keeps the last speaker in view.
                std::string last;
                bool hidden = false; // a scene has the screen: the fight's screens step away until it ends (note 1)
                const SceneBehind board = [&](umbra::core::DrawList &out, float sdt, const std::string &speaker) {
                    if (!std::exchange(hidden, true) && screens.Ready())
                        for (const std::string &name : {shown, hudName, std::string("match-labels"), std::string("pad-legend")})
                            if (!name.empty())
                                screens.Ui().Close(name);
                    const auto &names = play->Names();
                    if (!speaker.empty())
                        last = speaker;
                    if (const auto it = std::ranges::find(names, last); !last.empty() && it != names.end())
                        table.LookAt(play->State(), static_cast<int>(it - names.begin())), table.CloseUp(true);
                    table.Update(sdt);
                    table.Draw(play->State(), nlohmann::json::object(), -1, out);
                };
                hooks->beat(*play, board);
                table.CloseUp(false);
                if (hidden && screens.Ready()) // back as they were; the frame's screen opens again below
                {
                    shown.clear(), screens.Ui().Open("match-labels"), screens.Ui().Open("pad-legend");
                    if (hudOpen)
                        screens.Ui().Open(hudName);
                }
            }
            // What was just told: figures move, sounds play.
            for (const auto &[unit, motion] : play->TakeMoves())
            {
                table.Play(unit, motion);
                if (analog.rumble && (motion == "hit" || motion == "down")) // the pad feels blows and falls
                    umbra::platform::Rumble(motion == "down" ? 0.6F : 0.3F, motion == "down" ? 260 : 110);
            }
            for (const std::string &cue : play->TakeSounds())
                sounds.Play(cue);
            for (const int unit : play->TakeSnaps())
                table.Snap(unit); // an undone walk jumps back
            // The sequences' effects at whoever they name; a shake only when effects are full.
            const MatchState &st = play->State();
            const auto on = [&](int unit) { return unit >= 0 && unit < static_cast<int>(st.units.size()); };
            for (const MatchPlay::Fx &fx : play->TakeEffects())
                if (fx.shake > 0.0F)
                    heavy |= fx.shake >= 0.3F, analog.reducedFx ? void() : table.Shake(fx.shake);
                else if (on(fx.unit) && on(fx.to))
                    effects.Fly(fx.effect, table.Body(st, fx.unit), table.Body(st, fx.to), fx.seconds);
                else if (on(fx.unit))
                    effects.Burst(fx.effect, table.Body(st, fx.unit));
            // Dread (note 18): dark smoke at a ghost's feet and a red glint at its eyes, while it stands.
            for (int u = 0; hooks && u < static_cast<int>(st.units.size()); ++u)
                if (st.units[static_cast<std::size_t>(u)].faction == 1 && umbra::match::Up(st.units[static_cast<std::size_t>(u)]))
                {
                    effects.Follow(static_cast<std::uint64_t>(u) + 1, "dread", table.FeetShown(st, u));
                    if (!analog.reducedFx)
                        effects.Follow(static_cast<std::uint64_t>(u) + 1 + (1ULL << 20), "eye_glint", glm::mix(table.FeetShown(st, u), table.Head(st, u), 0.88F));
                }
            // Room unease: dust drifting in the lamps' light over the board.
            if (hooks)
                effects.Follow(1ULL << 30, "room_dust", table.Body(st, std::max(0, Current(st))) + glm::vec3{0.0F, 0.6F, 0.0F});
            effects.EndFrame();
            effects.Update(pdt);
            // An enemy's turn tears the picture a moment; the lamps stutter; a waiting portrait twitches.
            const int actor = Current(st);
            const bool enemyTurn = hooks && actor >= 0 && actor != std::exchange(lastActor, actor) && st.units[static_cast<std::size_t>(actor)].faction == 1;
            unease.Update(hooks ? pdt : 0.0F, enemyTurn, std::exchange(heavy, false), analog.reducedFx,
                          static_cast<int>(play->HudData().value("strip", nlohmann::json::array()).size()));
            table.SetDread(analog.reducedFx ? 0.5F : 1.0F);
            {
                const nlohmann::json told = play->UiData();
                table.SetFight(told.value("fight", std::vector<int>{}), told.value("picking", -1));
                if (hooks) // the battle view: the same theme with more bass (Music crossfades only on a change)
                    game.sounds->Music(told.contains("fight") ? battleMusic : boardMusic, 0.5F, true); // the same moment of the same music
                table.SetTold(told.value("standing", std::vector<int>{}), told.value("hp", std::vector<int>{}));
            }
            table.SetFocus(play->UiData().value("focus", std::vector<int>{}));
            if (hooks)
                table.SetCast(play->Names(), play->Heights(), play->Sprites()); // a stage's cast names and ghost heights
            table.Animate(play->State(), dt);
            // A screen over the board (level, treasure, event, battle) takes the mouse: nothing falls through to the board.
            // P4: WASD or the left stick move the board cursor (arrows and the d-pad stay with the menu);
            // Enter, Space or A then acts on its square, as a click would.
            if (hooks && !paused && play->Screen().empty())
            {
                using umbra::platform::InputKey;
                const auto key = [&](InputKey k) { return input.IsKeyHeld(k) ? 1 : 0; };
                const glm::ivec2 dir{key(InputKey::D) - key(InputKey::A) + (input.Axis(0) > 0.5F) - (input.Axis(0) < -0.5F),
                                     key(InputKey::S) - key(InputKey::W) + (input.Axis(1) > 0.5F) - (input.Axis(1) < -0.5F)};
                const int actor = Current(play->State());
                const int start = actor >= 0 ? play->State().units[static_cast<std::size_t>(actor)].cell : 0;
                cursorOn |= cursor.Update(glm::clamp(dir, glm::ivec2{-1}, glm::ivec2{1}), dt, play->State().board.width, play->State().board.height, start);
                if (uiInput.mouseMoved)
                    cursorOn = false, cursor.Off();
                if (cursorOn)
                    uiInput.confirm = false; // Enter / A go to the cursor's square, not the menu's focus
            }
            hover = !(play->Screen().empty() && !paused) ? -1 : cursorOn ? cursor.cell : table.Pick(play->State().board, uiInput.mouse, pixels);
            play->Hover(hover);
            const bool held = input.IsMouseButtonHeld(0);
            if (held && !wasHeld)
                pressAt = uiInput.mouse, dragging = false;
            if (held && !dragging && glm::length(uiInput.mouse - pressAt) > 4.0F)
                dragging = true;
            if (held && dragging)
                table.Pan(uiInput.mouse - lastMouse), table.StopFollow();
            if (play->State().over && !ended) // a campaign stage leaves for its result instead of the turning shot
                ended = true, hooks ? void() : table.StartScene("end", play->State().board.hub[static_cast<std::size_t>(std::max(0, play->State().winner))]);
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
            table.Draw(play->State(), ui, hover, drawList), effects.Extract(drawList);
        const bool opening = hooks && play && table.InScene(); // the board's opening shot: no screen, no HUD yet
        const std::string screen = paused ? (hooks ? "campaign-pause" : "pause") : opening ? "" : play ? (play->Screen().empty() ? "" : "match-" + play->Screen()) : "match-pick";
        show(screen);
        // A staged fight has the screen to itself: the HUD steps away (the user, 2026-11-20).
        // A campaign stage that is over: the HUD and labels go at once, before its end scene and the result.
        if (const bool hud = play && !opening && !ui.contains("fight") && !(hooks && play->State().over); screens.Ready() && play && hud != hudOpen)
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
                if (nlohmann::json &strip = data["hud"]["strip"]; unease.twitch >= 0 && unease.twitch < static_cast<int>(strip.size()))
                    if (std::string face = strip[static_cast<std::size_t>(unease.twitch)].value("portrait", ""); face.ends_with("face_calm.png"))
                        strip[static_cast<std::size_t>(unease.twitch)]["portrait"] = face.replace(face.size() - 13, 13, "face_neardead.png");
            data["career"] = career != nullptr; // the end screen leads back to the café
            data["padOn"] = hooks && input.PadLast();
            if (play && play->PlayerTurn())
                data["pad"] = PadLegend(data["words"], {{"cross", "padPick"}, {"circle", "padBack"}, {"triangle", "padEnd"}, {"square", "padSkills"},
                                                        {"l1", "padOrder"}, {"rstick", "padCamera"}, {"r2", "padZoom"}, {"options", "padPause"}});
            else
                data["pad"] = PadLegend(data["words"], {{"square", "padSkip"}, {"cross", "padHurry"}, {"l1", "padOrder"}, {"options", "padPause"}});
            data["single"] = career == nullptr;
            if (!play)
                data["pick"] = MatchPlay::PickData(def, picks, canContinue, level);
            else
            {
                // Spoken lines ride on their speaker's card as a bubble.
                data["labels"] = table.Labels(play->State(), pixels);
                // Numbers rise over whoever a blow or a heal reached (note 21).
                data["pops"] = nlohmann::json::array();
                for (const auto &pop : ui.value("pops", nlohmann::json::array()))
                    if (const auto at = table.HeadOnScreen(play->State(), pop["unit"], pixels))
                        data["pops"].push_back({{"at", {at->x, at->y - (20.0F + 70.0F * pop["rise"].get<float>()) * screens.Ui().UiScale()}},
                                                {"text", pop["text"]}, {"hurt", pop["hurt"]}, {"heal", pop["heal"]}});
                // Armed: the card at the hovered target says what the action would do (no menu while aiming).
                if (hooks && ui.contains("forecast") && ui["forecast"].is_object() && ui["forecast"].value("kind", "") == "target")
                    if (const auto at = table.HeadOnScreen(play->State(), ui["forecast"].value("unit", -1), pixels))
                        data["aim"] = {{{"at", {at->x, at->y - 24.0F * screens.Ui().UiScale()}}, {"name", ui["forecast"].value("name", "")}, {"line", ui["forecast"]["line"]}}};
                // The actions dock beside the one acting (to the right of its head).
                // The menu waits until the walk has arrived, then stands clear of the figure: its near edge a
                // little right of the head (left when that would leave the screen), in the UI's own scale.
                const int actor = Current(play->State());
                const float k = screens.Ui().UiScale(), half = 150.0F * k, gap = 70.0F * k;
                const bool menuShown = hooks ? !data["menu"]["rows"].empty() || !data["menu"].value("title", std::string()).empty() : ui.contains("actions");
                if (menuShown && !table.Walking(actor)) // no empty box while aiming
                    if (const auto at = table.HeadOnScreen(play->State(), actor, pixels))
                    {
                        const float right = at->x + gap + half, left = at->x - gap - half;
                        data["dock"] = {{{"at", {right + half < pixels.x ? right : left, at->y + 190.0F * k}}}};
                    }
                if (hooks && table.Walking(actor))
                    data["menu"]["rows"] = nlohmann::json::array();
                for (auto &l : data["labels"])
                    for (const auto &b : ui["bubbles"])
                        if (b["unit"] == l["unit"])
                            l["say"] = b["text"];
            }
            data["tip"] = screens.Ui().Tip();
            // A marked square under the mouse says what will happen there (note 3).
            if (data["tip"] == "" && ui.contains("forecast") && ui["forecast"].is_object() && ui["forecast"].value("kind", "") == "telegraph")
                data["tip"] = ui["forecast"]["text"];
            if (data["tip"] == "" && ui.contains("guide")) // a tutorial's guided step says what to do until it is done
                data["tip"] = ui["guide"]["tip"];
            data["analog"] = analog.Data();
            window.SetTextInput(screens.Ui().WantsText()); // typed text only while a text field has the focus
            game.Keep(drawList); // the place, for a screen opened from it (settings, the result)
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
                    if (std::exchange(effectsReduced, analog.reducedFx) != analog.reducedFx)
                        effects.Clear(), effects.Load(assets, renderer, analog.reducedFx);
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
                    table.LookAt(play->State(), std::stoi(a.arg)), play->Look(std::stoi(a.arg)); // and its card; the acting one: the panel back
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
        else if (play && !paused && cursorOn && play->Screen().empty() && play->PlayerTurn() &&
                 (input.IsKeyPressed(umbra::platform::InputKey::Enter) || input.IsKeyPressed(umbra::platform::InputKey::Space) || input.IsKeyPressed(umbra::platform::InputKey::PadA)))
            play->Click(cursor.cell);
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
        drawList.post.tear = unease.tear, drawList.post.exposure -= unease.dim;
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
