#include "VulkanInstance.hpp"

#include <Log.hpp>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace
{
#if !defined(NDEBUG)
    constexpr bool requestValidation = true;
#else
    constexpr bool requestValidation = false;
#endif

    constexpr std::string_view validationLayerName = "VK_LAYER_KHRONOS_validation";

    [[nodiscard]] bool HasLayer(std::string_view requestedLayer)
    {
        std::uint32_t layerCount = 0;
        if (vkEnumerateInstanceLayerProperties(&layerCount, nullptr) != VK_SUCCESS)
        {
            return false;
        }

        std::vector<VkLayerProperties> layers(layerCount);
        if (vkEnumerateInstanceLayerProperties(&layerCount, layers.data()) != VK_SUCCESS)
        {
            return false;
        }

        return std::ranges::any_of(layers, [requestedLayer](const VkLayerProperties &layer)
                                   { return requestedLayer == layer.layerName; });
    }

    [[nodiscard]] bool HasExtension(std::string_view requestedExtension)
    {
        std::uint32_t extensionCount = 0;
        if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr) != VK_SUCCESS)
        {
            return false;
        }

        std::vector<VkExtensionProperties> extensions(extensionCount);
        if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data()) != VK_SUCCESS)
        {
            return false;
        }

        return std::ranges::any_of(extensions, [requestedExtension](const VkExtensionProperties &extension)
                                   { return requestedExtension == extension.extensionName; });
    }

    [[nodiscard]] const char *SeverityName(VkDebugUtilsMessageSeverityFlagBitsEXT severity) noexcept
    {
        if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
        {
            return "error";
        }
        if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
        {
            return "warning";
        }
        if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0)
        {
            return "info";
        }
        return "verbose";
    }

    [[nodiscard]] std::string MessageTypeNames(VkDebugUtilsMessageTypeFlagsEXT types)
    {
        std::string names;
        const auto appendName = [&names](std::string_view name)
        {
            if (!names.empty())
            {
                names += '|';
            }
            names += name;
        };

        if ((types & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) != 0)
        {
            appendName("general");
        }
        if ((types & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0)
        {
            appendName("validation");
        }
        if ((types & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0)
        {
            appendName("performance");
        }

        return names.empty() ? "unknown" : names;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL ValidationCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT types,
        const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
        void *) noexcept
    {
        try
        {
            const char *message = callbackData != nullptr && callbackData->pMessage != nullptr
                                      ? callbackData->pMessage
                                      : "Vulkan validation produced an empty message.";
            const std::string formattedMessage = std::string{"Vulkan validation ["} + SeverityName(severity) + "][" +
                                                 MessageTypeNames(types) + "]: " + message;

            if ((severity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) != 0)
            {
                umbra::diagnostics::Error(formattedMessage);
            }
            else
            {
                umbra::diagnostics::Info(formattedMessage);
            }
        }
        catch (...)
        {
        }

        return VK_FALSE;
    }

    [[nodiscard]] VkDebugUtilsMessengerCreateInfoEXT MakeDebugMessengerCreateInfo() noexcept
    {
        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = ValidationCallback;
        return createInfo;
    }
}

namespace umbra::render
{
    VulkanInstance::~VulkanInstance()
    {
        Shutdown();
    }

    bool VulkanInstance::Initialize()
    {
        if (instance_ != VK_NULL_HANDLE)
        {
            diagnostics::Error("Cannot initialize Vulkan instance: it is already initialized.");
            return false;
        }

        const VkResult volkResult = volkInitialize();
        if (volkResult != VK_SUCCESS)
        {
            diagnostics::Error("Failed to initialize Volk. VkResult=" + std::to_string(static_cast<int>(volkResult)) + ".");
            return false;
        }

        Uint32 sdlExtensionCount = 0;
        char const *const *sdlExtensionNames = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
        if (sdlExtensionNames == nullptr || sdlExtensionCount == 0)
        {
            diagnostics::Error(std::string{"Failed to get Vulkan instance extensions from SDL: "} + SDL_GetError());
            return false;
        }

        std::vector<const char *> extensions(sdlExtensionNames, sdlExtensionNames + sdlExtensionCount);
        std::vector<const char *> layers;

        bool validationEnabled = false;
        if constexpr (requestValidation)
        {
            const bool validationLayerAvailable = HasLayer(validationLayerName);
            const bool debugUtilsAvailable = HasExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            validationEnabled = validationLayerAvailable && debugUtilsAvailable;

            if (validationEnabled)
            {
                layers.push_back(validationLayerName.data());
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }
            else
            {
                diagnostics::Error(
                    "Vulkan validation requested but VK_LAYER_KHRONOS_validation or VK_EXT_debug_utils is unavailable; "
                    "continuing without validation.");
            }
        }

        std::uint32_t supportedApiVersion = VK_API_VERSION_1_0;
        if (vkEnumerateInstanceVersion != nullptr)
        {
            const VkResult versionResult = vkEnumerateInstanceVersion(&supportedApiVersion);
            if (versionResult != VK_SUCCESS)
            {
                diagnostics::Error(
                    "Failed to query the Vulkan instance version. VkResult=" +
                    std::to_string(static_cast<int>(versionResult)) + ".");
                return false;
            }
        }

        VkApplicationInfo applicationInfo{};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = "Umbra";
        applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
        applicationInfo.pEngineName = "Umbra Engine";
        applicationInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
        applicationInfo.apiVersion = std::min(supportedApiVersion, VK_API_VERSION_1_3);

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = MakeDebugMessengerCreateInfo();

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &applicationInfo;
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();
        createInfo.pNext = validationEnabled ? &debugCreateInfo : nullptr;

        const VkResult createResult = vkCreateInstance(&createInfo, nullptr, &instance_);
        if (createResult != VK_SUCCESS)
        {
            instance_ = VK_NULL_HANDLE;
            diagnostics::Error(
                "Failed to create Vulkan instance. VkResult=" + std::to_string(static_cast<int>(createResult)) + ".");
            return false;
        }

        volkLoadInstance(instance_);

        if (validationEnabled)
        {
            const VkResult messengerResult = vkCreateDebugUtilsMessengerEXT(instance_, &debugCreateInfo, nullptr, &debugMessenger_);
            if (messengerResult != VK_SUCCESS)
            {
                diagnostics::Error(
                    "Failed to create the Vulkan debug messenger. VkResult=" +
                    std::to_string(static_cast<int>(messengerResult)) + ".");
                Shutdown();
                return false;
            }
        }

        diagnostics::Info(validationEnabled
                              ? "Vulkan instance initialized with validation enabled."
                              : "Vulkan instance initialized without validation.");
        return true;
    }

    void VulkanInstance::Shutdown() noexcept
    {
        if (instance_ == VK_NULL_HANDLE)
        {
            return;
        }

        if (debugMessenger_ != VK_NULL_HANDLE)
        {
            vkDestroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
            debugMessenger_ = VK_NULL_HANDLE;
        }

        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
        diagnostics::Info("Vulkan instance shut down.");
    }
}
