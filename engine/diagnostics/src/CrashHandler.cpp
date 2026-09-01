#include <umbra/diagnostics/CrashHandler.hpp>
#include <umbra/diagnostics/Log.hpp>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string_view>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace
{
    std::terminate_handler previousTerminateHandler = nullptr;
    bool handlersInstalled = false;
    std::atomic_flag handlingCrash = ATOMIC_FLAG_INIT;

    void ReportCrash(std::string_view message) noexcept
    {
        try
        {
            umbra::diagnostics::Critical(message);
            umbra::diagnostics::Flush();
        }
        catch (...)
        {
        }
    }

    [[noreturn]] void HandleTerminate() noexcept
    {
        // handle recursively because std::terminate might be called again during crash handling
        if (handlingCrash.test_and_set())
        {
            std::abort();
        }

        const std::exception_ptr currentException = std::current_exception();
        if (currentException)
        {
            try
            {
                std::rethrow_exception(currentException);
            }
            catch (const std::exception &ex)
            {
                ReportCrash("Unhandled C++ exception reached std::terminate.");
                ReportCrash(ex.what());
            }
            catch (...)
            {
                ReportCrash("Unknown C++ exception reached std::terminate.");
            }
        }
        else
        {
            ReportCrash("std::terminate called without an active exception.");
        }

        std::abort();
    }

#if defined(_WIN32)
    LPTOP_LEVEL_EXCEPTION_FILTER previousWindowsHandler = nullptr;
    LONG WINAPI HandleWindowsException(EXCEPTION_POINTERS *exceptionPointers) noexcept
    {
        if (handlingCrash.test_and_set())
        {
            return EXCEPTION_EXECUTE_HANDLER;
        }

        const unsigned long code = exceptionPointers != nullptr && exceptionPointers->ExceptionRecord != nullptr
                                       ? exceptionPointers->ExceptionRecord->ExceptionCode
                                       : 0UL;

        char message[96]{};
        std::snprintf(message, sizeof(message), "Unhandled Windows exception. Exception code: 0x%08lX", code);
        ReportCrash(message);
        return EXCEPTION_EXECUTE_HANDLER;
    }
#endif
}

namespace umbra::diagnostics
{
    void InstallCrashHandlers() noexcept
    {
        if (handlersInstalled)
        {
            return;
        }
        previousTerminateHandler = std::set_terminate(HandleTerminate);
#if defined(_WIN32)
        previousWindowsHandler = SetUnhandledExceptionFilter(HandleWindowsException);
#endif
        handlingCrash.clear();
        handlersInstalled = true;
    }

    void UninstallCrashHandlers() noexcept
    {
        if (!handlersInstalled)
        {
            return;
        }

        std::set_terminate(previousTerminateHandler);
#if defined(_WIN32)
        SetUnhandledExceptionFilter(previousWindowsHandler);
#endif
        previousTerminateHandler = nullptr;
#if defined(_WIN32)
        previousWindowsHandler = nullptr;
#endif
        handlingCrash.clear();
        handlersInstalled = false;
    }
}
