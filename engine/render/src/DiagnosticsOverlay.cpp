#include "DiagnosticsOverlay.hpp"

#include <Log.hpp>
#include <imgui.h>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace
{
    constexpr VkDeviceSize overlayBufferSize = 1024 * 1024;

    bool CreateShader(VkDevice device, const std::filesystem::path &path, VkShaderModule &shader)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file)
        {
            return false;
        }
        const auto size = file.tellg();
        if (size <= 0 || size % 4 != 0)
        {
            return false;
        }
        std::vector<std::uint32_t> code(static_cast<std::size_t>(size) / 4);
        file.seekg(0);
        file.read(reinterpret_cast<char *>(code.data()), size);
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = code.size() * 4;
        info.pCode = code.data();
        return vkCreateShaderModule(device, &info, nullptr, &shader) == VK_SUCCESS;
    }
}

namespace umbra::render
{
    DiagnosticsOverlay::~DiagnosticsOverlay() { Shutdown(); }

    bool DiagnosticsOverlay::Initialize(
        VkInstance, VkPhysicalDevice, VkDevice device, std::uint32_t, VkQueue,
        VkRenderPass renderPass, std::uint32_t, const char *gpuName,
        VulkanAllocator &allocator, const std::filesystem::path &vertexShaderPath,
        const std::filesystem::path &fragmentShaderPath)
    {
        if (initialized_)
        {
            return false;
        }
        device_ = device;
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGuiIO &io = ImGui::GetIO();
        io.BackendRendererName = "Umbra Vulkan overlay";
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

        unsigned char *pixels = nullptr;
        int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        if (!allocator.CreateTexture(std::as_bytes(std::span{pixels, static_cast<std::size_t>(width * height * 4)}),
                                     static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), fontImage_) ||
            !allocator.CreateMappedBuffer(overlayBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffers_[0]) ||
            !allocator.CreateMappedBuffer(overlayBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffers_[0]) ||
            !allocator.CreateMappedBuffer(overlayBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffers_[1]) ||
            !allocator.CreateMappedBuffer(overlayBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffers_[1]))
        {
            Shutdown();
            return false;
        }

        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        if (vkCreateSampler(device_, &samplerInfo, nullptr, &fontSampler_) != VK_SUCCESS)
        {
            Shutdown();
            return false;
        }

