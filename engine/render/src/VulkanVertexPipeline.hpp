#pragma once

#include "RenderResources.hpp"
#include "VulkanSwapchain.hpp"

#include <filesystem>
#include <map>
#include <string>

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
            VkDescriptorSetLayout textureSetLayout,
            VkRenderPass sceneRenderPass);
        // The best depth format this GPU has (VK_FORMAT_UNDEFINED if none).
        [[nodiscard]] static VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice);
        void Shutdown() noexcept;

        // The swapchain pass: the post composite and the overlay. The scene itself draws into the
        // HDR pass given to Initialize (VulkanPost).
        [[nodiscard]] VkRenderPass GetRenderPass() const noexcept { return renderPass_; }
        [[nodiscard]] VkPipeline GetHandle() const noexcept { return pipeline_; }
        // A material's custom surface shader (CustomShaders.md); the plain pipeline if it wasn't built.
        // `skinned`: the variant with the skinning vertex shader (S8); plain or skinned default if missing.
        [[nodiscard]] VkPipeline GetSurfacePipeline(const std::string &name, bool skinned) const;
        [[nodiscard]] VkPipeline GetSkinnedShadowPipeline() const noexcept { return skinnedShadowPipeline_; }
        [[nodiscard]] VkPipelineLayout GetLayout() const noexcept { return pipelineLayout_; }
        [[nodiscard]] VkDescriptorSetLayout GetDescriptorSetLayout() const noexcept { return descriptorSetLayout_; }
        [[nodiscard]] VkFormat GetDepthFormat() const noexcept { return depthFormat_; }
        [[nodiscard]] const std::vector<VkFramebuffer> &GetFramebuffers() const noexcept { return framebuffers_; }
        // The sky: one screen-covering triangle drawn before the scene (Lighting.md).
        [[nodiscard]] VkPipeline GetSkyPipeline() const noexcept { return skyPipeline_; }
        [[nodiscard]] VkPipeline GetParticlePipeline() const noexcept { return particlePipeline_; }
        // The sun's shadow map: a depth-only pass seen from the sun, sampled by the scene shader.
        static constexpr std::uint32_t shadowMapSize = 2048;
        [[nodiscard]] VkRenderPass GetShadowRenderPass() const noexcept { return shadowRenderPass_; }
        [[nodiscard]] VkFramebuffer GetShadowFramebuffer() const noexcept { return shadowFramebuffer_; }
        [[nodiscard]] VkPipeline GetShadowPipeline() const noexcept { return shadowPipeline_; }
        [[nodiscard]] VkImageView GetShadowView() const noexcept { return shadowImage_.GetView(); }
        [[nodiscard]] VkSampler GetShadowSampler() const noexcept { return shadowSampler_; }

    private:
        bool CreateShadowMap(VkPhysicalDevice physicalDevice, VulkanAllocator &allocator, VkShaderModule shadowShader, VkShaderModule skinnedShadowShader,
                             const VkGraphicsPipelineCreateInfo &scene, const VkPipelineVertexInputStateCreateInfo &vertexInput,
                             const VkPipelineVertexInputStateCreateInfo &skinnedInput, const VkPipelineInputAssemblyStateCreateInfo &assembly,
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
        VkPipeline particlePipeline_ = VK_NULL_HANDLE; // S9: camera-facing squares, one instance each
        std::map<std::string, VkPipeline> surfaces_; // "water" -> scene pipeline with shaders/surface/water.frag.spv
        std::map<std::string, VkPipeline> skinnedSurfaces_;
        VkPipeline skinnedPipeline_ = VK_NULL_HANDLE;       // GetSurfacePipeline("", true)
        VkPipeline skinnedShadowPipeline_ = VK_NULL_HANDLE;
        VkRenderPass shadowRenderPass_ = VK_NULL_HANDLE;
        VkPipeline shadowPipeline_ = VK_NULL_HANDLE;
        GpuImage shadowImage_;
        VkFramebuffer shadowFramebuffer_ = VK_NULL_HANDLE;
        VkSampler shadowSampler_ = VK_NULL_HANDLE;
    };
}
