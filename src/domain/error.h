#pragma once

#include <cstdint>

namespace switch_actuator::domain
{
enum class ErrorCode : std::uint8_t
{
	InvalidArgument,
	Unauthorized,
	Forbidden,
	NotFound,
	Busy,
	StorageError,
	ConfigurationError,
	HardwareError,
	NetworkError,
	ProtocolError,
	Unsupported,
	InternalError
};
}