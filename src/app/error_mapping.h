#pragma once

#include "configuration_service.h"
#include "relay_command_service.h"
#include "switching_policy_service.h"
#include "web_command_tracker.h"
#include "web_security_service.h"
#include "wifi_management_service.h"
#include "../domain/error.h"

#include <optional>

namespace switch_actuator::app
{
[[nodiscard]] constexpr std::optional<domain::ErrorCode> errorCode(const SwitchingPolicyResult result) noexcept
{
	switch (result)
	{
	case SwitchingPolicyResult::Accepted:
	case SwitchingPolicyResult::NoParticipants:
		return std::nullopt;
	case SwitchingPolicyResult::InvalidChannel:
	case SwitchingPolicyResult::InvalidAction:
	case SwitchingPolicyResult::InvalidSource:
		return domain::ErrorCode::InvalidArgument;
	case SwitchingPolicyResult::SafetyLockout:
		return domain::ErrorCode::Forbidden;
	case SwitchingPolicyResult::QueueFull:
		return domain::ErrorCode::Busy;
	default:
		return domain::ErrorCode::InternalError;
	}
}

[[nodiscard]] constexpr std::optional<domain::ErrorCode> errorCode(const RelayCommandReason reason) noexcept
{
	switch (reason)
	{
	case RelayCommandReason::None:
		return std::nullopt;
	case RelayCommandReason::EmptyBatch:
	case RelayCommandReason::InvalidChannel:
	case RelayCommandReason::InvalidAction:
	case RelayCommandReason::InvalidSource:
	case RelayCommandReason::DuplicateChannel:
		return domain::ErrorCode::InvalidArgument;
	case RelayCommandReason::SafetyLockout:
		return domain::ErrorCode::Forbidden;
	case RelayCommandReason::OutputFailure:
		return domain::ErrorCode::HardwareError;
	case RelayCommandReason::NotInitialized:
	case RelayCommandReason::EventRejected:
	default:
		return domain::ErrorCode::InternalError;
	}
}

[[nodiscard]] constexpr std::optional<domain::ErrorCode> errorCode(const WebSessionResult result) noexcept
{
	switch (result)
	{
	case WebSessionResult::Applied:
		return std::nullopt;
	case WebSessionResult::InvalidCredentials:
	case WebSessionResult::Unauthorized:
		return domain::ErrorCode::Unauthorized;
	case WebSessionResult::RateLimited:
	case WebSessionResult::CapacityFull:
		return domain::ErrorCode::Busy;
	case WebSessionResult::InvalidRequest:
		return domain::ErrorCode::InvalidArgument;
	case WebSessionResult::CryptoFailure:
	default:
		return domain::ErrorCode::InternalError;
	}
}

[[nodiscard]] constexpr std::optional<domain::ErrorCode> errorCode(const WebUserManagementResult result) noexcept
{
	switch (result)
	{
	case WebUserManagementResult::Applied:
		return std::nullopt;
	case WebUserManagementResult::Invalid:
	case WebUserManagementResult::DuplicateUsername:
		return domain::ErrorCode::InvalidArgument;
	case WebUserManagementResult::LastAdministrator:
		return domain::ErrorCode::Forbidden;
	case WebUserManagementResult::CapacityFull:
		return domain::ErrorCode::Busy;
	case WebUserManagementResult::PersistenceFailure:
		return domain::ErrorCode::StorageError;
	case WebUserManagementResult::CryptoFailure:
	default:
		return domain::ErrorCode::InternalError;
	}
}

[[nodiscard]] constexpr std::optional<domain::ErrorCode> errorCode(const ports::WebAuthorizationResult result) noexcept
{
	switch (result)
	{
	case ports::WebAuthorizationResult::Authorized:
		return std::nullopt;
	case ports::WebAuthorizationResult::Unauthenticated:
		return domain::ErrorCode::Unauthorized;
	case ports::WebAuthorizationResult::Forbidden:
		return domain::ErrorCode::Forbidden;
	case ports::WebAuthorizationResult::RateLimited:
		return domain::ErrorCode::Busy;
	default:
		return domain::ErrorCode::InternalError;
	}
}

[[nodiscard]] constexpr std::optional<domain::ErrorCode> errorCode(const WifiManagementResult result) noexcept
{
	switch (result)
	{
	case WifiManagementResult::Applied:
		return std::nullopt;
	case WifiManagementResult::InvalidIndex:
		return domain::ErrorCode::NotFound;
	case WifiManagementResult::GenerationConflict:
	case WifiManagementResult::InvalidConfiguration:
		return domain::ErrorCode::ConfigurationError;
	case WifiManagementResult::PersistenceFailure:
		return domain::ErrorCode::StorageError;
	default:
		return domain::ErrorCode::InternalError;
	}
}

[[nodiscard]] constexpr std::optional<domain::ErrorCode> errorCode(const WebCommandBeginResult result) noexcept
{
	switch (result)
	{
	case WebCommandBeginResult::Accepted:
	case WebCommandBeginResult::Duplicate:
		return std::nullopt;
	case WebCommandBeginResult::IdempotencyMismatch:
		return domain::ErrorCode::ConfigurationError;
	case WebCommandBeginResult::CapacityFull:
		return domain::ErrorCode::Busy;
	case WebCommandBeginResult::InvalidKey:
		return domain::ErrorCode::InvalidArgument;
	default:
		return domain::ErrorCode::InternalError;
	}
}

[[nodiscard]] constexpr std::optional<domain::ErrorCode> errorCode(const ConfigurationStageResult result) noexcept
{
	return result == ConfigurationStageResult::Staged ? std::nullopt :
		std::optional<domain::ErrorCode>{domain::ErrorCode::ConfigurationError};
}

[[nodiscard]] constexpr std::optional<domain::ErrorCode> errorCode(const ConfigurationCommitResult result) noexcept
{
	switch (result)
	{
	case ConfigurationCommitResult::Committed:
	case ConfigurationCommitResult::CommittedRestartRequired:
		return std::nullopt;
	case ConfigurationCommitResult::NothingStaged:
		return domain::ErrorCode::ConfigurationError;
	case ConfigurationCommitResult::PersistenceFailure:
	default:
		return domain::ErrorCode::StorageError;
	}
}
}