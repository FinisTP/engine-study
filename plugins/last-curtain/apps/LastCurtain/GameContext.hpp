#pragma once

#include "Analog.hpp"
#include "Keys.hpp"
#include "Screens.hpp"
#include "Sounds.hpp"

#include <ApplicationEvents.hpp>
#include <EventPump.hpp>
#include <FrameClock.hpp>
#include <Input.hpp>
#include <Log.hpp>
#include <Renderer.hpp>
#include <Window.hpp>

#include <fstream>
#include <memory>

// What the café and the match share for the whole run: one window, one renderer, the screens and
// the sounds (CafeApp.md). The café and the match take turns with it.
struct GameContext
{
    umbra::core::FrameClock clock;
    umbra::platform::Input input;
    umbra::platform::ApplicationEvents events;
    umbra::platform::EventPump pump;
    umbra::platform::Window window;
    umbra::render::Renderer renderer;
    std::unique_ptr<Screens> screens;
    std::unique_ptr<Sounds> sounds;
    umbra::night::Keys keys; // assets/keys-keyboard.json (Keys.hpp); defaults when missing or broken

    bool Open(const umbra::assets::AssetFolders &assets)
    {
        if (!window.Create("The Last Curtain", 1280, 720) || !renderer.Initialize(window))
            return false;
        screens = std::make_unique<Screens>(renderer, assets);
        sounds = std::make_unique<Sounds>(assets);
        std::vector<umbra::core::Diagnostic> d;
        if (std::ifstream in(assets / "keys-keyboard.json"); in && !keys.Load(nlohmann::json::parse(in, nullptr, false), "keys-keyboard.json", d))
            keys = {};
        for (const umbra::core::Diagnostic &e : d)
            umbra::diagnostics::Error(umbra::core::Format(e));
        return true;
    }

    // The player's settings that live outside any one screen: text size, full screen, effects volume.
    void Apply(const umbra::night::AnalogScreen &settings)
    {
        if (screens)
            screens->SetTextScale(settings.TextScale());
        if (sounds)
            sounds->SetVolume(static_cast<float>(settings.effects) / 100.0F);
        if (settings.fullScreen != fullScreen)
            window.SetFullScreen(settings.fullScreen), fullScreen = settings.fullScreen;
    }
    bool fullScreen = false;
};
