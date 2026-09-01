#include <CrashHandler.hpp>
#include <Log.hpp>

#include <cstdlib>
#include <exception>
#include <string_view>

#if defined(_WIN32)
#include <Windows.h>
#endif

int main(int argumentCount, char **arguments)
{
    if (argumentCount != 2)
    {
        return 2;
    }

#if defined(_WIN32)
    ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

    umbra::diagnostics::Initialize();
    umbra::diagnostics::InstallCrashHandlers();

    const std::string_view scenario{arguments[1]};
    if (scenario == "fatal")
    {
        umbra::diagnostics::Fatal("Fatal diagnostics probe.");
    }

    if (scenario == "assert")
    {
#if defined(NDEBUG)
        return 77;
#else
        UMBRA_ASSERT(false, "Assertion diagnostics probe.");
#endif
    }

    if (scenario == "terminate")
    {
        std::terminate();
    }

#if defined(_WIN32)
    if (scenario == "windows-exception")
    {
        ::RaiseException(0xE0000001UL, 0, 0, nullptr);
    }
#endif

    umbra::diagnostics::UninstallCrashHandlers();
    umbra::diagnostics::Shutdown();
    return 3;
}
