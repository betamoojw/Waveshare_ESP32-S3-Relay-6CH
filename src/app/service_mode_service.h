#pragma once

#include "../domain/deployment_profile.h"

#include <cstdint>

namespace switch_actuator::app
{
enum class ServiceModeState : std::uint8_t { User, Service };

enum class ServiceModeOperation : std::uint8_t
{
	ReadIdentity,
	ReadDiagnostics,
	SetManufacturingData,
	EraseUserConfiguration,
	ProvisionIdentity,
	FirmwareRecovery
};

enum class ServiceModeResult : std::uint8_t
{
	Applied,
	NoChange,
	NotAuthorized,
	FactoryConfigurationLocked,
	Unsupported
};

struct ServiceModeSnapshot final
{
	ServiceModeState state{ServiceModeState::User};
	std::uint32_t sequence{0};
	std::uint32_t enteredAtMs{0};
	std::uint32_t expiresAtMs{0};
};

class ServiceModeService final
{
public:
	[[nodiscard]] ServiceModeResult enterFromPhysicalPresence(std::uint32_t nowMs) noexcept;
	[[nodiscard]] ServiceModeResult exit() noexcept;
	[[nodiscard]] bool update(std::uint32_t nowMs) noexcept;
	[[nodiscard]] ServiceModeResult authorize(ServiceModeOperation operation,
		domain::DeploymentProfile deploymentProfile,
		bool factoryConfigurationLocked) const noexcept;
	[[nodiscard]] const ServiceModeSnapshot &snapshot() const noexcept;

	static constexpr std::uint32_t sessionDurationMs{5U * 60U * 1000U};

private:
	ServiceModeSnapshot snapshot_{};
};
}