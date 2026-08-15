#pragma once

#include <cstddef>
#include <cstdint>

namespace switch_actuator::domain
{
enum class FaultCode : std::uint8_t
{
	InvalidConfiguration,
	IncompatibleBoard,
	RelayOutputFailure,
	CommandQueueOverflow,
	ModbusTransportError,
	ModbusProtocolError,
	KnxUnavailable,
	KnxBusOff,
	SettingsLoadFailure,
	SettingsSaveFailure,
	TaskWatchdogFailure,
	WatchdogReset,
	BrownoutReset,
	PanicReset,
	RepeatedBoot,
	ResourceExhaustion,
	FileSystemFailure,
	Count
};

enum class FaultSeverity : std::uint8_t
{
	Info,
	Warning,
	Critical
};

struct FaultRecord final
{
	FaultCode code{FaultCode::InvalidConfiguration};
	FaultSeverity severity{FaultSeverity::Info};
	std::uint32_t firstOccurredAtMs{0};
	std::uint32_t lastOccurredAtMs{0};
	std::uint32_t occurrenceCount{0};
	bool active{false};
};

inline constexpr std::size_t faultCodeCount{static_cast<std::size_t>(FaultCode::Count)};

[[nodiscard]] constexpr bool isValid(const FaultCode code) noexcept
{
	return static_cast<std::size_t>(code) < faultCodeCount;
}

[[nodiscard]] constexpr bool isValid(const FaultSeverity severity) noexcept
{
	return severity == FaultSeverity::Info || severity == FaultSeverity::Warning || severity == FaultSeverity::Critical;
}
}