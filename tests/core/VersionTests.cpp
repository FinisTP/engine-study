#include <catch2/catch_test_macros.hpp>

#include <umbra/core/Version.hpp>

TEST_CASE("Umbra has a version")
{
    REQUIRE_FALSE(umbra::core::Version().empty());
}