        const VkDescriptorSetLayoutBinding binding{
            0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        setInfo.bindingCount = 1;
        setInfo.pBindings = &binding;
        if (vkCreateDescriptorSetLayout(device_, &setInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS)
        {
            Shutdown();
            return false;
        }
        const VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS)
        {
            Shutdown();
            return false;
        }
        VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocateInfo.descriptorPool = descriptorPool_;
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.pSetLayouts = &descriptorSetLayout_;
        if (vkAllocateDescriptorSets(device_, &allocateInfo, &descriptorSet_) != VK_SUCCESS)
        {
            Shutdown();
            return false;
        }
        const VkDescriptorImageInfo imageInfo{fontSampler_, fontImage_.GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = descriptorSet_;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
        io.Fonts->SetTexID(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(descriptorSet_)));

        const VkPushConstantRange pushRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 4};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &descriptorSetLayout_;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;
        if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS)
        {
            Shutdown();
            return false;
        }

        VkShaderModule vertexShader = VK_NULL_HANDLE, fragmentShader = VK_NULL_HANDLE;
        if (!CreateShader(device_, vertexShaderPath, vertexShader) ||
            !CreateShader(device_, fragmentShaderPath, fragmentShader))
        {
            if (vertexShader)
                vkDestroyShaderModule(device_, vertexShader, nullptr);
            if (fragmentShader)
                vkDestroyShaderModule(device_, fragmentShader, nullptr);
            Shutdown();
            return false;
        }
        const std::array stages{
            VkPipelineShaderStageCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                                            VK_SHADER_STAGE_VERTEX_BIT, vertexShader, "main", nullptr},
            VkPipelineShaderStageCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                                            VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader, "main", nullptr}};
        const VkVertexInputBindingDescription vertexBinding{0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX};
        const std::array attributes{
            VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)},
            VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)},
            VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)}};
        VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &vertexBinding;
        vertexInput.vertexAttributeDescriptionCount = 3;
        vertexInput.pVertexAttributeDescriptions = attributes.data();
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.lineWidth = 1.0F;
        VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depthState{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depthState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        depthState.maxDepthBounds = 1.0F;
        VkPipelineColorBlendAttachmentState attachment{};
        attachment.blendEnable = VK_TRUE;
        attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.colorBlendOp = VK_BLEND_OP_ADD;
        attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = 1;
        blend.pAttachments = &attachment;
        const std::array dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = dynamicStates.data();
        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages.data();
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &assembly;
        pipelineInfo.pViewportState = &viewport;
        pipelineInfo.pRasterizationState = &raster;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthState;
        pipelineInfo.pColorBlendState = &blend;
        pipelineInfo.pDynamicState = &dynamic;
        pipelineInfo.layout = pipelineLayout_;
        pipelineInfo.renderPass = renderPass;
        const VkResult result = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_);
        vkDestroyShaderModule(device_, fragmentShader, nullptr);
        vkDestroyShaderModule(device_, vertexShader, nullptr);
        if (result != VK_SUCCESS)
        {
            Shutdown();
            return false;
        }

        gpuName_ = gpuName != nullptr ? gpuName : "Unknown GPU";
        initialized_ = true;
        diagnostics::Info("ImGui diagnostics overlay initialized with Umbra's Volk renderer.");
        return true;
    }

    void DiagnosticsOverlay::BeginFrame(
        VkExtent2D extent, float deltaSeconds, std::size_t meshCount,
        std::size_t textureCount, std::size_t drawCount, std::uint32_t indexCount)
    {
        if (!initialized_)
            return;
        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(extent.width), static_cast<float>(extent.height));
        io.DeltaTime = std::max(deltaSeconds, 0.0001F);
        ImGui::NewFrame();
        ImGui::SetNextWindowBgAlpha(0.82F);
        ImGui::Begin("Umbra diagnostics", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::Text("GPU: %s", gpuName_.c_str());
        ImGui::Text("Swapchain: %u x %u", extent.width, extent.height);
        ImGui::Text("Frame: %.2f ms (%.0f FPS)", io.DeltaTime * 1000.0F, 1.0F / io.DeltaTime);
        ImGui::Separator();
        ImGui::Text("Meshes: %zu", meshCount);
        ImGui::Text("Textures: %zu", textureCount);
        ImGui::Text("Draws: %zu", drawCount);
        ImGui::Text("Indices: %u", indexCount);
        ImGui::TextDisabled("WASD move | mouse look | Space jump | F1 colliders");
        ImGui::End();
    }

    void DiagnosticsOverlay::DrawLines(const std::vector<core::DebugLine> &lines, const glm::mat4 &viewProjection, VkExtent2D extent)
    {
        if (!initialized_ || lines.empty())
            return;
        ImDrawList *draw = ImGui::GetBackgroundDrawList();
        const glm::vec2 size{static_cast<float>(extent.width), static_cast<float>(extent.height)};
        constexpr float nearW = 0.001F;
        for (const core::DebugLine &line : lines)
        {
            // 1. To clip space; drop the part of the line behind the camera (w <= nearW).
            glm::vec4 a = viewProjection * glm::vec4(line.from, 1.0F);
            glm::vec4 b = viewProjection * glm::vec4(line.to, 1.0F);
            if (a.w <= nearW && b.w <= nearW)
                continue;
            if (a.w <= nearW)
                a = a + (b - a) * ((nearW - a.w) / (b.w - a.w));
            else if (b.w <= nearW)
                b = b + (a - b) * ((nearW - b.w) / (a.w - b.w));

            // 2. Perspective divide, then NDC (-1..1) to pixels.
            const glm::vec2 pa = (glm::vec2(a) / a.w * 0.5F + 0.5F) * size;
            const glm::vec2 pb = (glm::vec2(b) / b.w * 0.5F + 0.5F) * size;
            const ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(line.color.r, line.color.g, line.color.b, line.color.a));
            draw->AddLine(ImVec2(pa.x, pa.y), ImVec2(pb.x, pb.y), color, 1.5F);
        }
    }

    void DiagnosticsOverlay::EndFrame()
    {
        if (initialized_)
            ImGui::Render();
    }

    void DiagnosticsOverlay::Render(VkCommandBuffer commandBuffer, std::size_t frameIndex)
    {
        ImDrawData *drawData = ImGui::GetDrawData();
        if (!initialized_ || drawData == nullptr || drawData->TotalVtxCount == 0)
            return;
        const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(drawData->TotalVtxCount) * sizeof(ImDrawVert);
        const VkDeviceSize indexBytes = static_cast<VkDeviceSize>(drawData->TotalIdxCount) * sizeof(ImDrawIdx);
        GpuBuffer &vertexStorage = vertexBuffers_[frameIndex % vertexBuffers_.size()];
        GpuBuffer &indexStorage = indexBuffers_[frameIndex % indexBuffers_.size()];
        if (vertexBytes > vertexStorage.GetSize() || indexBytes > indexStorage.GetSize())
            return;
        auto *vertexDestination = static_cast<std::byte *>(vertexStorage.GetMappedData());
        auto *indexDestination = static_cast<std::byte *>(indexStorage.GetMappedData());
        for (const ImDrawList *list : drawData->CmdLists)
        {
            const std::size_t vertexSize = static_cast<std::size_t>(list->VtxBuffer.Size) * sizeof(ImDrawVert);
            const std::size_t indexSize = static_cast<std::size_t>(list->IdxBuffer.Size) * sizeof(ImDrawIdx);
            std::memcpy(vertexDestination, list->VtxBuffer.Data, vertexSize);
            std::memcpy(indexDestination, list->IdxBuffer.Data, indexSize);
            vertexDestination += vertexSize;
            indexDestination += indexSize;
        }
        vertexStorage.Flush();
        indexStorage.Flush();
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        const VkBuffer vertexBuffer = vertexStorage.GetHandle();
        constexpr VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offset);
        vkCmdBindIndexBuffer(commandBuffer, indexStorage.GetHandle(), 0,
                             sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_,
                                0, 1, &descriptorSet_, 0, nullptr);
        const VkViewport viewport{0, 0, drawData->DisplaySize.x * drawData->FramebufferScale.x,
                                  drawData->DisplaySize.y * drawData->FramebufferScale.y, 0.0F, 1.0F};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        const std::array pushConstants{
            2.0F / drawData->DisplaySize.x, 2.0F / drawData->DisplaySize.y,
            -1.0F - drawData->DisplayPos.x * (2.0F / drawData->DisplaySize.x),
            -1.0F - drawData->DisplayPos.y * (2.0F / drawData->DisplaySize.y)};
        vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(pushConstants), pushConstants.data());
        std::uint32_t globalIndex = 0, globalVertex = 0;
        for (const ImDrawList *list : drawData->CmdLists)
        {
            for (const ImDrawCmd &command : list->CmdBuffer)
            {
                if (command.UserCallback != nullptr)
                {
                    command.UserCallback(list, &command);
                    continue;
                }
                const ImVec2 clipMin{(command.ClipRect.x - drawData->DisplayPos.x) * drawData->FramebufferScale.x,
                                     (command.ClipRect.y - drawData->DisplayPos.y) * drawData->FramebufferScale.y};
                const ImVec2 clipMax{(command.ClipRect.z - drawData->DisplayPos.x) * drawData->FramebufferScale.x,
                                     (command.ClipRect.w - drawData->DisplayPos.y) * drawData->FramebufferScale.y};
                if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                    continue;
                VkRect2D scissor{};
                scissor.offset = {std::max(0, static_cast<int>(clipMin.x)), std::max(0, static_cast<int>(clipMin.y))};
                scissor.extent = {static_cast<std::uint32_t>(clipMax.x - scissor.offset.x),
                                  static_cast<std::uint32_t>(clipMax.y - scissor.offset.y)};
                vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
                vkCmdDrawIndexed(commandBuffer, command.ElemCount, 1,
                                 globalIndex + command.IdxOffset, static_cast<std::int32_t>(globalVertex + command.VtxOffset), 0);
            }
            globalIndex += static_cast<std::uint32_t>(list->IdxBuffer.Size);
            globalVertex += static_cast<std::uint32_t>(list->VtxBuffer.Size);
        }
    }

    void DiagnosticsOverlay::Shutdown() noexcept
    {
        if (device_ != VK_NULL_HANDLE)
        {
            if (pipeline_)
                vkDestroyPipeline(device_, pipeline_, nullptr);
            if (pipelineLayout_)
                vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
            if (descriptorPool_)
                vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
            if (descriptorSetLayout_)
                vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
            if (fontSampler_)
                vkDestroySampler(device_, fontSampler_, nullptr);
        }
        pipeline_ = VK_NULL_HANDLE;
        pipelineLayout_ = VK_NULL_HANDLE;
        descriptorPool_ = VK_NULL_HANDLE;
        descriptorSetLayout_ = VK_NULL_HANDLE;
        descriptorSet_ = VK_NULL_HANDLE;
        fontSampler_ = VK_NULL_HANDLE;
        for (GpuBuffer &buffer : vertexBuffers_)
            buffer.Reset();
        for (GpuBuffer &buffer : indexBuffers_)
            buffer.Reset();
        fontImage_.Reset();
        device_ = VK_NULL_HANDLE;
        if (ImGui::GetCurrentContext() != nullptr)
            ImGui::DestroyContext();
        initialized_ = false;
        gpuName_.clear();
    }
}
