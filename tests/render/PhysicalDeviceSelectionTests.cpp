#include <catch2/catch_test_macros.hpp>

#include "PhysicalDeviceSelection.hpp"

#include <array>

namespace
{
    [[nodiscard]] umbra::render::detail::PhysicalDeviceCandidate MakeSuitableCandidate(
        VkPhysicalDeviceType deviceType,
        std::uint32_t maxImageDimension2D)
    {
        return umbra::render::detail::PhysicalDeviceCandidate{
            .deviceType = deviceType,
            .maxImageDimension2D = maxImageDimension2D,
            .hasGraphicsQueue = true,
            .hasPresentQueue = true,
            .hasSwapchainExtension = true,
            .hasSurfaceFormats = true,
            .hasPresentModes = true,
            .hasSamplerAnisotropy = true};
    }
}

TEST_CASE("Physical device suitability requires every rendering capability")
{
    auto candidate = MakeSuitableCandidate(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, 8192);
    REQUIRE(umbra::render::detail::IsSuitable(candidate));

    candidate.hasGraphicsQueue = false;
    REQUIRE_FALSE(umbra::render::detail::IsSuitable(candidate));

    candidate = MakeSuitableCandidate(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, 8192);
    candidate.hasPresentQueue = false;
    REQUIRE_FALSE(umbra::render::detail::IsSuitable(candidate));

    candidate = MakeSuitableCandidate(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, 8192);
    candidate.hasSwapchainExtension = false;
    REQUIRE_FALSE(umbra::render::detail::IsSuitable(candidate));

    candidate = MakeSuitableCandidate(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, 8192);
    candidate.hasSurfaceFormats = false;
    REQUIRE_FALSE(umbra::render::detail::IsSuitable(candidate));

    candidate = MakeSuitableCandidate(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, 8192);
    candidate.hasPresentModes = false;
    REQUIRE_FALSE(umbra::render::detail::IsSuitable(candidate));

    candidate = MakeSuitableCandidate(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, 8192);
    candidate.hasSamplerAnisotropy = false;
    REQUIRE_FALSE(umbra::render::detail::IsSuitable(candidate));
}

TEST_CASE("Physical device selection ignores unsuitable candidates")
{
    auto unsuitableDiscrete = MakeSuitableCandidate(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 16384);
    unsuitableDiscrete.hasPresentQueue = false;

    const std::array candidates{
        unsuitableDiscrete,
        MakeSuitableCandidate(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, 8192)};

    const auto selected = umbra::render::detail::SelectBestCandidate(candidates);
    REQUIRE(selected.has_value());
    REQUIRE(*selected == 1);
}

TEST_CASE("Physical device selection prefers a suitable discrete GPU")
{
    const std::array candidates{
        MakeSuitableCandidate(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, 16384),
        MakeSuitableCandidate(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 8192)};

    const auto selected = umbra::render::detail::SelectBestCandidate(candidates);
    REQUIRE(selected.has_value());
    REQUIRE(*selected == 1);
}

TEST_CASE("Physical device selection reports no result without a suitable device")
{
    auto candidate = MakeSuitableCandidate(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 16384);
    candidate.hasSwapchainExtension = false;

    const std::array candidates{candidate};
    REQUIRE_FALSE(umbra::render::detail::SelectBestCandidate(candidates).has_value());
}
