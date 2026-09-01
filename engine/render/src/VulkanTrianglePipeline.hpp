#pragma once

#include "RenderResources.hpp"
#include "VulkanSwapchain.hpp"

#include <filesystem>
#include <vector>

namespace umbra::render
{
    class VulkanTrianglePipeline final
    {
    public:
        VulkanTrianglePipeline() = default;
        ~VulkanTrianglePipeline();
        VulkanTrianglePipeline(const VulkanTrianglePipeline &) = delete;
        VulkanTrianglePipeline &operator=(const VulkanTrianglePipeline &) = delete;

        [[nodiscard]] bool Initialize(VkPhysicalDevice physicalDevice, VkDevice device,
            VulkanAllocator &allocator, const VulkanSwapchain &swapchain,
            const std::filesystem::path &vertexShaderPath,
            const std::filesystem::path &fragmentShaderPath);
        void Shutdown() noexcept;

        [[nodiscard]] VkRenderPass GetRenderPass() const noexcept { return renderPass_; }
        [[nodiscard]] VkPipeline GetHandle() const noexcept { return pipeline_; }
        [[nodiscard]] VkPipelineLayout GetLayout() const noexcept { return pipelineLayout_; }
        [[nodiscard]] VkDescriptorSetLayout GetDescriptorSetLayout() const noexcept { return descriptorSetLayout_; }
        [[nodiscard]] VkFormat GetDepthFormat() const noexcept { return depthFormat_; }
        [[nodiscard]] const std::vector<VkFramebuffer> &GetFramebuffers() const noexcept { return framebuffers_; }

    private:
        VkDevice device_ = VK_NULL_HANDLE;
        VkRenderPass renderPass_ = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
        std::vector<GpuImage> depthImages_;
        std::vector<VkFramebuffer> framebuffers_;
    };
}
