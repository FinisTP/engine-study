#include <Platform.hpp>
#include <string>
#include <SDL3/SDL.h>
#include <Log.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef _WIN32
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 2 // K32GetProcessMemoryInfo lives in kernel32: no extra library
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

namespace umbra::platform
{
	Platform::~Platform() noexcept
	{
		Shutdown();
	}

	bool Platform::Initialize()
	{
		if (isInitialized_)
		{
			diagnostics::Error("Platform is already initialized.");
			return false;
		}

		if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
		{
			diagnostics::Error(std::string{"SDL initialization failed: "} + SDL_GetError());
			return false;
		}

		isInitialized_ = true;
		diagnostics::Info("Platform initialized.");
		return true;
	}

	void Platform::Shutdown() noexcept
	{
		if (!isInitialized_)
		{
			return;
		}

		SDL_Quit();
		isInitialized_ = false;

		try
		{
			diagnostics::Info("Platform shut down.");
		}
		catch (...)
		{
		}
	}
}

namespace umbra::platform
{
	std::filesystem::path Platform::PreferencesFolder()
	{
		// SDL creates the folder (per user, per app) and returns it with a trailing separator.
		char *path = SDL_GetPrefPath("Umbra", "UmbraEngine");
		if (path == nullptr)
			return {};
		std::filesystem::path folder{reinterpret_cast<const char8_t *>(path)}; // SDL paths are UTF-8
		SDL_free(path);
		return folder;
	}

	int Platform::CpuThreads()
	{
		return SDL_GetNumLogicalCPUCores();
	}

	int Platform::SystemRamMb()
	{
		return SDL_GetSystemRAM();
	}

	void Platform::PersistFiles()
	{
#ifdef __EMSCRIPTEN__
		// The page (umbra-page.js) mounted IndexedDB under /libsdl and reports failures on screen.
		EM_ASM({ if (Module.persistSaves) Module.persistSaves(); });
#endif
	}

	void Platform::Idle(int milliseconds)
	{
#ifdef __EMSCRIPTEN__
		emscripten_sleep(static_cast<unsigned>(milliseconds)); // Asyncify: the page keeps running
#else
		SDL_Delay(static_cast<Uint32>(milliseconds));
#endif
	}

	int Platform::PeakMemoryMb()
	{
#ifdef _WIN32
		PROCESS_MEMORY_COUNTERS counters{};
		counters.cb = sizeof(counters);
		return GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) ? static_cast<int>(counters.PeakWorkingSetSize / (1024 * 1024)) : 0;
#else
		rusage usage{};
		return getrusage(RUSAGE_SELF, &usage) == 0 ? static_cast<int>(usage.ru_maxrss / 1024) : 0; // Linux reports KiB
#endif
	}

	std::filesystem::path Platform::ProgramFolder()
	{
		const char *base = SDL_GetBasePath(); // owned by SDL, UTF-8, ends with a separator
		return base ? std::filesystem::path{reinterpret_cast<const char8_t *>(base)} : std::filesystem::path{};
	}
}
