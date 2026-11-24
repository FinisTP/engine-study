

#pragma once

#include <string_view>

struct SDL_Window;

namespace umbra::platform
{
    class Window final
    {
    public:
        Window() = default;
        ~Window();

        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;

        [[nodiscard]] bool Create(std::string_view title, int width, int height);
        void Destroy();
        [[nodiscard]] bool Resize(int width, int height);
        // Captured: cursor hidden and locked, mouse motion stays relative (for camera control).
        void SetMouseCaptured(bool captured) noexcept;
        // Text input on: SDL sends typed text (Input::Text), keyboard layouts and IMEs such as
        // Vietnamese Telex included; phones show their keyboard. Off when no text field has focus.
        void SetTextInput(bool on) noexcept;
        // Full screen on the window's display (borderless, the desktop's size), or back to the window.
        void SetFullScreen(bool on) noexcept;

        [[nodiscard]] int GetWidth() const noexcept { return width_; }
        [[nodiscard]] int GetHeight() const noexcept { return height_; }
        void UpdateSizeFromEvent(int width, int height);
        
        [[nodiscard]] SDL_Window *GetNativeHandle() const noexcept
        {
            return window_;
        }

    private:
        SDL_Window *window_ = nullptr;
        int width_ = 0;
        int height_ = 0;
    };
}
