#pragma once

#include "VulkanSwapchain.hpp"
#include "VulkanVertexPipeline.hpp"
#include "RenderResources.hpp"

#include <volk.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include <DrawList.hpp>
#include <MeshData.hpp>

#include <glm/mat4x4.hpp>

namespace umbra::render
{
    enum class FrameResult
    {
        Presented,
        SwapchainOutOfDate,
        SurfaceLost, // phones: the window went away between frames
        FatalError
    };

    // Per-frame data (set 0, binding 0; must match SceneUniform in Scene.hlsl). The model matrix
    // and tint are per draw (push constants).
    struct SceneUniform final
    {
        glm::mat4 view{1.0F};                  // world coordinate -> camera coordinate
        glm::mat4 projection{1.0F};            // camera coordinate -> clip coordinate
        glm::mat4 shadowMatrix{1.0F};          // world -> sun shadow map
        glm::mat4 inverseViewProjection{1.0F}; // screen -> world (sky rays)
        core::LightingBlock lighting;          // sun, sky, fog, point lights (Lighting.md)
    };
    static_assert(sizeof(SceneUniform) == 4 * 64 + 8 * 16 + 2 * 16 * 16, "SceneUniform must match the shader's layout");

    // Called with the frame's pixels (RGBA8, top row first) when a capture was asked for.
    using CaptureCallback = std::function<void(const core::TextureData &)>;

    class DiagnosticsOverlay;

    class VulkanFrameResources final
    {
    public:
        VulkanFrameResources() = default;
        ~VulkanFrameResources();

        VulkanFrameResources(const VulkanFrameResources &) = delete;
        VulkanFrameResources &operator=(const VulkanFrameResources &) = delete;
        VulkanFrameResources(VulkanFrameResources &&) = delete;
        VulkanFrameResources &operator=(VulkanFrameResources &&) = delete;

        [[nodiscard]] bool Initialize(
            VkDevice device,
            std::uint32_t graphicsQueueFamily,
            std::size_t swapchainImageCount,
            VulkanAllocator &allocator,
            const VulkanVertexPipeline &pipeline);

        [[nodiscard]] FrameResult DrawFrame(
            VkQueue graphicsQueue,
            VkQueue presentQueue,
            const VulkanSwapchain &swapchain,
            const VulkanVertexPipeline &pipeline,
            const SceneResources &resources,
            const core::DrawList &drawList,
            TextureHandle fallbackTexture,
            const SceneUniform &uniform,
            DiagnosticsOverlay &overlay,
            const CaptureCallback *capture = nullptr);

        void Shutdown() noexcept;

    private:
        static constexpr std::size_t framesInFlight = 2;

        struct Frame final
        {
            VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
            VkSemaphore imageAvailable = VK_NULL_HANDLE;
            VkFence inFlight = VK_NULL_HANDLE;
            GpuBuffer uniformBuffer;
            VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        };

        VkDevice device_ = VK_NULL_HANDLE;
        VulkanAllocator *allocator_ = nullptr;
        VkCommandPool commandPool_ = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
        std::array<Frame, framesInFlight> frames_{};
        std::vector<VkSemaphore> renderFinished_;
        std::vector<VkFence> imagesInFlight_;
        std::size_t currentFrame_ = 0;
    };
}
