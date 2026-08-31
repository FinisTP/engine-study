#include <umbra/platform/Platform.hpp>
#include <string>
#include <SDL3/SDL.h>
#include <umbra/diagnostics/Log.hpp>

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
