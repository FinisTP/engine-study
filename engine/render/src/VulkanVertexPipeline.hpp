#pragma once

#include "RenderResources.hpp"
#include "VulkanSwapchain.hpp"

#include <filesystem>

#include <glm/mat4x4.hpp>
#include <cstdint>
#include <glm/vec4.hpp>
#include <vector>

namespace umbra::render
{
    // Per-draw data sent as push constants (must match DrawConstants in Scene.hlsl).
    struct DrawConstants final
    {
        glm::mat4 model{1.0F};
        glm::vec4 color{1.0F};
        glm::vec4 texturing{0.0F}; // x = meters per repeat for world-space texture coordinates (0 = mesh UVs), y = emissive
    };

    class VulkanVertexPipeline final
    {
    public:
        VulkanVertexPipeline() = default;
        ~VulkanVertexPipeline();
        VulkanVertexPipeline(const VulkanVertexPipeline &) = delete;
        VulkanVertexPipeline &operator=(const VulkanVertexPipeline &) = delete;

        // Loads Scene, Sky and Shadow shaders (*.spv) from `shaderDirectory`.
        [[nodiscard]] bool Initialize(VkPhysicalDevice physicalDevice, VkDevice device,
            VulkanAllocator &allocator, const VulkanSwapchain &swapchain,
            const std::filesystem::path &shaderDirectory,
            VkDescriptorSetLayout textureSetLayout);
        void Shutdown() noexcept;

        [[nodiscard]] VkRenderPass GetRenderPass() const noexcept { return renderPass_; }
        [[nodiscard]] VkPipeline GetHandle() const noexcept { return pipeline_; }
        [[nodiscard]] VkPipelineLayout GetLayout() const noexcept { return pipelineLayout_; }
        [[nodiscard]] VkDescriptorSetLayout GetDescriptorSetLayout() const noexcept { return descriptorSetLayout_; }
        [[nodiscard]] VkFormat GetDepthFormat() const noexcept { return depthFormat_; }
        [[nodiscard]] const std::vector<VkFramebuffer> &GetFramebuffers() const noexcept { return framebuffers_; }
        // The sky: one screen-covering triangle drawn before the scene (Lighting.md).
        [[nodiscard]] VkPipeline GetSkyPipeline() const noexcept { return skyPipeline_; }
        // The sun's shadow map: a depth-only pass seen from the sun, sampled by the scene shader.
        static constexpr std::uint32_t shadowMapSize = 2048;
        [[nodiscard]] VkRenderPass GetShadowRenderPass() const noexcept { return shadowRenderPass_; }
        [[nodiscard]] VkFramebuffer GetShadowFramebuffer() const noexcept { return shadowFramebuffer_; }
        [[nodiscard]] VkPipeline GetShadowPipeline() const noexcept { return shadowPipeline_; }
        [[nodiscard]] VkImageView GetShadowView() const noexcept { return shadowImage_.GetView(); }
        [[nodiscard]] VkSampler GetShadowSampler() const noexcept { return shadowSampler_; }

    private:
        bool CreateShadowMap(VkPhysicalDevice physicalDevice, VulkanAllocator &allocator, VkShaderModule shadowShader, const VkGraphicsPipelineCreateInfo &scene,
                             const VkPipelineVertexInputStateCreateInfo &vertexInput, const VkPipelineInputAssemblyStateCreateInfo &assembly,
                             const VkPipelineMultisampleStateCreateInfo &multisample, const VkPipelineDepthStencilStateCreateInfo &depthState);

        VkDevice device_ = VK_NULL_HANDLE;
        VkRenderPass renderPass_ = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
        std::vector<GpuImage> depthImages_;
        std::vector<VkFramebuffer> framebuffers_;
        VkPipeline skyPipeline_ = VK_NULL_HANDLE;
        VkRenderPass shadowRenderPass_ = VK_NULL_HANDLE;
        VkPipeline shadowPipeline_ = VK_NULL_HANDLE;
        GpuImage shadowImage_;
        VkFramebuffer shadowFramebuffer_ = VK_NULL_HANDLE;
        VkSampler shadowSampler_ = VK_NULL_HANDLE;
    };
}
