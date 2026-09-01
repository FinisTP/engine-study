#pragma once

#include <volk.h>

#include <cstdint>
#include <limits>

namespace umbra::render
{
    // A queue family is like group of CPU work queues that share the same capabilities.
    // This 'queue' is a logical concept in Vulkan, and the actual implementation is up to the GPU driver.
    struct QueueFamilyIndices final
    {
        static constexpr std::uint32_t invalidIndex = std::numeric_limits<std::uint32_t>::max();

        // The index of the queue that supports graphics commands
        std::uint32_t graphics = invalidIndex;

        // The index of the queue that supports presentation to a surface
        std::uint32_t present = invalidIndex;

        [[nodiscard]] constexpr bool IsComplete() const noexcept
        {
            return graphics != invalidIndex && present != invalidIndex;
        }
    };

    // A physical device is a GPU that can execute Vulkan commands.
    class VulkanPhysicalDevice final
    {
    public:
        // Fetch all GPU, evaluate, and select the best one for rendering.
        [[nodiscard]] bool Initialize(VkInstance instance, VkSurfaceKHR surface);
        void Shutdown() noexcept;

        [[nodiscard]] VkPhysicalDevice GetHandle() const noexcept { return device_; }
        [[nodiscard]] const QueueFamilyIndices &GetQueueFamilyIndices() const noexcept { return queueFamilies_; }
        [[nodiscard]] const VkPhysicalDeviceProperties &GetProperties() const noexcept { return properties_; }
        [[nodiscard]] const VkPhysicalDeviceFeatures &GetFeatures() const noexcept { return features_; }

    private:
        VkPhysicalDevice device_ = VK_NULL_HANDLE;
        QueueFamilyIndices queueFamilies_{};
        VkPhysicalDeviceProperties properties_{};
        VkPhysicalDeviceFeatures features_{};
    };
}
