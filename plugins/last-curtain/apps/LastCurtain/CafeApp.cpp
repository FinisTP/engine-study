#include "CafeApp.hpp"

#include "CafePlay.hpp"
#include "CafeRoom.hpp"
#include "GameContext.hpp"
#include "Analog.hpp"

#include <Log.hpp>
#include <MatchDef.hpp>
#include <TextureTools.hpp>

#include <chrono>
#include <fstream>
#include <thread>

using umbra::platform::InputKey;

int RunCafe(GameContext &game, const umbra::assets::AssetFolders &assets, umbra::cafe::Career &career, const std::filesystem::path &careerFile,
            const std::filesystem::path &capture, int frames)
{
    // 1. The café's data, the match's characters, the words and the room.
    umbra::cafe::CafeDef def;
    umbra::match::MatchDef match;
    std::vector<umbra::core::Diagnostic> diagnostics;
    const bool loaded = umbra::cafe::LoadCafe(assets.RootOf("cafe/cafe.json") / "cafe", def, diagnostics) &&
                        umbra::match::LoadMatch(assets.RootOf("match/rules.json") / "match", match, diagnostics);
    nlohmann::json words, room;
    if (std::ifstream in(assets / "cafe-words.json"); in)
        words = nlohmann::json::parse(in, nullptr, false);
    if (std::ifstream in(assets.RootOf("cafe/room.json") / "cafe/room.json"); in)
        room = nlohmann::json::parse(in, nullptr, false);
    for (const umbra::core::Diagnostic &d : diagnostics)
        umbra::diagnostics::Error(umbra::core::Format(d));
    if (!loaded || words.is_discarded() || room.is_discarded() || match.characters.empty())
        return 0;
    umbra::night::GameOptions options; // only for the analog screen's strengths (Analog.hpp)
    std::vector<umbra::core::Diagnostic> ignored;
    if (!umbra::night::LoadOptions(assets / "options.csv", options, ignored))
        options = {};
    const umbra::night::AnalogScreen analog = umbra::night::LoadAnalog(umbra::night::AnalogPath(), options);

    // 2. The walker: the first of the company the career knows, else the first character.
    const std::string walker = !career.characters.empty() ? career.characters.front().id : match.characters.front().id;
    umbra::night::CafePlay play(def, match, room, words, career, walker, static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count()));
    CafeRoom view(game.renderer, assets, def, match);
    Screens &screens = *game.screens;
    if (screens.Ready())
        screens.Ui().Open("cafe-labels"), screens.Ui().Open("cafe-hud");
    std::string shown;
    const auto show = [&](const std::string &want) {
        if (!screens.Ready() || want == shown)
            return;
        if (!shown.empty())
            screens.Ui().Close(shown);
        if (!want.empty())
            screens.Ui().Open(want), screens.Ui().ResetFocus(want);
        shown = want;
    };
    const auto save = [&] {
        std::string error;
        if (!careerFile.empty() && !umbra::cafe::SaveCareer(careerFile, career, error))
            umbra::diagnostics::Error("The career was not saved: " + error);
    };

    // 3. The loop: walk, E, the screens; the stage door ends it (1), the window's close quits (0).
    umbra::core::DrawList drawList;
    const int lastFrame = frames > 0 ? frames : (capture.empty() ? 0 : 3);
    int frame = 0, result = 0;
    bool captured = false, quit = false;
    while (!quit)
    {
        game.input.BeginFrame();
        game.events.BeginFrame();
        game.pump.PumpEvents(game.input, game.events);
        using umbra::platform::ApplicationEventType;
        if (game.events.IsEventRequested(ApplicationEventType::Quit) || game.events.IsEventRequested(ApplicationEventType::WindowClose))
            break;
        if (game.events.IsEventRequested(ApplicationEventType::WindowResize))
            game.window.UpdateSizeFromEvent(game.events.GetWindowWidth(), game.events.GetWindowHeight()), game.renderer.RequestResize();
        if (game.events.IsEventRequested(ApplicationEventType::WindowPixelSizeChanged))
            game.renderer.RequestResize();
        game.clock.Tick();
        const auto dt = static_cast<float>(game.clock.GetDeltaTimeSeconds());
        drawList.Clear();
        drawList.hud.diagnosticsWindow = false;
        const auto [width, height] = game.renderer.Extent();
        const glm::vec2 pixels{static_cast<float>(width), static_cast<float>(height)};
        umbra::ui::UiInput uiInput = Screens::MapInput(game.input, {static_cast<float>(game.window.GetWidth()), static_cast<float>(game.window.GetHeight())}, pixels);

        // 4. Walking (WASD or the arrows, a click on the floor) and E, while no screen is up.
        const bool free = play.Screen().empty();
        if (free)
        {
            const auto held = [&](InputKey a, InputKey b) { return game.input.IsKeyHeld(a) || game.input.IsKeyHeld(b); };
            const glm::vec2 move{(held(InputKey::D, InputKey::Right) ? 1.0F : 0.0F) - (held(InputKey::A, InputKey::Left) ? 1.0F : 0.0F),
                                 (held(InputKey::S, InputKey::Down) ? 1.0F : 0.0F) - (held(InputKey::W, InputKey::Up) ? 1.0F : 0.0F)};
            play.Move(move, dt);
            if (game.input.IsKeyPressed(InputKey::E) || game.input.IsKeyPressed(InputKey::Space))
                (void)play.Interact();
        }
        if (!capture.empty() && frame == 1)
            play.OpenShop(); // a capture shows the shop at work
        play.Update(capture.empty() ? dt : 0.5F); // ... and its guests come in quickly
        view.Update(play, dt);
        for (const std::string &cue : play.TakeSounds())
            game.sounds->Play(cue);
        view.Draw(play, drawList);
        show(play.Screen().empty() ? "" : "cafe-" + play.Screen());

        // 5. The screens, then a click no screen took walks to the floor there.
        bool uiTook = false;
        if (screens.Ready())
        {
            nlohmann::json data = play.UiData();
            data["labels"] = view.Labels(play, pixels);
            data["tip"] = screens.Ui().Tip();
            game.window.SetTextInput(screens.Ui().WantsText()); // typed text only while a text field has the focus
            for (const umbra::ui::UiAction &a : screens.Ui().Update(data, pixels, uiInput, drawList.ui))
            {
                uiTook = true;
                game.sounds->Play("click");
                (void)play.HandleUi(a.screen.substr(a.screen.starts_with("cafe-") ? 5 : 0), a.action, a.arg);
            }
        }
        if (uiInput.click && !uiTook && free)
            if (const auto at = view.Floor(uiInput.mouse, pixels, {room["size"][0].get<float>(), room["size"][1].get<float>()}))
                play.WalkTo(*at);
        if (play.Leaving())
            result = 1, quit = true;

        // 6. A capture: save the last frame, then stop.
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
    // 7. The day so far is kept; the café's screens close for the match.
    save();
    if (screens.Ready())
        for (const char *name : {"cafe-hud", "cafe-labels", "cafe-board", "cafe-takings"})
            screens.Ui().Close(name);
    return result;
}
