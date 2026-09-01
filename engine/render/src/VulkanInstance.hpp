#pragma once

#include <volk.h>

namespace umbra::render
{
    class VulkanInstance final
    {
    public:
        VulkanInstance() = default;
        ~VulkanInstance();

        [[nodiscard]] bool Initialize();
        void Shutdown() noexcept;

        VulkanInstance(const VulkanInstance &) = delete;
        VulkanInstance &operator=(const VulkanInstance &) = delete;
        VulkanInstance(VulkanInstance &&) = delete;
        VulkanInstance &operator=(VulkanInstance &&) = delete;

        [[nodiscard]] VkInstance GetHandle() const noexcept { return instance_; }

    private:
        VkInstance instance_ = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    };
}
