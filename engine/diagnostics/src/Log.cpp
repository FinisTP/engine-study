#include <umbra/diagnostics/Log.hpp>

#include <spdlog/spdlog.h>
#include <cstdlib>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace
{
	void ReportFailure(std::string_view category, std::string_view expression, std::string_view message, std::source_location location)
	{
		spdlog::critical("[{}] Expression: {}. Message: {}. Location: {}:{} in function {}",
					  category, expression, message, location.file_name(), location.line(), location.function_name());
	}

	void BreakIntoDebuggerIfAttached() noexcept
	{
#if defined(_WIN32)
		if (::IsDebuggerPresent())
		{
			__debugbreak();
		}
#endif
	}
}

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

	void Critical(std::string_view message)
	{
		spdlog::critical("{}", message);
	}

	void Flush() noexcept
	{
		try
		{
			if (auto logger = spdlog::default_logger())
			{
				logger->flush();
			}
		}
		catch (...)
		{
		}
	}

	void Shutdown()
	{
		spdlog::info("Diagnostics shutting down.");
		spdlog::shutdown();
	}

	[[noreturn]] void Fatal(std::string_view message, std::source_location location) noexcept
	{
		try
		{
			ReportFailure("Fatal", "Fatal error", message, location);
			Flush();
			BreakIntoDebuggerIfAttached();
		}
		catch (...)
		{
		}
		std::abort();
	}

	namespace detail
	{
		[[noreturn]] void AssertionFailed(std::string_view expression, std::string_view message, std::source_location location) noexcept
		{
			try
			{
				ReportFailure("Assertion", expression, message, location);
				Flush();
				BreakIntoDebuggerIfAttached();
			}
			catch (...)
			{
			}
			std::abort();
		}
	}
}