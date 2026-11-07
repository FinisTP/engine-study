// Umbra Editor: edits scenes and RPG content through umbra::authoring, previews them with the
// game's renderer, and starts the game on the open scene. See Editor.md.
// usage: UmbraEditor [scene path under the asset folder]

#include "Editor.hpp"

#include <ApplicationEvents.hpp>
#include <CrashHandler.hpp>
#include <EventPump.hpp>
#include <FrameClock.hpp>
#include <Input.hpp>
#include <Log.hpp>
#include <Platform.hpp>
#include <Renderer.hpp>
#include <Window.hpp>

#include <imgui.h>
#include <imgui_impl_sdl3.h>

#include <SDL3/SDL.h>

#include <chrono>
#include <exception>
#include <string>
#include <thread>

namespace
{
    // Startup problems are shown in a message box too: started by double-click or editor.bat the
    // console closes with the program, so the log alone would never be seen.
    int Fail(const std::string &message)
    {
        umbra::diagnostics::Critical(message);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Umbra Editor", message.c_str(), nullptr);
        return 1;
    }

    int Run(const std::string &scenePath)
    {
        using umbra::platform::ApplicationEventType;
        umbra::platform::Platform platform;
        if (!platform.Initialize())
            return Fail("SDL could not start (see the console log).");
        int result = 0;
        {
            umbra::platform::Window window;
            umbra::render::Renderer renderer;
            if (!window.Create("Umbra Editor", 1600, 900))
                return Fail("The editor window could not be created.");
            if (!renderer.Initialize(window))
                return Fail("The Vulkan renderer could not start (see the console log).");

            // 1. Dear ImGui gets real mouse and keyboard input through its SDL backend.
            ImGui_ImplSDL3_InitForVulkan(window.GetNativeHandle());
            ImGui::GetIO().IniFilename = "umbra-editor.ini"; // window layout, separate from the game
            umbra::platform::EventPump events;
            events.SetRawEventHandler([](const void *event) { ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event *>(event)); });

            // 2. The editor and its first scene.
            ModelLibrary models(renderer, UMBRA_ASSET_DIRECTORY, UMBRA_CACHE_DIRECTORY);
            {
                Editor editor(models, UMBRA_ASSET_DIRECTORY, UMBRA_GAME_EXECUTABLE);
                editor.OpenScene(scenePath);
                umbra::platform::Input input;
                umbra::platform::ApplicationEvents app;
                umbra::core::FrameClock clock;
                umbra::core::DrawList drawList;

                // 3. Frame loop: events -> editor -> render with the editor's windows.
                while (!editor.WantsQuit())
                {
                    input.BeginFrame();
                    app.BeginFrame();
                    events.PumpEvents(input, app);
                    if (app.IsEventRequested(ApplicationEventType::Quit) || app.IsEventRequested(ApplicationEventType::WindowClose))
                        editor.RequestQuit();
                    if (app.IsEventRequested(ApplicationEventType::WindowResize))
                    {
                        window.UpdateSizeFromEvent(app.GetWindowWidth(), app.GetWindowHeight());
                        renderer.RequestResize();
                    }
                    if (app.IsEventRequested(ApplicationEventType::WindowPixelSizeChanged))
                        renderer.RequestResize();

                    clock.Tick();
                    const float dt = static_cast<float>(clock.GetDeltaTimeSeconds());
                    editor.Update(dt, drawList);
                    ImGui_ImplSDL3_NewFrame();
                    if (!renderer.RenderFrame(dt, drawList, [&] { editor.DrawUi(); }))
                    {
                        result = 1;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds{1});
                }
            }
            ImGui_ImplSDL3_Shutdown();
        }
        platform.Shutdown();
        return result;
    }
}

int main(int argc, char **argv)
{
    umbra::diagnostics::Initialize();
    umbra::diagnostics::InstallCrashHandlers();
    int result = 1;
    try
    {
        result = Run(argc > 1 ? argv[1] : "scenes/settlement.scene.json");
    }
    catch (const std::exception &error)
    {
        result = Fail(std::string{"The editor stopped: "} + error.what());
    }
    umbra::diagnostics::UninstallCrashHandlers();
    umbra::diagnostics::Shutdown();
    return result;
}
