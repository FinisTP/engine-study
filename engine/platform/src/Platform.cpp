#include <umbra/platform/Platform.hpp>
#include <string>
#include <SDL3/SDL.h>
#include <umbra/diagnostics/Log.hpp>

namespace umbra::platform
{
	bool Platform::Initialize()
	{
		if (!SDL_Init(SDL_INIT_VIDEO))
		{
			diagnostics::Error(std::string{"SDL initialization failed: "} + SDL_GetError());
			return false;
		}

		diagnostics::Info("Platform initialized.");
		return true;
	}

	void Platform::Shutdown()
	{
		SDL_Quit();
		diagnostics::Info("Platform shut down.");
	}
}