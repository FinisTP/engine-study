#include <catch2/catch_test_macros.hpp>

#include <source_location>
#include <string>

#include <umbra/diagnostics/Log.hpp>

namespace
{
    std::string FormatTestFailure()
    {
        return umbra::diagnostics::detail::FormatFailure(
            "Assertion",
            "value > 0",
            "Value must be positive",
            std::source_location::current());
    }
}

TEST_CASE("Diagnostics failure formatting contains deterministic context")
{
    const std::string formatted = FormatTestFailure();

    REQUIRE(formatted.starts_with("[Assertion] Expression: value > 0"));
    REQUIRE(formatted.find("Message: Value must be positive") != std::string::npos);
    REQUIRE(formatted.find("DiagnosticsFormattingTests.cpp") != std::string::npos);
    REQUIRE(formatted.find("FormatTestFailure") != std::string::npos);
}
