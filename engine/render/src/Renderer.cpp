#include <Renderer.hpp>

#include <SDL3/SDL.h>

#include "DiagnosticsOverlay.hpp"
#include "RenderResources.hpp"
#include "VulkanDevice.hpp"
#include "VulkanFrameResources.hpp"
#include "VulkanInstance.hpp"
#include "VulkanPhysicalDevice.hpp"
#include "VulkanSurface.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanVertexPipeline.hpp"

#include <Log.hpp>
#include <Platform.hpp>
#include <Window.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_video.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace umbra::render
{
    struct Renderer::Implementation final
    {
        enum class PresentationInitializationResult
        {
            Initialized,
            SurfaceUnavailable,
            FatalError
        };

        enum class DrawableExtentResult
        {
            Available,
            Unavailable,
            FatalError
        };

        VulkanInstance instance;
        VulkanSurface surface;
        VulkanPhysicalDevice physicalDevice;
        VulkanDevice device;
        VulkanAllocator allocator;
        SceneResources sceneResources;
        VulkanSwapchain swapchain;
        VulkanPost post;
        VulkanVertexPipeline pipeline;
        VulkanFrameResources frameResources;
        DiagnosticsOverlay overlay;
        platform::Window *window = nullptr;
        bool resizeRequested = false;
        bool surfaceReleased = false; // in the background (ReleaseSurface)

        ~Implementation() { (void)device.WaitIdle(); }

        [[nodiscard]] DrawableExtentResult QueryDrawableExtent() const
        {
            int width = 0, height = 0;
            if (!SDL_GetWindowSizeInPixels(window->GetNativeHandle(), &width, &height))
            {
                diagnostics::Error(std::string{"Failed querying drawable extent: "} + SDL_GetError());
                return DrawableExtentResult::FatalError;
            }
            return width > 0 && height > 0
                       ? DrawableExtentResult::Available
                       : DrawableExtentResult::Unavailable;
        }

        [[nodiscard]] PresentationInitializationResult InitializePresentationResources()
        {
            // first we need swapchain
            const SwapchainInitializationResult swapchainResult = swapchain.Initialize(
                physicalDevice.GetHandle(),
                device.GetHandle(),
                surface.GetHandle(),
                physicalDevice.GetQueueFamilyIndices(),
                window->GetNativeHandle());
            if (swapchainResult == SwapchainInitializationResult::SurfaceUnavailable)
            {
                resizeRequested = true;
                return PresentationInitializationResult::SurfaceUnavailable;
            }
            if (swapchainResult == SwapchainInitializationResult::FatalError)
            {
                return PresentationInitializationResult::FatalError;
            }

            // A packaged game has its shaders next to the program; a build uses the build folder.
            std::filesystem::path shaderDirectory{UMBRA_SHADER_DIRECTORY};
            if (const std::filesystem::path packaged = platform::Platform::ProgramFolder() / "shaders"; std::filesystem::exists(packaged / "Scene.vert.spv"))
                shaderDirectory = packaged;

            // HDR picture and bloom first (the scene pipelines draw into its pass), then the scene
            // pipelines, then the composite inside the swapchain pass. Phones get the reduced post.
#if defined(__ANDROID__)
            constexpr core::PostQuality postQuality = core::PostQuality::Reduced;
#else
            constexpr core::PostQuality postQuality = core::PostQuality::Full;
#endif
            const VkFormat depthFormat = VulkanVertexPipeline::FindDepthFormat(physicalDevice.GetHandle());
            if (!post.Initialize(device.GetHandle(), allocator, swapchain, depthFormat, shaderDirectory, postQuality) ||
                !pipeline.Initialize(physicalDevice.GetHandle(), device.GetHandle(), allocator, swapchain, shaderDirectory,
                                     sceneResources.GetTextureSetLayout(), post.GetSceneRenderPass()) ||
                !post.CreateComposite(pipeline.GetRenderPass()))
            {
                return PresentationInitializationResult::FatalError;
            }

            if (!frameResources.Initialize(
                    device.GetHandle(),
                    physicalDevice.GetQueueFamilyIndices().graphics,
                    swapchain.GetImages().size(),
                    allocator,
                    pipeline))
            {
                return PresentationInitializationResult::FatalError;
            }

            if (!overlay.Initialize(
                    instance.GetHandle(),
                    physicalDevice.GetHandle(),
                    device.GetHandle(),
                    physicalDevice.GetQueueFamilyIndices().graphics,
                    device.GetGraphicsQueue(),
                    pipeline.GetRenderPass(),
                    static_cast<std::uint32_t>(swapchain.GetImages().size()),
                    physicalDevice.GetProperties().deviceName,
                    allocator,
                    shaderDirectory / "Overlay.vert.spv",
                    shaderDirectory / "Overlay.frag.spv"))
            {
                return PresentationInitializationResult::FatalError;
            }
            resizeRequested = false;
            return PresentationInitializationResult::Initialized;
        }

        [[nodiscard]] PresentationInitializationResult RecreatePresentationResources()
        {
            const DrawableExtentResult drawableExtent = QueryDrawableExtent();
            if (drawableExtent == DrawableExtentResult::Unavailable)
            {
                // If the window is minimized, we cannot create a swapchain with zero width or height.
                return PresentationInitializationResult::SurfaceUnavailable;
            }
            if (drawableExtent == DrawableExtentResult::FatalError)
            {
                return PresentationInitializationResult::FatalError;
            }
            if (!device.WaitIdle())
            {
                // If the device is busy, we cannot destroy and recreate the swapchain.
                return PresentationInitializationResult::FatalError;
            }
            // The GPU is idle, so retired resources can all go now.
            sceneResources.FlushRetired();

            // Clear all resources that depend on the swapchain before recreating it.
            overlay.Shutdown(true); // keep the ImGui context (and the editor's input backend)
            frameResources.Shutdown();
            pipeline.Shutdown();
            post.Shutdown();
            swapchain.Shutdown();
            return InitializePresentationResources();
        }

        [[nodiscard]] SceneUniform BuildUniform(const core::DrawList &drawList) const
        {
            const core::Camera &camera = drawList.camera;
            SceneUniform uniform;
            uniform.view = glm::lookAtRH(camera.position, camera.target, glm::vec3{0.0F, 1.0F, 0.0F});
            const VkExtent2D extent = swapchain.GetExtent();
            uniform.projection = glm::perspectiveRH_ZO(glm::radians(camera.fovYDegrees),
                                                       static_cast<float>(extent.width) / static_cast<float>(extent.height),
                                                       camera.nearPlane, camera.farPlane);
            uniform.projection[1][1] *= -1.0F;
            uniform.inverseViewProjection = glm::inverse(uniform.projection * uniform.view);
            // Sun shadows cover the area around what the camera looks at (Lighting.md).
            uniform.shadowMatrix = core::SunShadowMatrix(drawList.environment.sunDirection, camera.target, shadowRadius, VulkanVertexPipeline::shadowMapSize);
            uniform.lighting = core::PackLighting(drawList.environment, drawList.lights, camera.position, seconds, VulkanVertexPipeline::shadowMapSize);
            return uniform;
        }

        static constexpr float shadowRadius = 25.0F; // meters around the camera's target
        float seconds = 0.0F;                        // since start: light flicker
        std::optional<CaptureCallback> capture;       // asked for by CaptureNextFrame
    };

    Renderer::Renderer() = default;
    Renderer::~Renderer() { Shutdown(); }

    bool Renderer::Initialize(platform::Window &window)
    {
        if (implementation_ != nullptr)
        {
            diagnostics::Error("Cannot initialize renderer: it is already initialized.");
            return false;
        }

        // singleton
        auto implementation = std::make_unique<Implementation>();

        if (!implementation->instance.Initialize())
        {
            return false;
        }

        if (!implementation->surface.Initialize(window.GetNativeHandle(), implementation->instance.GetHandle()))
        {
            return false;
        }

        if (!implementation->physicalDevice.Initialize(implementation->instance.GetHandle(), implementation->surface.GetHandle()))
        {
            return false;
        }

        if (!implementation->device.Initialize(implementation->physicalDevice))
        {
            return false;
        }

        if (!implementation->allocator.Initialize(implementation->instance.GetHandle(),
                                                  implementation->physicalDevice.GetHandle(),
                                                  implementation->device.GetHandle(),
                                                  implementation->device.GetGraphicsQueue(),
                                                  implementation->physicalDevice.GetQueueFamilyIndices().graphics))
        {
            return false;
        }

        if (!implementation->sceneResources.Initialize(implementation->device.GetHandle(),
                                                       implementation->allocator,
                                                       implementation->physicalDevice.GetProperties()))
        {
            return false;
        }


        implementation->window = &window;
        const Implementation::PresentationInitializationResult presentationResult =
            implementation->InitializePresentationResources();
        if (presentationResult == Implementation::PresentationInitializationResult::FatalError)
        {
            return false;
        }

        implementation_ = std::move(implementation);
        diagnostics::Info("Renderer initialized.");
        return true;
    }

    bool Renderer::RenderFrame(float deltaSeconds, const core::DrawList &drawList, const std::function<void()> &ui)
    {
        if (implementation_ == nullptr)
        {
            diagnostics::Error("Cannot render frame: renderer is not initialized.");
            return false;
        }

        if (implementation_->surfaceReleased)
            return true; // nothing to present to until RestoreSurface
        if (implementation_->resizeRequested)
        {
            const Implementation::PresentationInitializationResult recreationResult =
                implementation_->RecreatePresentationResources();
            if (recreationResult == Implementation::PresentationInitializationResult::FatalError)
            {
                return false;
            }
            if (recreationResult == Implementation::PresentationInitializationResult::SurfaceUnavailable)
            {
                return true;
            }
        }


        // Count what will actually be drawn (stale meshes are skipped).
        const SceneResources &resources = implementation_->sceneResources;
        std::size_t drawCount = 0;
        std::uint32_t indexCount = 0;
        for (const core::DrawItem &item : drawList.items)
        {
            if (const MeshResource *mesh = resources.GetMesh(item.mesh))
            {
                ++drawCount;
                indexCount += mesh->indexCount;
            }
        }

        implementation_->overlay.BeginFrame(implementation_->swapchain.GetExtent(),
                                            deltaSeconds,
                                            resources.GetMeshCount(),
                                            resources.GetTextureCount(),
                                            drawCount,
                                            indexCount,
                                            drawList.hud.diagnosticsWindow);
        implementation_->seconds += deltaSeconds;
        const SceneUniform uniform = implementation_->BuildUniform(drawList);
        implementation_->overlay.DrawLines(drawList.lines, uniform.projection * uniform.view, implementation_->swapchain.GetExtent());
        implementation_->overlay.DrawUi(drawList.ui, resources);
        implementation_->overlay.DrawHud(drawList.hud, implementation_->swapchain.GetExtent());
        if (ui)
            ui(); // the editor's windows, inside the same ImGui frame
        implementation_->overlay.EndFrame();

        implementation_->post.SetGrade(drawList.post, drawList.postLut); // only does work when the grade changed
        const FrameResult result = implementation_->frameResources.DrawFrame(
            implementation_->device.GetGraphicsQueue(),
            implementation_->device.GetPresentQueue(),
            implementation_->swapchain,
            implementation_->pipeline,
            resources,
            drawList,
            resources.GetFallbackTexture(),
            uniform,
            implementation_->overlay,
            implementation_->post,
            implementation_->capture ? &*implementation_->capture : nullptr);
        if (result == FrameResult::Presented)
            implementation_->capture.reset();

        if (result == FrameResult::SwapchainOutOfDate)
        {
            implementation_->resizeRequested = true;
            return true;
        }
        if (result == FrameResult::SurfaceLost)
        {
            // Android took the window before the app heard it went to the background: let go of
            // the surface now; RestoreSurface makes a new one when the app comes back.
            ReleaseSurface();
            return true;
        }
        if (result != FrameResult::Presented)
        {
            return false;
        }

        implementation_->sceneResources.OnFrameSubmitted();
        return true;
    }

    core::MeshHandle Renderer::CreateMesh(const core::MeshData &data)
    {
        return implementation_ ? implementation_->sceneResources.CreateMesh(data) : core::MeshHandle{};
    }

    core::TextureHandle Renderer::CreateTexture(const core::TextureData &data)
    {
        return implementation_ ? implementation_->sceneResources.CreateTexture(data) : core::TextureHandle{};
    }

    bool Renderer::ReplaceMesh(core::MeshHandle handle, const core::MeshData &data)
    {
        return implementation_ && implementation_->sceneResources.ReplaceMesh(handle, data);
    }

    bool Renderer::ReplaceTexture(core::TextureHandle handle, const core::TextureData &data)
    {
        return implementation_ && implementation_->sceneResources.ReplaceTexture(handle, data);
    }

    bool Renderer::DestroyMesh(core::MeshHandle handle)
    {
        return implementation_ && implementation_->sceneResources.DestroyMesh(handle);
    }

    bool Renderer::DestroyTexture(core::TextureHandle handle)
    {
        return implementation_ && implementation_->sceneResources.DestroyTexture(handle);
    }

    void Renderer::RequestResize() noexcept
    {
        if (implementation_ != nullptr)
        {
            implementation_->resizeRequested = true;
        }
    }

    void Renderer::ReleaseSurface() noexcept
    {
        // Everything that presents to the surface goes, then the surface; meshes and textures stay.
        Implementation *i = implementation_.get();
        if (i == nullptr || i->surfaceReleased)
            return;
        (void)i->device.WaitIdle();
        i->sceneResources.FlushRetired();
        i->overlay.Shutdown(true);
        i->frameResources.Shutdown();
        i->pipeline.Shutdown();
        i->post.Shutdown();
        i->swapchain.Shutdown();
        i->surface.Shutdown();
        i->surfaceReleased = true;
        diagnostics::Info("Renderer: surface released (background).");
    }

    void Renderer::RestoreSurface() noexcept
    {
        // A new surface for the window Android gave back; the next frame builds the swapchain on it.
        Implementation *i = implementation_.get();
        if (i == nullptr || !i->surfaceReleased)
            return;
        if (!i->surface.Initialize(i->window->GetNativeHandle(), i->instance.GetHandle()))
        {
            diagnostics::Error("Renderer: could not make a new surface after returning to the foreground.");
            return;
        }
        i->surfaceReleased = false;
        i->resizeRequested = true;
        diagnostics::Info("Renderer: surface restored (foreground).");
    }

    void Renderer::CaptureNextFrame(std::function<void(const core::TextureData &)> done)
    {
        if (implementation_)
            implementation_->capture = std::move(done);
    }

    std::string Renderer::DeviceName() const
    {
        return implementation_ ? std::string{implementation_->physicalDevice.GetProperties().deviceName} : std::string{};
    }

    std::pair<std::uint32_t, std::uint32_t> Renderer::Extent() const
    {
        if (!implementation_)
            return {0, 0};
        const VkExtent2D extent = implementation_->swapchain.GetExtent();
        return {extent.width, extent.height};
    }

    void Renderer::Shutdown() noexcept
    {
        if (implementation_ == nullptr)
        {
            return;
        }
        implementation_.reset();
        diagnostics::Info("Renderer shut down.");
    }
}
