#pragma once

#include <DrawList.hpp>
#include <MeshData.hpp>

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

        // GPU resources. Create* return an invalid handle if the data is invalid.
        // Replace* keep the handle and swap the data; Destroy* make the handle stale.
        // Old GPU memory is freed only after in-flight frames finish with it.
        [[nodiscard]] core::MeshHandle CreateMesh(const core::MeshData &data);
        [[nodiscard]] core::TextureHandle CreateTexture(const core::TextureData &data);
        bool ReplaceMesh(core::MeshHandle handle, const core::MeshData &data);
        bool ReplaceTexture(core::TextureHandle handle, const core::TextureData &data);
        bool DestroyMesh(core::MeshHandle handle);
        bool DestroyTexture(core::TextureHandle handle);
        void RequestResize() noexcept;
        void Shutdown() noexcept;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation_;
    };
}
