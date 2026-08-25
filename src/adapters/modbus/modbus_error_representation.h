#pragma once

#include "../../domain/error.h"
#include "nanomodbus/nanomodbus.h"

namespace switch_actuator::adapters::modbus
{
[[nodiscard]] constexpr nmbs_error represent(const domain::ErrorCode error) noexcept
{
	switch (error)
	{
	case domain::ErrorCode::NotFound:
		return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
	case domain::ErrorCode::InvalidArgument:
	case domain::ErrorCode::ConfigurationError:
		return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;
	case domain::ErrorCode::Unsupported:
		return NMBS_EXCEPTION_ILLEGAL_FUNCTION;
	case domain::ErrorCode::Unauthorized:
	case domain::ErrorCode::Forbidden:
	case domain::ErrorCode::Busy:
	case domain::ErrorCode::StorageError:
	case domain::ErrorCode::HardwareError:
	case domain::ErrorCode::NetworkError:
	case domain::ErrorCode::ProtocolError:
	case domain::ErrorCode::InternalError:
	default:
		return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;
	}
}
}