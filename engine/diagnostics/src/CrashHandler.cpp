#include <umbra/diagnostics/CrashHandler.hpp>
#include <umbra/diagnostics/Log.hpp>
#include <atomic>
#include <cstdlib>
#include <exception>
#include <string>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace
{
    std::terminate_handler previousTerminateHandler = nullptr;
    bool handlersInstalled = false;
    std::atomic_flag handlingCrash = ATOMIC_FLAG_INIT;

    [[noreturn]] void HandleTerminate() noexcept
    {
        if (handlingCrash.test_and_set())
        {
            // Already handling a crash, avoid recursive calls
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
                umbra::diagnostics::Critical(std::string{"Unhandled exception: "} + ex.what());
            }
            catch (...)
            {
                umbra::diagnostics::Critical("Unhandled unknown exception.");
            }
        }
        else
        {
            umbra::diagnostics::Critical("Terminate called without an active exception.");
        }

        umbra::diagnostics::Flush();

        if (previousTerminateHandler)
        {
            previousTerminateHandler();
        }
        else
        {
            std::abort();
        }
    }

#if defined(_WIN32)
    LPTOP_LEVEL_EXCEPTION_FILTER previousWindowsHandler = nullptr;
    LONG WINAPI HandleWindowsException(EXCEPTION_POINTERS *exceptionPointers) noexcept
    {
        const auto code = exceptionPointers->ExceptionRecord->ExceptionCode;
        umbra::diagnostics::Critical("Unhandled exception occurred. Exception code: " + std::to_string(code));
        umbra::diagnostics::Flush();
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
#if defined(_WIN32)
        previousWindowsHandler = SetUnhandledExceptionFilter(HandleWindowsException);
#else
        previousTerminateHandler = std::set_terminate(HandleTerminate);
#endif
        handlersInstalled = true;
    }

    void UninstallCrashHandlers() noexcept
    {
        if (!handlersInstalled)
        {
            return;
        }

#if defined(_WIN32)
        SetUnhandledExceptionFilter(previousWindowsHandler);
#else
        std::set_terminate(previousTerminateHandler);
#endif
        handlersInstalled = false;
    }
}