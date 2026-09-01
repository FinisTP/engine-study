#include "VulkanPhysicalDevice.hpp"

#include "PhysicalDeviceSelection.hpp"

#include <Log.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using umbra::render::QueueFamilyIndices;
    using umbra::render::detail::PhysicalDeviceCandidate;

    struct EvaluatedDevice final
    {
        VkPhysicalDevice handle = VK_NULL_HANDLE;
        QueueFamilyIndices queueFamilies{};
        VkPhysicalDeviceProperties properties{};
        VkPhysicalDeviceFeatures features{};
        PhysicalDeviceCandidate candidate{};
    };

    [[nodiscard]] QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        std::uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        QueueFamilyIndices indices;
        for (std::uint32_t index = 0; index < queueFamilyCount; ++index)
        {
            const bool supportsGraphics = (queueFamilies[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;

            VkBool32 supportsPresent = VK_FALSE;
            const VkResult presentResult = vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &supportsPresent);
            if (presentResult != VK_SUCCESS)
            {
                supportsPresent = VK_FALSE;
            }

            if (supportsGraphics && supportsPresent == VK_TRUE)
            {
                return QueueFamilyIndices{index, index};
            }

            if (supportsGraphics && indices.graphics == QueueFamilyIndices::invalidIndex)
            {
                indices.graphics = index;
            }
            if (supportsPresent == VK_TRUE && indices.present == QueueFamilyIndices::invalidIndex)
            {
                indices.present = index;
            }
        }

        return indices;
    }

    [[nodiscard]] bool HasSwapchainExtension(VkPhysicalDevice device)
    {
        std::uint32_t extensionCount = 0;
        if (vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr) != VK_SUCCESS)
        {
            return false;
        }

        std::vector<VkExtensionProperties> extensions(extensionCount);
        if (vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data()) != VK_SUCCESS)
        {
            return false;
        }

        return std::ranges::any_of(extensions, [](const VkExtensionProperties &extension)
                                   { return std::string_view{extension.extensionName} == VK_KHR_SWAPCHAIN_EXTENSION_NAME; });
    }

    [[nodiscard]] bool HasSurfaceFormats(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        std::uint32_t formatCount = 0;
        return vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr) == VK_SUCCESS &&
               formatCount > 0;
    }

    [[nodiscard]] bool HasPresentModes(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        std::uint32_t presentModeCount = 0;
        return vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr) == VK_SUCCESS &&
               presentModeCount > 0;
    }

    [[nodiscard]] EvaluatedDevice EvaluateDevice(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        EvaluatedDevice evaluated;
        evaluated.handle = device;
        vkGetPhysicalDeviceProperties(device, &evaluated.properties);
        vkGetPhysicalDeviceFeatures(device, &evaluated.features);
        evaluated.queueFamilies = FindQueueFamilies(device, surface);

        evaluated.candidate.deviceType = evaluated.properties.deviceType;
        evaluated.candidate.maxImageDimension2D = evaluated.properties.limits.maxImageDimension2D;
        evaluated.candidate.hasGraphicsQueue =
            evaluated.queueFamilies.graphics != QueueFamilyIndices::invalidIndex;
        evaluated.candidate.hasPresentQueue =
            evaluated.queueFamilies.present != QueueFamilyIndices::invalidIndex;
        evaluated.candidate.hasSwapchainExtension = HasSwapchainExtension(device);

        if (evaluated.candidate.hasSwapchainExtension)
        {
            evaluated.candidate.hasSurfaceFormats = HasSurfaceFormats(device, surface);
            evaluated.candidate.hasPresentModes = HasPresentModes(device, surface);
        }

        evaluated.candidate.hasSamplerAnisotropy = evaluated.features.samplerAnisotropy == VK_TRUE;
        return evaluated;
    }

    [[nodiscard]] std::string RejectionReason(const PhysicalDeviceCandidate &candidate)
    {
        std::string reason;
        const auto appendReason = [&reason](std::string_view missingRequirement)
        {
            if (!reason.empty())
            {
                reason += ", ";
            }
            reason += missingRequirement;
        };

        if (!candidate.hasGraphicsQueue)
        {
            appendReason("no graphics queue");
        }
        if (!candidate.hasPresentQueue)
        {
            appendReason("no presentation queue");
        }
        if (!candidate.hasSwapchainExtension)
        {
            appendReason("VK_KHR_swapchain unavailable");
        }
        if (!candidate.hasSurfaceFormats)
        {
            appendReason("no surface formats");
        }
        if (!candidate.hasPresentModes)
        {
            appendReason("no present modes");
        }
        if (!candidate.hasSamplerAnisotropy)
        {
            appendReason("sampler anisotropy unavailable");
        }

        return reason;
    }
}

namespace umbra::render
{
    bool VulkanPhysicalDevice::Initialize(VkInstance instance, VkSurfaceKHR surface)
    {
        if (device_ != VK_NULL_HANDLE)
        {
            diagnostics::Error("Cannot select Vulkan physical device: one is already selected.");
            return false;
        }

        if (instance == VK_NULL_HANDLE || surface == VK_NULL_HANDLE)
        {
            diagnostics::Error("Cannot select Vulkan physical device: instance and surface must be valid.");
            return false;
        }

        std::uint32_t deviceCount = 0;
        VkResult enumerateResult = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (enumerateResult != VK_SUCCESS || deviceCount == 0)
        {
            diagnostics::Error(
                "Failed to enumerate Vulkan physical devices. VkResult=" +
                std::to_string(static_cast<int>(enumerateResult)) + ".");
            return false;
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        enumerateResult = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        if (enumerateResult != VK_SUCCESS)
        {
            diagnostics::Error(
                "Failed to read Vulkan physical devices. VkResult=" +
                std::to_string(static_cast<int>(enumerateResult)) + ".");
            return false;
        }

        std::vector<EvaluatedDevice> evaluatedDevices;
        std::vector<detail::PhysicalDeviceCandidate> candidates;
        evaluatedDevices.reserve(devices.size());
        candidates.reserve(devices.size());

        for (VkPhysicalDevice device : devices)
        {
            EvaluatedDevice evaluated = EvaluateDevice(device, surface);
            if (!detail::IsSuitable(evaluated.candidate))
            {
                diagnostics::Info(
                    std::string{"Rejected Vulkan physical device '"} + evaluated.properties.deviceName +
                    "': " + RejectionReason(evaluated.candidate) + ".");
            }

            candidates.push_back(evaluated.candidate);
            evaluatedDevices.push_back(evaluated);
        }

        const std::optional<std::size_t> selectedIndex = detail::SelectBestCandidate(candidates);
        if (!selectedIndex.has_value())
        {
            diagnostics::Error("No Vulkan physical device satisfies Umbra's rendering requirements.");
            return false;
        }

        const EvaluatedDevice &selected = evaluatedDevices[*selectedIndex];
        device_ = selected.handle;
        queueFamilies_ = selected.queueFamilies;
        properties_ = selected.properties;
        features_ = selected.features;

        diagnostics::Info(
            std::string{"Selected Vulkan physical device '"} + properties_.deviceName +
            "' (graphics queue=" + std::to_string(queueFamilies_.graphics) +
            ", present queue=" + std::to_string(queueFamilies_.present) + ").");
        return true;
    }

    void VulkanPhysicalDevice::Shutdown() noexcept
    {
        device_ = VK_NULL_HANDLE;
        queueFamilies_ = {};
        properties_ = {};
        features_ = {};
    }
}
