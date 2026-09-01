#include "VulkanSwapchain.hpp"

#include <Log.hpp>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace
{
    struct SwapchainSupport final
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    // Query the swapchain support details for a given physical device and surface.
    [[nodiscard]] bool QuerySwapchainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, SwapchainSupport &support)
    {
        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &support.capabilities) != VK_SUCCESS)
        {
            return false;
        }

        std::uint32_t formatCount = 0;
        if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr) != VK_SUCCESS || formatCount == 0)
        {
            return false;
        }
        support.formats.resize(formatCount);
        if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, support.formats.data()) != VK_SUCCESS)
        {
            return false;
        }

        std::uint32_t presentModeCount = 0;
        if (vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr) != VK_SUCCESS || presentModeCount == 0)
        {
            return false;
        }
        support.presentModes.resize(presentModeCount);
        return vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, support.presentModes.data()) == VK_SUCCESS;
    }

    // Choose most suitable surface format, prefer VK_FORMAT_B8G8R8A8_SRGB with VK_COLOR_SPACE_SRGB_NONLINEAR_KHR because it is widely supported and provides good color accuracy.
    [[nodiscard]] VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats)
    {
        for (const VkSurfaceFormatKHR &format : formats)
        {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return format;
            }
        }
        return formats.front();
    }

    // Choose the most suitable present mode, prefer VK_PRESENT_MODE_MAILBOX_KHR for low latency and no tearing, fallback to VK_PRESENT_MODE_FIFO_KHR which is guaranteed to be available.
    [[nodiscard]] VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR> &presentModes)
    {
        for (const VkPresentModeKHR presentMode : presentModes)
        {
            if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return presentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    // An extent represents width and height of the swapchain images.
    [[nodiscard]] bool ChooseExtent(const VkSurfaceCapabilitiesKHR &capabilities, SDL_Window *window, VkExtent2D &extent)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max())
        {
            extent = capabilities.currentExtent;
            return extent.width > 0 && extent.height > 0;
        }

        int pixelWidth = 0;
        int pixelHeight = 0;
        if (!SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight))
        {
            umbra::diagnostics::Error(std::string{"Failed to query SDL window pixel extent: "} + SDL_GetError());
            return false;
        }
        if (pixelWidth <= 0 || pixelHeight <= 0)
        {
            return false;
        }

        extent.width = std::clamp(static_cast<std::uint32_t>(pixelWidth), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(static_cast<std::uint32_t>(pixelHeight), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return true;
    }

    [[nodiscard]] VkCompositeAlphaFlagBitsKHR ChooseCompositeAlpha(VkCompositeAlphaFlagsKHR supportedFlags)
    {
        constexpr std::array candidates{
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};
        for (const VkCompositeAlphaFlagBitsKHR candidate : candidates)
        {
            if ((supportedFlags & candidate) != 0)
            {
                return candidate;
            }
        }
        return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }
}

namespace umbra::render
{
    VulkanSwapchain::~VulkanSwapchain()
    {
        Shutdown();
    }

    bool VulkanSwapchain::Initialize(
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        VkSurfaceKHR surface,
        const QueueFamilyIndices &queueFamilies,
        SDL_Window *window)
    {
        if (swapchain_ != VK_NULL_HANDLE)
        {
            diagnostics::Error("Cannot initialize Vulkan swapchain: it is already initialized.");
            return false;
        }
        if (physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE ||
            surface == VK_NULL_HANDLE || window == nullptr || !queueFamilies.IsComplete())
        {
            diagnostics::Error("Cannot initialize Vulkan swapchain: required handles are invalid.");
            return false;
        }

        SwapchainSupport support;
        if (!QuerySwapchainSupport(physicalDevice, surface, support))
        {
            diagnostics::Error("Failed to query Vulkan swapchain support.");
            return false;
        }

        const VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(support.formats);
        const VkPresentModeKHR presentMode = ChoosePresentMode(support.presentModes);
        VkExtent2D selectedExtent{};
        if (!ChooseExtent(support.capabilities, window, selectedExtent))
        {
            diagnostics::Error("Cannot create Vulkan swapchain for a zero-sized or invalid window.");
            return false;
        }

        std::uint32_t imageCount = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount)
        {
            imageCount = support.capabilities.maxImageCount;
        }

