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
		// Hardware facts for reports (benchmark, P9).
		[[nodiscard]] static int CpuThreads();
		[[nodiscard]] static int SystemRamMb();
		// Most memory this process has used so far (peak working set / max resident size), 0 if unknown.
		[[nodiscard]] static int PeakMemoryMb();
		// Browser: copy changed files (saves) from memory to the browser's storage. Desktop: nothing.
		static void PersistFiles();
		// Waits without busy looping; in the browser this hands control back to the page meanwhile.
		static void Idle(int milliseconds);
	};
}
