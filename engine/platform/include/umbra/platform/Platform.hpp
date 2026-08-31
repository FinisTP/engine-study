#pragma once

#include <string_view>

namespace umbra::platform
{
	class Platform
	{
	private:
		bool isInitialized_ = false;
		bool isShutdown_ = false;

	public:
		bool Initialize();
		void Shutdown();
	};
}