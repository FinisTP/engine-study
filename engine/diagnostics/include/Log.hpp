#pragma once

#include <source_location>
#include <string>
#include <string_view>

namespace umbra::diagnostics
{
    void Initialize();
    void Shutdown();

    void Info(std::string_view message);
    void Error(std::string_view message);
    void Critical(std::string_view message);
    void Flush() noexcept;

    [[noreturn]] void Fatal(std::string_view message, std::source_location location = std::source_location::current()) noexcept;

    namespace detail
    {
        [[nodiscard]] std::string FormatFailure(std::string_view category, std::string_view expression, std::string_view message, std::source_location location);
        [[noreturn]] void AssertionFailed(std::string_view expression, std::string_view message, std::source_location location) noexcept;
    }
}

#if !defined(NDEBUG)
#define UMBRA_ASSERT(expression, message)                                                                         \
    do                                                                                                            \
    {                                                                                                             \
        if (!(expression))                                                                                        \
        {                                                                                                         \
            ::umbra::diagnostics::detail::AssertionFailed(#expression, message, std::source_location::current()); \
        }                                                                                                         \
    } while (false)
#else
#define UMBRA_ASSERT(expression, message) ((void)0)
#endif
