#pragma once

#include <string_view>
#include <source_location>

#if !defined(NDEBUG)
#define UMBRA_ASSERT(expression, message)                                                                       \
    do                                                                                                          \
    {                                                                                                           \
        if (!(expression))                                                                                      \
        {                                                                                                       \
            ::umbra::diagnostics::detail::AssertionFailed(#expression, message, std::source_location::current()); \
        }                                                                                                       \
    } while (false)
#else
#define UMBRA_ASSERT(expression, message) ((void)0)
#endif

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
        [[noreturn]] void AssertionFailed(std::string_view expression, std::string_view message, std::source_location location) noexcept;
    }
}