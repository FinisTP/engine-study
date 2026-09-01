#include "VulkanSurface.hpp"

#include <Log.hpp>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_vulkan.h>

#include <string>

namespace umbra::render
{

    VulkanSurface::~VulkanSurface()
    {
        Shutdown();
    }

    [[nodiscard]] bool VulkanSurface::Initialize(SDL_Window *window, VkInstance instance)
    {
        if (surface_ != VK_NULL_HANDLE)
        {
            diagnostics::Error("Cannot initialize Vulkan surface: it is already initialized.");
            return false;
        }

        if (window == nullptr || instance == VK_NULL_HANDLE)
        {
            diagnostics::Error("Cannot initialize Vulkan surface: window and instance must be valid.");
            return false;
        }

        if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface_))
        {
            diagnostics::Error(std::string{"Failed to create Vulkan surface: "} + SDL_GetError());
            surface_ = VK_NULL_HANDLE;
            return false;
        }

        instance_ = instance;
        diagnostics::Info("Vulkan surface initialized.");
        return true;
    }

    void VulkanSurface::Shutdown() noexcept
    {
        if (surface_ == VK_NULL_HANDLE)
        {
            instance_ = VK_NULL_HANDLE;
            return;
        }

        SDL_Vulkan_DestroySurface(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
        instance_ = VK_NULL_HANDLE;
        diagnostics::Info("Vulkan surface shut down.");
    }
}
