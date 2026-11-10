#include <CrashHandler.hpp>
#include <Log.hpp>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string_view>

#if defined(_WIN32)
#include <Windows.h>
// After Windows.h:
#include <DbgHelp.h>
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

        // 1. Where it happened: the address and the module (exe or DLL) it belongs to.
        const void *address = exceptionPointers != nullptr && exceptionPointers->ExceptionRecord != nullptr
                                  ? exceptionPointers->ExceptionRecord->ExceptionAddress
                                  : nullptr;
        char module[MAX_PATH] = "?";
        HMODULE handle = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               static_cast<LPCSTR>(address), &handle))
            GetModuleFileNameA(handle, module, sizeof(module));
        char message[MAX_PATH + 160]{};
        std::snprintf(message, sizeof(message), "Unhandled Windows exception. Exception code: 0x%08lX at %p (%s + 0x%llX)", code, address,
                      module, static_cast<unsigned long long>(static_cast<const char *>(address) - reinterpret_cast<const char *>(handle)));
        ReportCrash(message);

        // 2. A minidump next to the program (crash-<time>.dmp): open it in Visual Studio with the
        //    .pdb files to see the exact line and call stack.
        char dumpPath[MAX_PATH]{};
        SYSTEMTIME time{};
        GetLocalTime(&time);
        std::snprintf(dumpPath, sizeof(dumpPath), "crash-%04u%02u%02u-%02u%02u%02u.dmp", time.wYear, time.wMonth, time.wDay, time.wHour,
                      time.wMinute, time.wSecond);
        const HANDLE file = CreateFileA(dumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE)
        {
            MINIDUMP_EXCEPTION_INFORMATION info{GetCurrentThreadId(), exceptionPointers, FALSE};
            if (MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpWithIndirectlyReferencedMemory, &info, nullptr, nullptr))
            {
                char saved[MAX_PATH + 64]{};
                std::snprintf(saved, sizeof(saved), "Crash dump written: %s (in the working folder)", dumpPath);
                ReportCrash(saved);
            }
            CloseHandle(file);
        }
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
