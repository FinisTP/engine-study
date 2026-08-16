#pragma once

#include <string_view>

namespace umbra::core
{
    [[nodiscard]] std::string_view Version() noexcept;
}