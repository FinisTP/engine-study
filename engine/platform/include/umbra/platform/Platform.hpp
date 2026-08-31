#pragma once

#include <string_view>

namespace umbra::platform
{
	class Platform
	{
	public:
		bool Initialize();
		void Shutdown();
	};
}