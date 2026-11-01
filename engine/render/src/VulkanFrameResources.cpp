#include "VulkanFrameResources.hpp"
#include "DiagnosticsOverlay.hpp"

#include <Log.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <cstring>
#include <string>

namespace
{
    // Summary: Records Vulkan commands to render every draw item and the diagnostics overlay.
    [[nodiscard]] bool RecordSceneCommands(
        VkCommandBuffer commandBuffer,
        std::uint32_t imageIndex,
        const umbra::render::VulkanSwapchain &swapchain,
        const umbra::render::VulkanVertexPipeline &pipeline,
        const umbra::render::SceneResources &resources,
        const umbra::core::DrawList &drawList,
        umbra::render::TextureHandle fallbackTexture,
        VkDescriptorSet descriptorSet,
        umbra::render::DiagnosticsOverlay &overlay,
        std::size_t frameIndex)
    {
        if (imageIndex >= pipeline.GetFramebuffers().size())
        {
            umbra::diagnostics::Error("Cannot record scene: swapchain image index is out of range.");
            return false;
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
        if (result != VK_SUCCESS)
        {
            umbra::diagnostics::Error(
                "Failed to begin Vulkan command buffer. VkResult=" +
                std::to_string(static_cast<int>(result)) + ".");
            return false;
        }

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.015F, 0.02F, 0.04F, 1.0F}};
        clearValues[1].depthStencil = {1.0F, 0};

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = pipeline.GetRenderPass();
        renderPassInfo.framebuffer = pipeline.GetFramebuffers()[imageIndex];
        renderPassInfo.renderArea.extent = swapchain.GetExtent();
        renderPassInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetHandle());
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetLayout(), 0, 1, &descriptorSet, 0, nullptr);
        for (const umbra::core::DrawItem &item : drawList.items)
        {
            // 1. Resolve handles; stale meshes are skipped, missing textures use the fallback.
            const umbra::render::MeshResource *mesh = resources.GetMesh(item.mesh);
            const umbra::render::TextureResource *texture = resources.GetTexture(item.texture);
            if (texture == nullptr)
                texture = resources.GetTexture(fallbackTexture);
            if (mesh == nullptr || texture == nullptr)
                continue;

            // 2. Bind this item's texture (set 1) and buffers, push its matrix and tint, draw.
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetLayout(), 1, 1, &texture->descriptorSet, 0, nullptr);
            const umbra::render::DrawConstants constants{item.model, item.color * mesh->baseColor};
            vkCmdPushConstants(commandBuffer, pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), &constants);
            const VkBuffer vertexBuffer = mesh->vertexBuffer.GetHandle();
            constexpr VkDeviceSize vertexOffset = 0;
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &vertexOffset);
            vkCmdBindIndexBuffer(commandBuffer, mesh->indexBuffer.GetHandle(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer, mesh->indexCount, 1, 0, 0, 0);
        }
        overlay.Render(commandBuffer, frameIndex);
        vkCmdEndRenderPass(commandBuffer);

        result = vkEndCommandBuffer(commandBuffer);
        if (result != VK_SUCCESS)
        {
            umbra::diagnostics::Error(
                "Failed to end Vulkan command buffer. VkResult=" +
                std::to_string(static_cast<int>(result)) + ".");
            return false;
        }
        return true;
    }
}

namespace umbra::render
{
    VulkanFrameResources::~VulkanFrameResources()
    {
        Shutdown();
    }

    bool VulkanFrameResources::Initialize(
        VkDevice device,
        std::uint32_t graphicsQueueFamily,
        std::size_t swapchainImageCount,
        VulkanAllocator &allocator,
        VkDescriptorSetLayout descriptorSetLayout)
    {
        if (device_ != VK_NULL_HANDLE)
        {
            diagnostics::Error("Cannot initialize Vulkan frame resources: they are already initialized.");
            return false;
        }
        if (device == VK_NULL_HANDLE || swapchainImageCount == 0)
        {
            diagnostics::Error("Cannot initialize Vulkan frame resources: device or swapchain is invalid.");
            return false;
        }

        device_ = device;

        // 1. Create a command pool for allocating command buffers for rendering.
        VkCommandPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolCreateInfo.queueFamilyIndex = graphicsQueueFamily;
        VkResult result = vkCreateCommandPool(device_, &poolCreateInfo, nullptr, &commandPool_);
        if (result != VK_SUCCESS)
        {
            diagnostics::Error(
                "Failed to create Vulkan command pool. VkResult=" +
                std::to_string(static_cast<int>(result)) + ".");
            Shutdown();
            return false;
        }

        // 2. Allocate two command buffers for rendering frames in flight.
        std::array<VkCommandBuffer, framesInFlight> commandBuffers{};
        VkCommandBufferAllocateInfo allocationInfo{};
        allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocationInfo.commandPool = commandPool_;
        allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocationInfo.commandBufferCount = static_cast<std::uint32_t>(commandBuffers.size());
        result = vkAllocateCommandBuffers(device_, &allocationInfo, commandBuffers.data());
        if (result != VK_SUCCESS)
        {
            diagnostics::Error(
                "Failed to allocate Vulkan command buffers. VkResult=" +
                std::to_string(static_cast<int>(result)) + ".");
            Shutdown();
            return false;
        }

        // 3. Create semaphores and fences for synchronizing rendering of frames in flight.
        // imageAvailable: signaled when the swapchain image is available for rendering
        // inFlight: signaled when the frame is finished rendering
        VkSemaphoreCreateInfo semaphoreCreateInfo{};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (std::size_t index = 0; index < frames_.size(); ++index)
        {
            frames_[index].commandBuffer = commandBuffers[index];
            result = vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &frames_[index].imageAvailable);
            if (result == VK_SUCCESS)
            {
                result = vkCreateFence(device_, &fenceCreateInfo, nullptr, &frames_[index].inFlight);
            }
            if (result != VK_SUCCESS)
            {
                diagnostics::Error(
                    "Failed to create Vulkan frame synchronization. VkResult=" +
                    std::to_string(static_cast<int>(result)) + ".");
                Shutdown();
                return false;
            }
        }

