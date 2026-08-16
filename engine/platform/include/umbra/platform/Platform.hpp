#pragma once

#include <string_view>

struct SDL_Window;

namespace umbra::platform
{
	bool Initialize();
	void Shutdown();

	class Window final
	{
	public:
		Window() = default;
		~Window();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		[[nodiscard]] bool Create(std::string_view title, int width, int height);

		[[nodiscard]] bool PollCloseRequested();

		void Destroy();

	private:
		SDL_Window* window_ = nullptr;
	};
}