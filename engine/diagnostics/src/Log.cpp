#include <umbra/diagnostics/Log.hpp>

#include <spdlog/spdlog.h>

namespace umbra::diagnostics
{
	void Initialize()
	{
		spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
		spdlog::info("Diagnostics initialized.");
	}

	void Info(std::string_view message)
	{
		spdlog::info("{}", message);
	}

	void Error(std::string_view message)
	{
		spdlog::error("{}", message);
	}

	void Shutdown()
	{
		spdlog::info("Diagnostics shutting down.");
		spdlog::shutdown();
	}
}