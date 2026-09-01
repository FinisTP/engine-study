#pragma once

#include <memory>

namespace umbra::platform
{
    class Input;
    class Window;
}

namespace umbra::render
{
    class Renderer final
    {
    public:
        Renderer();
        ~Renderer();

        Renderer(const Renderer &) = delete;
        Renderer &operator=(const Renderer &) = delete;
        Renderer(Renderer &&) = delete;
        Renderer &operator=(Renderer &&) = delete;

        [[nodiscard]] bool Initialize(platform::Window &window);
        [[nodiscard]] bool RenderFrame(const platform::Input &input, float deltaSeconds);
        void RequestResize() noexcept;
        void Shutdown() noexcept;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation_;
    };
}