        const std::array familyIndices{queueFamilies.graphics, queueFamilies.present};
        const bool hasDistinctQueueFamilies = queueFamilies.graphics != queueFamilies.present;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = selectedExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = hasDistinctQueueFamilies ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = hasDistinctQueueFamilies ? static_cast<std::uint32_t>(familyIndices.size()) : 0;
        createInfo.pQueueFamilyIndices = hasDistinctQueueFamilies ? familyIndices.data() : nullptr;
        createInfo.preTransform = support.capabilities.currentTransform;
        createInfo.compositeAlpha = ChooseCompositeAlpha(support.capabilities.supportedCompositeAlpha);
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        const VkResult createResult = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain_);
        if (createResult != VK_SUCCESS)
        {
            swapchain_ = VK_NULL_HANDLE;
            diagnostics::Error(
                "Failed to create Vulkan swapchain. VkResult=" +
                std::to_string(static_cast<int>(createResult)) + ".");
            return false;
        }

        device_ = device;
        imageFormat_ = surfaceFormat.format;
        extent_ = selectedExtent;

        // first we fetch the number of swapchain images available
        // swapchain images are actual images presented to the screen (like a GPU framebuffer)
        std::uint32_t actualImageCount = 0;
        VkResult imageResult = vkGetSwapchainImagesKHR(device_, swapchain_, &actualImageCount, nullptr);
        if (imageResult != VK_SUCCESS || actualImageCount == 0)
        {
            diagnostics::Error("Failed to query Vulkan swapchain images.");
            Shutdown();
            return false;
        }

        images_.resize(actualImageCount);
        imageResult = vkGetSwapchainImagesKHR(device_, swapchain_, &actualImageCount, images_.data());
        if (imageResult != VK_SUCCESS)
        {
            diagnostics::Error("Failed to retrieve Vulkan swapchain images.");
            Shutdown();
            return false;
        }

        // for each swapchain image, we create an image view
        // image views are used to describe how to access the image data (like a GPU texture)
        imageViews_.reserve(images_.size());
        for (const VkImage image : images_)
        {
            VkImageViewCreateInfo viewCreateInfo{};
            viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewCreateInfo.image = image;
            viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewCreateInfo.format = imageFormat_;
            viewCreateInfo.components = {
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY};
            viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewCreateInfo.subresourceRange.baseMipLevel = 0;
            viewCreateInfo.subresourceRange.levelCount = 1;
            viewCreateInfo.subresourceRange.baseArrayLayer = 0;
            viewCreateInfo.subresourceRange.layerCount = 1;

            VkImageView imageView = VK_NULL_HANDLE;
            const VkResult viewResult = vkCreateImageView(device_, &viewCreateInfo, nullptr, &imageView);
            if (viewResult != VK_SUCCESS)
            {
                diagnostics::Error(
                    "Failed to create Vulkan swapchain image view. VkResult=" +
                    std::to_string(static_cast<int>(viewResult)) + ".");
                Shutdown();
                return false;
            }
            imageViews_.push_back(imageView);
        }

        diagnostics::Info(
            "Vulkan swapchain initialized with " + std::to_string(images_.size()) +
            " images at " + std::to_string(extent_.width) + "x" + std::to_string(extent_.height) + ".");
        return true;
    }

    void VulkanSwapchain::Shutdown() noexcept
    {
        if (device_ != VK_NULL_HANDLE)
        {
            for (const VkImageView imageView : imageViews_)
            {
                vkDestroyImageView(device_, imageView, nullptr);
            }
            if (swapchain_ != VK_NULL_HANDLE)
            {
                vkDestroySwapchainKHR(device_, swapchain_, nullptr);
            }
        }

        imageViews_.clear();
        images_.clear();
        swapchain_ = VK_NULL_HANDLE;
        imageFormat_ = VK_FORMAT_UNDEFINED;
        extent_ = {};
        device_ = VK_NULL_HANDLE;
        diagnostics::Info("Vulkan swapchain shut down.");
    }
}
