#pragma once

#include <volk.h>

struct SDL_Window;

namespace umbra::render
{
    // A vulkan surface allows Vulkan to present rendered images to the screen.
    class VulkanSurface final
    {
    public:
        VulkanSurface() = default;
        ~VulkanSurface();

        VulkanSurface(const VulkanSurface &) = delete;
        VulkanSurface &operator=(const VulkanSurface &) = delete;
        VulkanSurface(VulkanSurface &&) = delete;
        VulkanSurface &operator=(VulkanSurface &&) = delete;

        [[nodiscard]] bool Initialize(SDL_Window *window, VkInstance instance);

        void Shutdown() noexcept;

        [[nodiscard]] VkSurfaceKHR GetHandle() const noexcept { return surface_; }

    private:
        VkInstance instance_ = VK_NULL_HANDLE;
        VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    };
}
