#include <Log.hpp>

#include <spdlog/spdlog.h>
#if defined(__ANDROID__)
#include <spdlog/sinks/android_sink.h>
#endif
#include <cstdlib>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace
{
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
#if defined(__ANDROID__)
		// Phones have no console: messages go to logcat under the tag "Umbra".
		spdlog::set_default_logger(spdlog::android_logger_mt("umbra", "Umbra"));
#endif
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
			Critical(detail::FormatFailure("Fatal", "Fatal error", message, location));
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
		std::string FormatFailure(std::string_view category, std::string_view expression, std::string_view message, std::source_location location)
		{
			std::string result;
			result.reserve(category.size() + expression.size() + message.size() + 128);
			result.append("[").append(category).append("] Expression: ").append(expression);
			result.append(". Message: ").append(message);
			result.append(". Location: ").append(location.file_name());
			result.append(":").append(std::to_string(location.line()));
			result.append(" in function ").append(location.function_name());
			return result;
		}

		[[noreturn]] void AssertionFailed(std::string_view expression, std::string_view message, std::source_location location) noexcept
		{
			try
			{
				Critical(FormatFailure("Assertion", expression, message, location));
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
