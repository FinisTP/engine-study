#pragma once

#include <string_view>

namespace umbra::diagnostics
{
    void Initialize();
    void Info(std::string_view message);
    void Error(std::string_view message);
    void Shutdown();
}