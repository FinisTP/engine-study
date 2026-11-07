#pragma once

#include <filesystem>

namespace umbra::platform
{
	class Platform
	{
	private:
		bool isInitialized_ = false;

	public:
		Platform() = default;
		~Platform() noexcept;

		Platform(const Platform &) = delete;
		Platform &operator=(const Platform &) = delete;
		Platform(Platform &&) = delete;
		Platform &operator=(Platform &&) = delete;

		bool Initialize();
		void Shutdown() noexcept;

		// A folder the user may write to (saves, settings), created if missing; empty on failure.
		[[nodiscard]] static std::filesystem::path PreferencesFolder();
		// The folder the program file is in (empty if unknown).
		[[nodiscard]] static std::filesystem::path ProgramFolder();
	};
}
