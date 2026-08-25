#pragma once

#include "../../domain/error.h"

namespace switch_actuator::adapters::web
{
struct WebErrorRepresentation final
{
	int status;
	const char *code;
	const char *message;
};

[[nodiscard]] constexpr WebErrorRepresentation represent(const domain::ErrorCode error) noexcept
{
	switch (error)
	{
	case domain::ErrorCode::InvalidArgument:
		return {400, "invalid_argument", "The request contains an invalid argument."};
	case domain::ErrorCode::Unauthorized:
		return {401, "unauthorized", "Authentication is required."};
	case domain::ErrorCode::Forbidden:
		return {403, "forbidden", "The operation is not permitted."};
	case domain::ErrorCode::NotFound:
		return {404, "not_found", "The requested resource was not found."};
	case domain::ErrorCode::Busy:
		return {429, "busy", "The device is busy; retry later."};
	case domain::ErrorCode::StorageError:
		return {503, "storage_error", "Persistent storage is unavailable."};
	case domain::ErrorCode::ConfigurationError:
		return {422, "configuration_error", "The configuration is invalid or stale."};
	case domain::ErrorCode::HardwareError:
		return {503, "hardware_error", "A hardware operation failed."};
	case domain::ErrorCode::NetworkError:
		return {503, "network_error", "The network operation failed."};
	case domain::ErrorCode::ProtocolError:
		return {502, "protocol_error", "The protocol operation failed."};
	case domain::ErrorCode::Unsupported:
		return {501, "unsupported", "The operation is not supported."};
	case domain::ErrorCode::InternalError:
	default:
		return {500, "internal_error", "An internal error occurred."};
	}
}
}