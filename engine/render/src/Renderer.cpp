#include <Renderer.hpp>

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
#include <Window.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_video.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <filesystem>
#include <memory>
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
        VulkanVertexPipeline pipeline;
        VulkanFrameResources frameResources;
        DiagnosticsOverlay overlay;
        platform::Window *window = nullptr;
        bool resizeRequested = false;

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

            const std::filesystem::path shaderDirectory{UMBRA_SHADER_DIRECTORY};

            if (!pipeline.Initialize(physicalDevice.GetHandle(),
                                     device.GetHandle(),
                                     allocator,
                                     swapchain,
                                     shaderDirectory / "Scene.vert.spv",
                                     shaderDirectory / "Scene.frag.spv",
                                     sceneResources.GetTextureSetLayout()))
            {
                return PresentationInitializationResult::FatalError;
            }

            if (!frameResources.Initialize(
                    device.GetHandle(),
                    physicalDevice.GetQueueFamilyIndices().graphics,
                    swapchain.GetImages().size(),
                    allocator,
                    pipeline.GetDescriptorSetLayout()))
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
            overlay.Shutdown();
            frameResources.Shutdown();
            pipeline.Shutdown();
            swapchain.Shutdown();
            return InitializePresentationResources();
        }

        [[nodiscard]] SceneUniform BuildUniform(const core::Camera &camera) const
        {
            SceneUniform uniform;
            uniform.view = glm::lookAtRH(camera.position, camera.target, glm::vec3{0.0F, 1.0F, 0.0F});
            const VkExtent2D extent = swapchain.GetExtent();
            uniform.projection = glm::perspectiveRH_ZO(glm::radians(camera.fovYDegrees),
                                                       static_cast<float>(extent.width) / static_cast<float>(extent.height),
                                                       camera.nearPlane, camera.farPlane);
            uniform.projection[1][1] *= -1.0F;
            return uniform;
        }
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

    bool Renderer::RenderFrame(float deltaSeconds, const core::DrawList &drawList)
    {
        if (implementation_ == nullptr)
        {
            diagnostics::Error("Cannot render frame: renderer is not initialized.");
            return false;
        }

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
                                            indexCount);
        const SceneUniform uniform = implementation_->BuildUniform(drawList.camera);
        implementation_->overlay.DrawLines(drawList.lines, uniform.projection * uniform.view, implementation_->swapchain.GetExtent());
        implementation_->overlay.DrawHud(drawList.hud, implementation_->swapchain.GetExtent());
        implementation_->overlay.EndFrame();

        const FrameResult result = implementation_->frameResources.DrawFrame(
            implementation_->device.GetGraphicsQueue(),
            implementation_->device.GetPresentQueue(),
            implementation_->swapchain,
            implementation_->pipeline,
            resources,
            drawList,
            resources.GetFallbackTexture(),
            uniform,
            implementation_->overlay);

        if (result == FrameResult::SwapchainOutOfDate)
        {
            implementation_->resizeRequested = true;
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
