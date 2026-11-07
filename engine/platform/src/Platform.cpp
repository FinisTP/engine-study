#include <Platform.hpp>
#include <string>
#include <SDL3/SDL.h>
#include <Log.hpp>

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

		if (!SDL_Init(SDL_INIT_VIDEO))
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

	std::filesystem::path Platform::ProgramFolder()
	{
		const char *base = SDL_GetBasePath(); // owned by SDL, UTF-8, ends with a separator
		return base ? std::filesystem::path{reinterpret_cast<const char8_t *>(base)} : std::filesystem::path{};
	}
}
