#pragma once

#include "../../domain/deployment_profile.h"

#include <cstdarg>
#include <cstdint>

namespace switch_actuator::adapters::logging
{
enum class LogSeverity : std::uint8_t
{
	Fatal = 0,
	Error = 1,
	Warning = 2,
	Info = 3,
	Debug = 4
};

enum class LogLevelResult : std::uint8_t
{
	Applied,
	NotAllowed
};

class LoggerAdapter final
{
public:
	[[nodiscard]] static LoggerAdapter &instance() noexcept;

	void initialize(domain::DeploymentProfile profile) noexcept;
	[[nodiscard]] LogLevelResult setLevel(LogSeverity severity) noexcept;
	[[nodiscard]] LogSeverity level() const noexcept;
	[[nodiscard]] domain::DeploymentProfile profile() const noexcept;

	void debug(const char *tag, const char *format, ...) noexcept;
	void info(const char *tag, const char *format, ...) noexcept;
	void warning(const char *tag, const char *format, ...) noexcept;
	void error(const char *tag, const char *format, ...) noexcept;
	void fatal(const char *tag, const char *format, ...) noexcept;

private:
	LoggerAdapter() noexcept = default;
	void write(LogSeverity severity, const char *tag, const char *format, va_list arguments) noexcept;
	[[nodiscard]] bool enabled(LogSeverity severity) const noexcept;

	domain::DeploymentProfile profile_{domain::deploymentProfile};
	LogSeverity level_{LogSeverity::Info};
};
}

#if SWITCH_ACTUATOR_DEPLOYMENT_PROFILE == 2
#define LOG_DEBUG(tag, ...) ((void)0)
#else
#define LOG_DEBUG(tag, ...) ::switch_actuator::adapters::logging::LoggerAdapter::instance().debug(tag, __VA_ARGS__)
#endif
#define LOG_INFO(tag, ...) ::switch_actuator::adapters::logging::LoggerAdapter::instance().info(tag, __VA_ARGS__)
#define LOG_WARNING(tag, ...) ::switch_actuator::adapters::logging::LoggerAdapter::instance().warning(tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) ::switch_actuator::adapters::logging::LoggerAdapter::instance().error(tag, __VA_ARGS__)
#define LOG_FATAL(tag, ...) ::switch_actuator::adapters::logging::LoggerAdapter::instance().fatal(tag, __VA_ARGS__)