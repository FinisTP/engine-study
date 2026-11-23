#include <Window.hpp>
#include <string>
#include <SDL3/SDL.h>
#include <Log.hpp>

namespace umbra::platform
{
    Window::~Window()
    {
        Destroy();
    }

    bool Window::Create(std::string_view title, int width, int height)
    {
        if (window_ != nullptr)
        {
            diagnostics::Error("Cannot create window: Window is already created.");
            return false;
        }

        if (width <= 0 || height <= 0)
        {
            diagnostics::Error("Cannot create window: Dimensions must be positive.");
            return false;
        }

        const std::string windowTitle{title};
#ifdef __EMSCRIPTEN__
        window_ = SDL_CreateWindow(windowTitle.c_str(), width, height, SDL_WINDOW_RESIZABLE); // the page's canvas; WebGPU draws into it
#else
        window_ = SDL_CreateWindow(windowTitle.c_str(), width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
#endif

        if (window_ == nullptr)
        {
            diagnostics::Error(std::string{"Window creation failed: "} + SDL_GetError());
            return false;
        }

        width_ = width;
        height_ = height;

        diagnostics::Info("Window created.");
        return true;
    }

    void Window::Destroy()
    {
        if (window_ == nullptr)
        {
            return;
        }

        SDL_DestroyWindow(window_);
        window_ = nullptr;
        width_ = 0;
        height_ = 0;

        diagnostics::Info("Window destroyed.");
    }

    bool Window::Resize(int width, int height)
    {
        if (window_ == nullptr)
        {
            diagnostics::Error("Cannot resize window: Window is not created.");
            return false;
        }

        if (width <= 0 || height <= 0)
        {
            diagnostics::Error("Cannot resize window: Dimensions must be positive.");
            return false;
        }

        if (!SDL_SetWindowSize(window_, width, height))
        {
            diagnostics::Error(std::string{"Failed to resize window: "} + SDL_GetError());
            return false;
        }

        width_ = width;
        height_ = height;

        diagnostics::Info("Window resized to " + std::to_string(width) + "x" + std::to_string(height) + ".");
        return true;
    }

    void Window::UpdateSizeFromEvent(int width, int height)
    {
        if (width <= 0 || height <= 0)
        {
            diagnostics::Error("Cannot update window size from event: Dimensions must be positive.");
            return;
        }

        width_ = width;
        height_ = height;

        diagnostics::Info("Window size updated from event to " + std::to_string(width) + "x" + std::to_string(height) + ".");
    }

    void Window::SetMouseCaptured(bool captured) noexcept
    {
        if (window_ != nullptr)
        {
            SDL_SetWindowRelativeMouseMode(window_, captured);
        }
    }

    void Window::SetTextInput(bool on) noexcept
    {
        // 1. Only on a change: starting text input again would reset an IME's half-typed word.
        if (window_ == nullptr || SDL_TextInputActive(window_) == on)
            return;
        if (on)
            SDL_StartTextInput(window_);
        else
            SDL_StopTextInput(window_);
    }
}
