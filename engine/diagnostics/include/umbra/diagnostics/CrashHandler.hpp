#pragma once

namespace umbra::diagnostics
{
    void InstallCrashHandlers() noexcept;
    void UninstallCrashHandlers() noexcept;
}