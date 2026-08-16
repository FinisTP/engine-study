#include <chrono>
#include <string>
#include <thread>

#include <umbra/core/Version.hpp>
#include <umbra/diagnostics/Log.hpp>
#include <umbra/platform/Platform.hpp>

int main()
{
    umbra::diagnostics::Initialize();

    umbra::diagnostics::Info(
        std::string{"Starting Umbra "} +
        std::string{umbra::core::Version()});

    if (!umbra::platform::Initialize())
    {
        umbra::diagnostics::Shutdown();
        return 1;
    }

    {
        umbra::platform::Window window;

        if (!window.Create("Umbra", 1280, 720))
        {
            umbra::platform::Shutdown();
            umbra::diagnostics::Shutdown();
            return 1;
        }

        while (!window.PollCloseRequested())
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds{1});
        }
    }

    umbra::platform::Shutdown();
    umbra::diagnostics::Shutdown();

    return 0;
}