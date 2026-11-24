#include "DialogApp.hpp"
#include "CampaignText.hpp"
#include "Localization.hpp"

#include "Analog.hpp"
#include "DialogView.hpp"
#include "GameContext.hpp"

#include <Log.hpp>
#include <Options.hpp>
#include <TextureTools.hpp>

#include <DrawList.hpp>

#include <chrono>
#include <fstream>
#include <thread>


int RunScene(GameContext &game, const umbra::assets::AssetFolders &assets, const std::string &sceneId, const std::string &backdrop,
             const umbra::campaign::Speaker &player, umbra::campaign::Facts &facts, const std::filesystem::path &capture, int frames,
             const SceneBehind &behind)
{
    // 1. The cast, the scene, the words.
    umbra::campaign::Cast cast;
    umbra::campaign::Scene scene;
    std::vector<umbra::core::Diagnostic> diagnostics;
    const std::filesystem::path campaign = assets.RootOf("campaign/cast.csv") / "campaign";
    const bool loaded = umbra::campaign::LoadCast(campaign / "cast.csv", cast, diagnostics) &&
                        umbra::campaign::LoadScene(campaign / "scenes" / (sceneId + ".json"), cast, scene, diagnostics);
    for (const umbra::core::Diagnostic &d : diagnostics)
        umbra::diagnostics::Error(umbra::core::Format(d));
    if (umbra::night::EnglishChosen())
        umbra::campaign::UseEnglish(scene); // the lines in the chosen language (Localization.md)
    if (CampaignText text; text.Load(assets / "localization.csv"))
        text.Fill(cast); // speakers' names
    nlohmann::json words;
    words = umbra::night::LoadWords(assets / "localization.csv", "campaign", umbra::night::EnglishChosen());
    Screens &screens = *game.screens;
    if (!loaded || words.is_discarded() || !screens.Ready())
        return 0;
    umbra::night::GameOptions options;
    std::vector<umbra::core::Diagnostic> ignored;
    if (!umbra::night::LoadOptions(assets / "options.csv", options, ignored))
        options = {};
    const umbra::night::AnalogScreen analog = umbra::night::LoadAnalog(umbra::night::AnalogPath(), options);
    const auto exists = [&](const std::string &path) { return std::filesystem::exists(assets.RootOf(path) / path); };
    const std::string backdropFile = backdrop.empty() ? std::string() : "art/backdrops/" + backdrop + ".jpg";

    // 2. The loop: the typewriter, clicks and keys; the log over it.
    umbra::campaign::ScenePlayer play(scene, cast, player, facts);
    screens.SetTextScale(analog.TextScale()); // large text, as the player left it
    screens.Ui().Open("dialog");
    bool logOpen = false;
    umbra::core::DrawList drawList;
    const int lastFrame = frames > 0 ? frames : (capture.empty() ? 0 : 3);
    int frame = 0, result = 0;
    bool captured = false;
    std::size_t typed = 0; // letters shown so far: a new one ticks the typing sound
    float tick = 0.0F;
    while (!play.Done())
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
        const umbra::ui::UiInput uiInput = Screens::MapInput(game.input, {static_cast<float>(game.window.GetWidth()), static_cast<float>(game.window.GetHeight())}, pixels);

        // 3. Keys (keys-keyboard.json): next advances, menu1-4 choose (the screen's clicks are its actions below).
        play.Update(dt, options.lettersPerSecond * analog.LetterScale()); // the player's text speed (Settings)
        if (!logOpen && game.keys.Pressed(game.input, "next"))
            play.Advance();
        for (int i = 0; i < 4 && !logOpen; ++i)
            if (game.keys.Pressed(game.input, "menu" + std::to_string(i + 1)))
                (void)play.Choose(i);
        if (!capture.empty() && frame == 1)
            play.Skip(); // a capture shows the first choice, or the end
        if (const std::string sound = play.TakeSound(); !sound.empty())
            game.sounds->Play(sound);
        // The typewriter's sound: a soft key every few letters while a line types (sounds.csv "type").
        const std::size_t letters = play.Shown().size();
        tick += dt;
        if (letters > typed && !play.Complete() && tick >= 0.07F)
            game.sounds->Play("type"), tick = 0.0F;
        typed = letters;
        if (backdropFile.empty() && behind)
            behind(drawList, dt, play.SpeakerName()); // the place itself, its camera on the speaker

        // 4. The screens' data and actions.
        nlohmann::json data = DialogData(play, cast, player, exists);
        data["dialog"]["backdrop"] = backdropFile;
        data["words"] = words;
        for (const umbra::ui::UiAction &a : screens.Ui().Update(data, pixels, uiInput, drawList.ui))
        {
            if (a.action == "next" && !play.Complete())
                play.Advance();
            else if (a.action == "next" && play.Choices().empty())
                play.Advance();
            else if (a.action == "choose")
                (void)play.Choose(std::atoi(a.arg.c_str()));
            else if (a.action == "skip")
                play.Skip();
            else if (a.action == "log")
                screens.Ui().Open("dialog-log"), logOpen = true;
            else if (a.action == "close")
                screens.Ui().Close("dialog-log"), logOpen = false;
        }

        // 5. A capture: save the last frame, then stop.
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
    if (play.Done())
        result = 1;
    screens.Ui().Close("dialog-log");
    screens.Ui().Close("dialog");
    return result;
}
