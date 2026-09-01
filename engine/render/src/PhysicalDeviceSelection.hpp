#pragma once

#include <volk.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace umbra::render::detail
{
    struct PhysicalDeviceCandidate final
    {
        VkPhysicalDeviceType deviceType = VK_PHYSICAL_DEVICE_TYPE_OTHER;
        std::uint32_t maxImageDimension2D = 0;
        bool hasGraphicsQueue = false;
        bool hasPresentQueue = false;
        bool hasSwapchainExtension = false;
        bool hasSurfaceFormats = false;
        bool hasPresentModes = false;
        bool hasSamplerAnisotropy = false;
    };

    [[nodiscard]] constexpr bool IsSuitable(const PhysicalDeviceCandidate &candidate) noexcept
    {
        return candidate.hasGraphicsQueue &&
               candidate.hasPresentQueue &&
               candidate.hasSwapchainExtension &&
               candidate.hasSurfaceFormats &&
               candidate.hasPresentModes &&
               candidate.hasSamplerAnisotropy;
    }

    [[nodiscard]] constexpr std::uint64_t Score(const PhysicalDeviceCandidate &candidate) noexcept
    {
        std::uint64_t typeScore = 0;
        switch (candidate.deviceType)
        {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            typeScore = 100'000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            typeScore = 50'000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            typeScore = 10'000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            typeScore = 1'000;
            break;
        default:
            break;
        }

        return typeScore + candidate.maxImageDimension2D;
    }

    [[nodiscard]] constexpr std::optional<std::size_t> SelectBestCandidate(std::span<const PhysicalDeviceCandidate> candidates) noexcept
    {
        std::optional<std::size_t> selectedIndex;
        std::uint64_t selectedScore = 0;

        for (std::size_t index = 0; index < candidates.size(); ++index)
        {
            const PhysicalDeviceCandidate &candidate = candidates[index];
            if (!IsSuitable(candidate))
            {
                continue;
            }

            const std::uint64_t candidateScore = Score(candidate);
            if (!selectedIndex.has_value() || candidateScore > selectedScore)
            {
                selectedIndex = index;
                selectedScore = candidateScore;
            }
        }

        return selectedIndex;
    }
}
