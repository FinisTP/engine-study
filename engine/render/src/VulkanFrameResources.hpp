#pragma once

#include "VulkanSwapchain.hpp"
#include "VulkanVertexPipeline.hpp"
#include "RenderResources.hpp"

#include <volk.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/mat4x4.hpp>

namespace umbra::render
{
    enum class FrameResult
    {
        Presented,
        SwapchainOutOfDate,
        FatalError
    };

    // vertex shader -> model -> view -> projection -> fragment shader
    struct SceneUniform final
    {
        // local coordinate -> world coordinate
        glm::mat4 model{1.0F};

        // world coordinate -> camera coordinate
        glm::mat4 view{1.0F};

        // camera coordinate -> clip coordinate
        glm::mat4 projection{1.0F};

        // tint
        glm::vec4 baseColor{1.0F};
    };

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
            VkDescriptorSetLayout descriptorSetLayout,
            const TextureResource &texture);

        [[nodiscard]] FrameResult DrawFrame(
            VkQueue graphicsQueue,
            VkQueue presentQueue,
            const VulkanSwapchain &swapchain,
            const VulkanVertexPipeline &pipeline,
            const MeshResource &mesh,
            const SceneUniform &uniform,
            DiagnosticsOverlay &overlay);

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
        VkCommandPool commandPool_ = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
        std::array<Frame, framesInFlight> frames_{};
        std::vector<VkSemaphore> renderFinished_;
        std::vector<VkFence> imagesInFlight_;
        std::size_t currentFrame_ = 0;
    };
}
