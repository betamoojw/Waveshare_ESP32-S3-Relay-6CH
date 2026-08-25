#include "logger_adapter.h"

#include <Logger.h>

#include <cstdarg>
#include <cstdio>

namespace switch_actuator::adapters::logging
{
namespace
{
constexpr std::size_t maximumLogMessageLength{256};

[[nodiscard]] ::LogLevel backendLevel(const LogSeverity severity) noexcept
{
	switch (severity)
	{
	case LogSeverity::Debug: return ::LOG_DEBUG;
	case LogSeverity::Info: return ::LOG_INFO;
	case LogSeverity::Warning: return ::LOG_WARN;
	case LogSeverity::Error:
	case LogSeverity::Fatal:
	default: return ::LOG_ERROR;
	}
}
}

LoggerAdapter &LoggerAdapter::instance() noexcept
{
	static LoggerAdapter adapter{};
	return adapter;
}

void LoggerAdapter::initialize(const domain::DeploymentProfile profile) noexcept
{
#if SWITCH_ACTUATOR_DEPLOYMENT_PROFILE == 2
	static_cast<void>(profile);
	profile_ = domain::DeploymentProfile::Production;
#else
	profile_ = profile;
#endif
	level_ = profile_ == domain::DeploymentProfile::Production ? LogSeverity::Info : LogSeverity::Debug;
	auto *const logger = Logger::getInstance();
	logger->enableSerial(true);
	logger->enableFile(false);
	logger->setLogLevel(backendLevel(level_));
}

LogLevelResult LoggerAdapter::setLevel(const LogSeverity severity) noexcept
{
	if (profile_ == domain::DeploymentProfile::Production && severity == LogSeverity::Debug)
	{
		return LogLevelResult::NotAllowed;
	}
	level_ = severity;
	Logger::getInstance()->setLogLevel(backendLevel(severity));
	return LogLevelResult::Applied;
}

LogSeverity LoggerAdapter::level() const noexcept
{
	return level_;
}

domain::DeploymentProfile LoggerAdapter::profile() const noexcept
{
	return profile_;
}

void LoggerAdapter::debug(const char *const tag, const char *const format, ...) noexcept
{
	va_list arguments;
	va_start(arguments, format);
	write(LogSeverity::Debug, tag, format, arguments);
	va_end(arguments);
}

void LoggerAdapter::info(const char *const tag, const char *const format, ...) noexcept
{
	va_list arguments;
	va_start(arguments, format);
	write(LogSeverity::Info, tag, format, arguments);
	va_end(arguments);
}

void LoggerAdapter::warning(const char *const tag, const char *const format, ...) noexcept
{
	va_list arguments;
	va_start(arguments, format);
	write(LogSeverity::Warning, tag, format, arguments);
	va_end(arguments);
}

void LoggerAdapter::error(const char *const tag, const char *const format, ...) noexcept
{
	va_list arguments;
	va_start(arguments, format);
	write(LogSeverity::Error, tag, format, arguments);
	va_end(arguments);
}

void LoggerAdapter::fatal(const char *const tag, const char *const format, ...) noexcept
{
	va_list arguments;
	va_start(arguments, format);
	write(LogSeverity::Fatal, tag, format, arguments);
	va_end(arguments);
}

void LoggerAdapter::write(const LogSeverity severity,
	const char *const tag,
	const char *const format,
	va_list arguments) noexcept
{
	if (!enabled(severity) || tag == nullptr || format == nullptr) return;
	char message[maximumLogMessageLength]{};
	static_cast<void>(std::vsnprintf(message, sizeof(message), format, arguments));
	auto *const logger = Logger::getInstance();
	switch (severity)
	{
	case LogSeverity::Debug: logger->debug(tag, "%s", message); break;
	case LogSeverity::Info: logger->info(tag, "%s", message); break;
	case LogSeverity::Warning: logger->warn(tag, "%s", message); break;
	case LogSeverity::Error: logger->error(tag, "%s", message); break;
	case LogSeverity::Fatal: logger->error(tag, "FATAL: %s", message); break;
	}
}

bool LoggerAdapter::enabled(const LogSeverity severity) const noexcept
{
	return static_cast<std::uint8_t>(severity) <= static_cast<std::uint8_t>(level_);
}
}