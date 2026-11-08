#pragma once

#include "RenderResources.hpp"
#include "VulkanSwapchain.hpp"

#include <Post.hpp>

#include <filesystem>
#include <memory>
#include <vector>

namespace umbra::render
{
    // Post-processing (S7, Post.md): the scene is drawn into an HDR picture, bloom is built from
    // it in a chain of smaller pictures, and the composite pass writes the final picture into the
    // swapchain pass (before the overlay). One set of pictures per swapchain image.
    class VulkanPost final
    {
    public:
        VulkanPost() = default;
        ~VulkanPost();
        VulkanPost(const VulkanPost &) = delete;
        VulkanPost &operator=(const VulkanPost &) = delete;

        // HDR pictures, bloom chain, the bloom pipelines and a neutral color table.
        [[nodiscard]] bool Initialize(VkDevice device, VulkanAllocator &allocator, const VulkanSwapchain &swapchain, VkFormat depthFormat,
                                      const std::filesystem::path &shaderDirectory, core::PostQuality quality);
        // The composite pipeline, drawn inside the swapchain's render pass.
        [[nodiscard]] bool CreateComposite(VkRenderPass finalPass);
        void Shutdown() noexcept;

        // The scene draws into this pass (HDR color + depth).
        [[nodiscard]] VkRenderPass GetSceneRenderPass() const noexcept { return scenePass_; }
        [[nodiscard]] VkFramebuffer GetSceneFramebuffer(std::uint32_t imageIndex) const noexcept { return targets_[imageIndex].sceneFramebuffer; }
        [[nodiscard]] std::size_t GetImageCount() const noexcept { return targets_.size(); }

        // Rebuilds the color table when the grade changed (waits for the GPU: only on scene changes).
        void SetGrade(const core::PostSettings &settings, const std::shared_ptr<const core::TextureData> &lutImage);
        // Bloom: down the chain from the HDR picture, then back up. Outside any render pass.
        void RecordBloom(VkCommandBuffer commandBuffer, std::uint32_t imageIndex, const core::PostSettings &settings) const;
        // The final picture; call inside the swapchain pass.
        void DrawComposite(VkCommandBuffer commandBuffer, std::uint32_t imageIndex, const core::PostSettings &settings) const;

    private:
        struct Target final
        {
            GpuImage hdr, depth;
            VkFramebuffer sceneFramebuffer = VK_NULL_HANDLE;
            std::vector<GpuImage> levels;              // bloom: each half the size of the one before
            std::vector<VkFramebuffer> levelFramebuffers;
            std::vector<VkDescriptorSet> downSets;     // [i]: source for drawing level i
            std::vector<VkDescriptorSet> upSets;       // [i]: level i + 1, added onto level i
            VkDescriptorSet compositeSet = VK_NULL_HANDLE;
        };
        void Pass(VkCommandBuffer commandBuffer, VkRenderPass pass, VkFramebuffer framebuffer, VkExtent2D size, VkPipeline pipeline,
                  VkDescriptorSet set, VkExtent2D sourceSize, const core::PostSettings &settings) const;
        void WriteSets();
        [[nodiscard]] VkPipeline MakePipeline(VkShaderModule fragment, VkRenderPass pass, bool add) const;
        [[nodiscard]] VkExtent2D LevelSize(std::size_t level) const;

        VkDevice device_ = VK_NULL_HANDLE;
        VulkanAllocator *allocator_ = nullptr;
        VkExtent2D extent_{};
        core::PostQuality quality_ = core::PostQuality::Full;
        VkRenderPass scenePass_ = VK_NULL_HANDLE, bloomPass_ = VK_NULL_HANDLE, addPass_ = VK_NULL_HANDLE;
        VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE;
        VkPipelineLayout layout_ = VK_NULL_HANDLE;
        VkPipeline bright_ = VK_NULL_HANDLE, down_ = VK_NULL_HANDLE, up_ = VK_NULL_HANDLE, composite_ = VK_NULL_HANDLE;
        VkShaderModule fullscreen_ = VK_NULL_HANDLE, compositeShader_ = VK_NULL_HANDLE;
        VkDescriptorPool pool_ = VK_NULL_HANDLE;
        VkSampler sampler_ = VK_NULL_HANDLE;
        GpuImage lut_;
        core::PostSettings grade_;
        std::shared_ptr<const core::TextureData> gradeImage_;
        std::vector<Target> targets_;
    };
}