        // 4. Create a descriptor pool and allocate per-frame descriptor sets for uniform buffers.
        const std::array poolSizes{
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<std::uint32_t>(framesInFlight)}};
        VkDescriptorPoolCreateInfo descriptorPoolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        descriptorPoolInfo.maxSets = static_cast<std::uint32_t>(framesInFlight);
        descriptorPoolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
        descriptorPoolInfo.pPoolSizes = poolSizes.data();
        result = vkCreateDescriptorPool(device_, &descriptorPoolInfo, nullptr, &descriptorPool_);
        if (result != VK_SUCCESS)
        {
            diagnostics::Error("Failed to create scene descriptor pool.");
            Shutdown();
            return false;
        }
        // 5. Allocate per-frame descriptor sets (textures live in their own sets, see SceneResources).
        const std::array layouts{descriptorSetLayout, descriptorSetLayout};
        std::array<VkDescriptorSet, framesInFlight> descriptorSets{};
        VkDescriptorSetAllocateInfo descriptorAllocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        descriptorAllocateInfo.descriptorPool = descriptorPool_;
        descriptorAllocateInfo.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
        descriptorAllocateInfo.pSetLayouts = layouts.data();
        result = vkAllocateDescriptorSets(device_, &descriptorAllocateInfo, descriptorSets.data());
        if (result != VK_SUCCESS)
        {
            diagnostics::Error("Failed to allocate per-frame descriptor sets.");
            Shutdown();
            return false;
        }

        // 6. Create and update uniform buffers and descriptor sets for each frame in flight.
        // For each frame: create SceneUniform buffer and put it in descriptor binding 0.
        for (std::size_t index = 0; index < frames_.size(); ++index)
        {
            if (!allocator.CreateMappedBuffer(sizeof(SceneUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                              frames_[index].uniformBuffer))
            {
                Shutdown();
                return false;
            }
            frames_[index].descriptorSet = descriptorSets[index];
            const VkDescriptorBufferInfo bufferInfo{frames_[index].uniformBuffer.GetHandle(), 0, sizeof(SceneUniform)};
            const std::array writes{
                VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSets[index], 0, 0, 1,
                                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &bufferInfo, nullptr}};
            vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }

        // 7. Create semaphores for signaling when rendering of each frame is finished.
        renderFinished_.resize(swapchainImageCount, VK_NULL_HANDLE);
        for (VkSemaphore &semaphore : renderFinished_)
        {
            result = vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &semaphore);
            if (result != VK_SUCCESS)
            {
                diagnostics::Error(
                    "Failed to create Vulkan presentation semaphore. VkResult=" +
                    std::to_string(static_cast<int>(result)) + ".");
                Shutdown();
                return false;
            }
        }

        imagesInFlight_.assign(swapchainImageCount, VK_NULL_HANDLE);
        currentFrame_ = 0;
        diagnostics::Info("Vulkan frame submission resources initialized.");
        return true;
    }

    // one call = draw a single frame, including acquiring a swapchain image, recording command buffer, submitting it, and presenting the image.
    FrameResult VulkanFrameResources::DrawFrame(
        VkQueue graphicsQueue,
        VkQueue presentQueue,
        const VulkanSwapchain &swapchain,
        const VulkanVertexPipeline &pipeline,
        const SceneResources &resources,
        const core::DrawList &drawList,
        TextureHandle fallbackTexture,
        const SceneUniform &uniform,
        DiagnosticsOverlay &overlay)
    {
        if (device_ == VK_NULL_HANDLE || graphicsQueue == VK_NULL_HANDLE || presentQueue == VK_NULL_HANDLE)
        {
            diagnostics::Error("Cannot draw Vulkan frame: frame resources or queues are invalid.");
            return FrameResult::FatalError;
        }

        Frame &frame = frames_[currentFrame_];
        // first we must wait for the current frame to finish rendering before we can reuse its resources.
        VkResult result = vkWaitForFences(device_, 1, &frame.inFlight, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
        if (result != VK_SUCCESS)
        {
            diagnostics::Error(
                "Failed waiting for Vulkan frame fence. VkResult=" +
                std::to_string(static_cast<int>(result)) + ".");
            return FrameResult::FatalError;
        }

        // acquire the next available swapchain image for rendering
        std::uint32_t imageIndex = 0;
        result = vkAcquireNextImageKHR(
            device_,
            swapchain.GetHandle(),
            std::numeric_limits<std::uint64_t>::max(),
            frame.imageAvailable,
            VK_NULL_HANDLE,
            &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            return FrameResult::SwapchainOutOfDate;
        }
        const bool acquiredSuboptimal = result == VK_SUBOPTIMAL_KHR;
        if (result != VK_SUCCESS && !acquiredSuboptimal)
        {
            diagnostics::Error(
                "Failed acquiring Vulkan swapchain image. VkResult=" +
                std::to_string(static_cast<int>(result)) + ".");
            return FrameResult::FatalError;
        }

        if (imageIndex >= imagesInFlight_.size())
        {
            diagnostics::Error("Acquired Vulkan swapchain image index is out of range.");
            return FrameResult::FatalError;
        }

        // if the acquired swapchain image is already in use by another frame, wait for that frame to finish before reusing it.
        if (imagesInFlight_[imageIndex] != VK_NULL_HANDLE)
        {
            result = vkWaitForFences(
                device_,
                1,
                &imagesInFlight_[imageIndex],
                VK_TRUE,
                std::numeric_limits<std::uint64_t>::max());
            if (result != VK_SUCCESS)
            {
                diagnostics::Error("Failed waiting for Vulkan swapchain image fence.");
                return FrameResult::FatalError;
            }
        }
        imagesInFlight_[imageIndex] = frame.inFlight;

        result = vkResetFences(device_, 1, &frame.inFlight);
        if (result != VK_SUCCESS || vkResetCommandBuffer(frame.commandBuffer, 0) != VK_SUCCESS)
        {
            diagnostics::Error("Failed resetting Vulkan frame resources.");
            return FrameResult::FatalError;
        }

        // update the uniform buffer with the latest scene data for this frame
        std::memcpy(frame.uniformBuffer.GetMappedData(), &uniform, sizeof(uniform));
        frame.uniformBuffer.Flush();
        if (!RecordSceneCommands(frame.commandBuffer, imageIndex, swapchain, pipeline,
                                 resources, drawList, fallbackTexture, frame.descriptorSet, overlay, currentFrame_))
        {
            return FrameResult::FatalError;
        }

        // add to graphic queue for execution, and signal the inFlight fence when rendering is finished.
        constexpr VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &frame.imageAvailable;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &frame.commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinished_[imageIndex];

        result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, frame.inFlight);
        if (result != VK_SUCCESS)
        {
            diagnostics::Error(
                "Failed submitting Vulkan frame. VkResult=" +
                std::to_string(static_cast<int>(result)) + ".");
            return FrameResult::FatalError;
        }

        // add to present queue for presentation, and wait for the renderFinished semaphore to ensure rendering is complete before presenting.
        const VkSwapchainKHR swapchainHandle = swapchain.GetHandle();
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinished_[imageIndex];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchainHandle;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(presentQueue, &presentInfo);
        currentFrame_ = (currentFrame_ + 1) % framesInFlight;
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || acquiredSuboptimal)
        {
            return FrameResult::SwapchainOutOfDate;
        }
        if (result != VK_SUCCESS)
        {
            diagnostics::Error(
                "Failed presenting Vulkan frame. VkResult=" +
                std::to_string(static_cast<int>(result)) + ".");
            return FrameResult::FatalError;
        }
        return FrameResult::Presented;
    }

    void VulkanFrameResources::Shutdown() noexcept
    {
        if (device_ == VK_NULL_HANDLE)
        {
            return;
        }

        for (Frame &frame : frames_)
        {
            if (frame.inFlight != VK_NULL_HANDLE)
            {
                vkDestroyFence(device_, frame.inFlight, nullptr);
            }
            if (frame.imageAvailable != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(device_, frame.imageAvailable, nullptr);
            }
            frame = {};
        }

        if (descriptorPool_ != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
            descriptorPool_ = VK_NULL_HANDLE;
        }

        for (const VkSemaphore semaphore : renderFinished_)
        {
            if (semaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(device_, semaphore, nullptr);
            }
        }
        renderFinished_.clear();

        if (commandPool_ != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(device_, commandPool_, nullptr);
        }

        commandPool_ = VK_NULL_HANDLE;
        imagesInFlight_.clear();
        currentFrame_ = 0;
        device_ = VK_NULL_HANDLE;
        diagnostics::Info("Vulkan frame submission resources shut down.");
    }
}
