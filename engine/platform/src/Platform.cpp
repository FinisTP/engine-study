#include <umbra/platform/Platform.hpp>

#include <string>

#include <SDL3/SDL.h>

#include <umbra/diagnostics/Log.hpp>

namespace umbra::platform
{
    bool Initialize()
    {
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            diagnostics::Error(
                std::string{"SDL initialization failed: "} + SDL_GetError());

            return false;
        }

        diagnostics::Info("Platform initialized.");
        return true;
    }

    void Shutdown()
    {
        SDL_Quit();
        diagnostics::Info("Platform shut down.");
    }

    Window::~Window()
    {
        Destroy();
    }

    bool Window::Create(
        std::string_view title,
        int width,
        int height)
    {
        const std::string windowTitle{title};

        window_ = SDL_CreateWindow(
            windowTitle.c_str(),
            width,
            height,
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

        if (window_ == nullptr)
        {
            diagnostics::Error(
                std::string{"Window creation failed: "} + SDL_GetError());

            return false;
        }

        diagnostics::Info("Window created.");
        return true;
    }

    bool Window::PollCloseRequested()
    {
        SDL_Event event{};

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                return true;
            }

            if (event.type == SDL_EVENT_KEY_DOWN &&
                event.key.key == SDLK_ESCAPE)
            {
                return true;
            }
        }

        return false;
    }

    void Window::Destroy()
    {
        if (window_ == nullptr)
        {
            return;
        }

        SDL_DestroyWindow(window_);
        window_ = nullptr;

        diagnostics::Info("Window destroyed.");
    }
}