#include <umbra/platform/Platform.hpp>
#include <string>
#include <SDL3/SDL.h>
#include <umbra/diagnostics/Log.hpp>

namespace umbra::platform
{
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

		diagnostics::Info("Platform initialized.");
		isInitialized_ = true;
		return true;
	}

	void Platform::Shutdown()
	{
		if (isShutdown_)
		{
			diagnostics::Error("Platform is already shut down.");
			return;
		}

		SDL_Quit();
		isShutdown_ = true;
		diagnostics::Info("Platform shut down.");
	}
}