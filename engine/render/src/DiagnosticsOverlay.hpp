#pragma once

#include "RenderResources.hpp"

#include <DrawList.hpp>

#include <glm/mat4x4.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <filesystem>
#include <array>
#include <vector>

namespace umbra::render
{
    class DiagnosticsOverlay final
    {
    public:
        DiagnosticsOverlay() = default;
        ~DiagnosticsOverlay();
        DiagnosticsOverlay(const DiagnosticsOverlay &) = delete;
        DiagnosticsOverlay &operator=(const DiagnosticsOverlay &) = delete;

        [[nodiscard]] bool Initialize(
            VkInstance instance,
            VkPhysicalDevice physicalDevice,
            VkDevice device,
            std::uint32_t queueFamily,
            VkQueue queue,
            VkRenderPass renderPass,
            std::uint32_t imageCount,
            const char *gpuName,
            VulkanAllocator &allocator,
            const std::filesystem::path &vertexShader,
            const std::filesystem::path &fragmentShader);

        // Frame order: BeginFrame (stats window) -> DrawLines / other ImGui calls -> EndFrame -> Render.
        void BeginFrame(VkExtent2D extent, float deltaSeconds, std::size_t meshCount, std::size_t textureCount, std::size_t drawCount, std::uint32_t indexCount);
        // Projects world-space lines with viewProjection and draws them over the scene. See DebugDraw.md.
        void DrawLines(const std::vector<core::DebugLine> &lines, const glm::mat4 &viewProjection, VkExtent2D extent);
        void EndFrame();

        void Render(VkCommandBuffer commandBuffer, std::size_t frameIndex);

        void Shutdown() noexcept;

    private:
        bool initialized_ = false;
        std::string gpuName_;
        VkDevice device_ = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkSampler fontSampler_ = VK_NULL_HANDLE;
        GpuImage fontImage_;
        std::array<GpuBuffer, 2> vertexBuffers_;
        std::array<GpuBuffer, 2> indexBuffers_;
    };
}
