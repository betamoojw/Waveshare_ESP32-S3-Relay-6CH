#include "service_mode_service.h"

namespace switch_actuator::app
{
ServiceModeResult ServiceModeService::enterFromPhysicalPresence(const std::uint32_t nowMs) noexcept
{
	snapshot_.state = ServiceModeState::Service;
	snapshot_.enteredAtMs = nowMs;
	snapshot_.expiresAtMs = nowMs + sessionDurationMs;
	++snapshot_.sequence;
	return ServiceModeResult::Applied;
}

ServiceModeResult ServiceModeService::exit() noexcept
{
	if (snapshot_.state == ServiceModeState::User)
	{
		return ServiceModeResult::NoChange;
	}
	snapshot_.state = ServiceModeState::User;
	snapshot_.enteredAtMs = 0;
	snapshot_.expiresAtMs = 0;
	++snapshot_.sequence;
	return ServiceModeResult::Applied;
}

bool ServiceModeService::update(const std::uint32_t nowMs) noexcept
{
	if (snapshot_.state != ServiceModeState::Service ||
		static_cast<std::int32_t>(nowMs - snapshot_.expiresAtMs) < 0)
	{
		return false;
	}
	static_cast<void>(exit());
	return true;
}

ServiceModeResult ServiceModeService::authorize(const ServiceModeOperation operation,
	const domain::DeploymentProfile deploymentProfile,
	const bool factoryConfigurationLocked) const noexcept
{
	if (snapshot_.state != ServiceModeState::Service)
	{
		return ServiceModeResult::NotAuthorized;
	}
	if (operation == ServiceModeOperation::FirmwareRecovery)
	{
		return ServiceModeResult::Unsupported;
	}
	if ((operation == ServiceModeOperation::ProvisionIdentity ||
		 operation == ServiceModeOperation::SetManufacturingData) &&
		deploymentProfile == domain::DeploymentProfile::Production && factoryConfigurationLocked)
	{
		return ServiceModeResult::FactoryConfigurationLocked;
	}
	return ServiceModeResult::Applied;
}

const ServiceModeSnapshot &ServiceModeService::snapshot() const noexcept
{
	return snapshot_;
}
}