#include "VulkanDevice.hpp"

#include <Log.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace umbra::render
{
    VulkanDevice::~VulkanDevice()
    {
        Shutdown();
    }

    bool VulkanDevice::Initialize(const VulkanPhysicalDevice &physicalDevice)
    {
        if (device_ != VK_NULL_HANDLE)
        {
            diagnostics::Error("Cannot initialize Vulkan device: it is already initialized.");
            return false;
        }

        if (physicalDevice.GetHandle() == VK_NULL_HANDLE || !physicalDevice.GetQueueFamilyIndices().IsComplete())
        {
            diagnostics::Error("Cannot initialize Vulkan device: physical device selection is incomplete.");
            return false;
        }

        const QueueFamilyIndices &queueFamilies = physicalDevice.GetQueueFamilyIndices();

        // since graphic queue and present queue can be the same, we need to make sure we only request each unique queue family once
        const std::array requestedFamilies{queueFamilies.graphics, queueFamilies.present};
        std::vector<std::uint32_t> uniqueFamilies;
        uniqueFamilies.reserve(requestedFamilies.size());
        for (const std::uint32_t family : requestedFamilies)
        {
            bool alreadyRequested = false;
            for (const std::uint32_t requestedFamily : uniqueFamilies)
            {
                if (family == requestedFamily)
                {
                    alreadyRequested = true;
                    break;
                }
            }

            if (!alreadyRequested)
            {
                uniqueFamilies.push_back(family);
            }
        }

        constexpr float queuePriority = 1.0F;

        // Create a queue create info for each unique queue family
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        queueCreateInfos.reserve(uniqueFamilies.size());
        for (const std::uint32_t family : uniqueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = family;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // Enable required device features and extensions
        // samplerAnisotropy is required for high-quality texture sampling, and VK_KHR_swapchain is required for presenting images to the screen.
        VkPhysicalDeviceFeatures enabledFeatures{};
        enabledFeatures.samplerAnisotropy = VK_TRUE;
        constexpr std::array requiredExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();
        createInfo.pEnabledFeatures = &enabledFeatures;

        const VkResult result = vkCreateDevice(physicalDevice.GetHandle(), &createInfo, nullptr, &device_);
        if (result != VK_SUCCESS)
        {
            device_ = VK_NULL_HANDLE;
            diagnostics::Error(
                "Failed to create Vulkan logical device. VkResult=" +
                std::to_string(static_cast<int>(result)) + ".");
            return false;
        }

        volkLoadDevice(device_);
        vkGetDeviceQueue(device_, queueFamilies.graphics, 0, &graphicsQueue_);
        vkGetDeviceQueue(device_, queueFamilies.present, 0, &presentQueue_);

        if (graphicsQueue_ == VK_NULL_HANDLE || presentQueue_ == VK_NULL_HANDLE)
        {
            diagnostics::Error("Vulkan logical device returned an invalid graphics or presentation queue.");
            Shutdown();
            return false;
        }

        diagnostics::Info("Vulkan logical device initialized.");
        return true;
    }

    bool VulkanDevice::WaitIdle() const noexcept
    {
        if (device_ == VK_NULL_HANDLE)
        {
            return true;
        }

        const VkResult result = vkDeviceWaitIdle(device_);
        if (result != VK_SUCCESS)
        {
            diagnostics::Error(
                "Failed waiting for Vulkan device idle. VkResult=" +
                std::to_string(static_cast<int>(result)) + ".");
            return false;
        }
        return true;
    }

    void VulkanDevice::Shutdown() noexcept
    {
        if (device_ == VK_NULL_HANDLE)
        {
            graphicsQueue_ = VK_NULL_HANDLE;
            presentQueue_ = VK_NULL_HANDLE;
            return;
        }

        (void)WaitIdle();
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
        graphicsQueue_ = VK_NULL_HANDLE;
        presentQueue_ = VK_NULL_HANDLE;
        diagnostics::Info("Vulkan logical device shut down.");
    }
}
