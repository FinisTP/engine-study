#pragma once

#include "VulkanPhysicalDevice.hpp"

#include <volk.h>

#include <vector>

struct SDL_Window;

namespace umbra::render
{
    class VulkanSwapchain final
    {
    public:
        VulkanSwapchain() = default;
        ~VulkanSwapchain();

        VulkanSwapchain(const VulkanSwapchain &) = delete;
        VulkanSwapchain &operator=(const VulkanSwapchain &) = delete;
        VulkanSwapchain(VulkanSwapchain &&) = delete;
        VulkanSwapchain &operator=(VulkanSwapchain &&) = delete;

        [[nodiscard]] bool Initialize(
            VkPhysicalDevice physicalDevice,
            VkDevice device,
            VkSurfaceKHR surface,
            const QueueFamilyIndices &queueFamilies,
            SDL_Window *window);
        void Shutdown() noexcept;

        [[nodiscard]] VkSwapchainKHR GetHandle() const noexcept { return swapchain_; }
        [[nodiscard]] VkFormat GetImageFormat() const noexcept { return imageFormat_; }
        [[nodiscard]] VkExtent2D GetExtent() const noexcept { return extent_; }
        [[nodiscard]] const std::vector<VkImage> &GetImages() const noexcept { return images_; }
        [[nodiscard]] const std::vector<VkImageView> &GetImageViews() const noexcept { return imageViews_; }

    private:
        VkDevice device_ = VK_NULL_HANDLE;
        VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
        VkFormat imageFormat_ = VK_FORMAT_UNDEFINED;
        VkExtent2D extent_{};
        std::vector<VkImage> images_;
        std::vector<VkImageView> imageViews_;
    };
}
