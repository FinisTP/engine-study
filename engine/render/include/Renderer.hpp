#pragma once

#include <DrawList.hpp>
#include <MeshData.hpp>

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace umbra::platform
{
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
        // Draws every item in drawList from drawList.camera. Items with stale mesh handles are skipped.
        // `ui`, if given, is called inside the frame's Dear ImGui frame to draw extra windows (editor).
        [[nodiscard]] bool RenderFrame(float deltaSeconds, const core::DrawList &drawList, const std::function<void()> &ui = {});
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
        // Phones (P11): the window's surface is destroyed while the app is in the background.
        // ReleaseSurface frees everything presented to it; RestoreSurface makes a new one.
        void ReleaseSurface() noexcept;
        void RestoreSurface() noexcept;
        // Hands the next presented frame's pixels (RGBA8, top row first) to `done`, on this thread.
        // For tests, screenshots and the agent's look at the scene. No-op where the GPU can't copy.
        void CaptureNextFrame(std::function<void(const core::TextureData &)> done);
        // The GPU in use and the size of the picture, for reports (benchmark, P9).
        [[nodiscard]] std::string DeviceName() const;
        [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> Extent() const;
        void Shutdown() noexcept;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation_;
    };
}
