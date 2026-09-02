#pragma once

#include "VulkanPhysicalDevice.hpp"

#include <volk.h>

namespace umbra::render
{
    // If the physical device defines "which GPU we use",
    // the logical device defines "how we use that GPU".
    class VulkanDevice final
    {
    public:
        VulkanDevice() = default;
        ~VulkanDevice();

        VulkanDevice(const VulkanDevice &) = delete;
        VulkanDevice &operator=(const VulkanDevice &) = delete;
        VulkanDevice(VulkanDevice &&) = delete;
        VulkanDevice &operator=(VulkanDevice &&) = delete;

        [[nodiscard]] bool Initialize(const VulkanPhysicalDevice &physicalDevice);

        // Wait until the device is idle (all pending operations are completed).
        // We don't want to destroy the device while it is still processing commands.
        [[nodiscard]] bool WaitIdle() const noexcept;
        void Shutdown() noexcept;

        [[nodiscard]] VkDevice GetHandle() const noexcept { return device_; }
        [[nodiscard]] VkQueue GetGraphicsQueue() const noexcept { return graphicsQueue_; }
        [[nodiscard]] VkQueue GetPresentQueue() const noexcept { return presentQueue_; }

    private:
        VkDevice device_ = VK_NULL_HANDLE;
        VkQueue graphicsQueue_ = VK_NULL_HANDLE;
        VkQueue presentQueue_ = VK_NULL_HANDLE;
    };
}
