#pragma once

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
	};
}
