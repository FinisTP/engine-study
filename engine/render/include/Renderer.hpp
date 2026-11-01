#pragma once

#include <DrawList.hpp>

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
        // Draws every item in drawList. Items with stale mesh handles are skipped.
        [[nodiscard]] bool RenderFrame(const platform::Input &input, float deltaSeconds, const core::DrawList &drawList);
        // Handles of the built-in sample model loaded at initialization.
        [[nodiscard]] core::MeshHandle GetSampleMesh() const noexcept;
        [[nodiscard]] core::TextureHandle GetSampleTexture() const noexcept;
        void RequestResize() noexcept;
        void Shutdown() noexcept;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation_;
    };
}
