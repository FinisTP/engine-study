#pragma once

#include <volk.h>

#include <cstdint>
#include <limits>

namespace umbra::render
{
    struct QueueFamilyIndices final
    {
        static constexpr std::uint32_t invalidIndex = std::numeric_limits<std::uint32_t>::max();

        std::uint32_t graphics = invalidIndex;
        std::uint32_t present = invalidIndex;

        [[nodiscard]] constexpr bool IsComplete() const noexcept
        {
            return graphics != invalidIndex && present != invalidIndex;
        }
    };

    class VulkanPhysicalDevice final
    {
    public:
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
