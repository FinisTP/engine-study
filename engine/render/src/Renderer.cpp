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

#include <Input.hpp>
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
        glm::vec3 cameraPosition{0.0F, 0.5F, 5.0F};
        float cameraYaw = -90.0F;
        float cameraPitch = -6.0F;
        float sceneRotation = 0.0F;

        ~Implementation() { (void)device.WaitIdle(); }

        [[nodiscard]] bool HasDrawableExtent() const
        {
            int width = 0, height = 0;
            if (!SDL_GetWindowSizeInPixels(window->GetNativeHandle(), &width, &height))
            {
                diagnostics::Error(std::string{"Failed querying drawable extent: "} + SDL_GetError());
                return false;
            }
            return width > 0 && height > 0;
        }

        [[nodiscard]] bool InitializePresentationResources()
        {
            // first we need swapchain
            if (!swapchain.Initialize(
                    physicalDevice.GetHandle(),
                    device.GetHandle(),
                    surface.GetHandle(),
                    physicalDevice.GetQueueFamilyIndices(),
                    window->GetNativeHandle()))
            {
                return false;
            }

            const std::filesystem::path shaderDirectory{UMBRA_SHADER_DIRECTORY};

            if (!pipeline.Initialize(physicalDevice.GetHandle(),
                                     device.GetHandle(),
                                     allocator,
                                     swapchain,
                                     shaderDirectory / "Scene.vert.spv",
                                     shaderDirectory / "Scene.frag.spv"))
            {
                return false;
            }

            const TextureResource *texture = sceneResources.GetTexture(sceneResources.GetSceneTexture());
            if (texture == nullptr)
            {
                return false;
            }
            if (!frameResources.Initialize(
                    device.GetHandle(),
                    physicalDevice.GetQueueFamilyIndices().graphics,
                    swapchain.GetImages().size(),
                    allocator,
                    pipeline.GetDescriptorSetLayout(),
                    *texture))
            {
                return false;
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
                return false;
            }
            resizeRequested = false;
            return true;
        }

        [[nodiscard]] bool RecreatePresentationResources()
        {
            if (!HasDrawableExtent())
            {
                // If the window is minimized, we cannot create a swapchain with zero width or height.
                return true;
            }
            if (!device.WaitIdle())
            {
                // If the device is busy, we cannot destroy and recreate the swapchain.
                return false;
            }

            // Clear all resources that depend on the swapchain before recreating it.
            overlay.Shutdown();
            frameResources.Shutdown();
            pipeline.Shutdown();
            swapchain.Shutdown();
            return InitializePresentationResources();
        }

        [[nodiscard]] glm::vec3 Forward() const
        {
            return glm::normalize(glm::vec3{
                glm::cos(glm::radians(cameraYaw)) * glm::cos(glm::radians(cameraPitch)),
                glm::sin(glm::radians(cameraPitch)),
                glm::sin(glm::radians(cameraYaw)) * glm::cos(glm::radians(cameraPitch))});
        }

        void UpdateCamera(const platform::Input &input, float deltaSeconds)
        {
            const float delta = std::clamp(deltaSeconds, 0.0F, 0.1F);
            const float turn = 70.0F * delta;
            if (input.IsKeyHeld(platform::InputKey::Left))
                cameraYaw -= turn;
            if (input.IsKeyHeld(platform::InputKey::Right))
                cameraYaw += turn;
            if (input.IsKeyHeld(platform::InputKey::Up))
                cameraPitch += turn;
            if (input.IsKeyHeld(platform::InputKey::Down))
                cameraPitch -= turn;
            cameraPitch = std::clamp(cameraPitch, -85.0F, 85.0F);
            const glm::vec3 forward = Forward();
            const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3{0.0F, 1.0F, 0.0F}));
            const float movement = 3.0F * delta;
            if (input.IsKeyHeld(platform::InputKey::W))
                cameraPosition += forward * movement;
            if (input.IsKeyHeld(platform::InputKey::S))
                cameraPosition -= forward * movement;
            if (input.IsKeyHeld(platform::InputKey::A))
                cameraPosition -= right * movement;
            if (input.IsKeyHeld(platform::InputKey::D))
                cameraPosition += right * movement;
            sceneRotation += delta * glm::radians(18.0F);
        }

        [[nodiscard]] SceneUniform BuildUniform() const
        {
            SceneUniform uniform;
            uniform.model = glm::rotate(glm::mat4{1.0F}, sceneRotation, glm::vec3{0.0F, 1.0F, 0.0F});
            uniform.view = glm::lookAtRH(cameraPosition, cameraPosition + Forward(), glm::vec3{0.0F, 1.0F, 0.0F});
            const VkExtent2D extent = swapchain.GetExtent();
            uniform.projection = glm::perspectiveRH_ZO(glm::radians(60.0F),
                                                       static_cast<float>(extent.width) / static_cast<float>(extent.height), 0.1F, 100.0F);
            uniform.projection[1][1] *= -1.0F;
            uniform.baseColor = sceneResources.GetMesh(sceneResources.GetSceneMesh())->baseColor;
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

        // test loading a glTF scene
        if (!implementation->sceneResources.LoadGltf(
                std::filesystem::path{UMBRA_ASSET_DIRECTORY} / "scenes" / "phase2.gltf",
                implementation->allocator, implementation->device.GetHandle(),
                implementation->physicalDevice.GetProperties()))
        {
            return false;
        }

        implementation->window = &window;
        if (!implementation->InitializePresentationResources())
        {
            return false;
        }

        implementation_ = std::move(implementation);
        diagnostics::Info("Renderer initialized with the Phase 2 glTF scene.");
        return true;
    }

    bool Renderer::RenderFrame(const platform::Input &input, float deltaSeconds)
    {
        if (implementation_ == nullptr)
        {
            diagnostics::Error("Cannot render frame: renderer is not initialized.");
            return false;
        }

        if (implementation_->resizeRequested)
        {
            if (!implementation_->RecreatePresentationResources())
            {
                return false;
            }

            if (implementation_->resizeRequested)
            {
                return true;
            }
        }

        implementation_->UpdateCamera(input, deltaSeconds);
        const MeshResource *mesh = implementation_->sceneResources.GetMesh(implementation_->sceneResources.GetSceneMesh());
        if (mesh == nullptr)
        {
            diagnostics::Error("Scene mesh handle is stale.");
            return false;
        }

        implementation_->overlay.BeginFrame(implementation_->swapchain.GetExtent(),
                                            deltaSeconds,
                                            implementation_->sceneResources.GetMeshCount(),
                                            implementation_->sceneResources.GetTextureCount(),
                                            mesh->indexCount);

        const FrameResult result = implementation_->frameResources.DrawFrame(
            implementation_->device.GetGraphicsQueue(),
            implementation_->device.GetPresentQueue(),
            implementation_->swapchain,
            implementation_->pipeline,
            *mesh,
            implementation_->BuildUniform(),
            implementation_->overlay);

        if (result == FrameResult::SwapchainOutOfDate)
        {
            implementation_->resizeRequested = true;
            return true;
        }

        return result == FrameResult::Presented;
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
