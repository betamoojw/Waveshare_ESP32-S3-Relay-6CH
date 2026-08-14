#pragma once

#include "configuration.h"
#include "relay_types.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace switch_actuator::domain
{
enum class ResetCategory : std::uint8_t
{
	PowerOn,
	ControlledRestart,
	Brownout,
	Watchdog,
	Panic,
	RepeatedBoot,
	Unknown
};

struct PersistedRelayState final
{
	std::array<RelayState, relayChannelCount> states{};
	bool valid{false};
};

struct RelayRestoreContext final
{
	ResetCategory resetCategory{ResetCategory::Unknown};
	PersistedRelayState persisted{};
	bool allowLastKnownAfterAbnormalReset{false};
	std::uint32_t firstCorrelationId{0};
	std::uint32_t nowMs{0};
};

struct RelayRestorePlan final
{
	std::array<RelayCommand, relayChannelCount> commands{};
	std::size_t count{0};
};

enum class RelayRestorePlanResult : std::uint8_t
{
	Planned,
	InvalidConfiguration,
	InvalidPersistedState,
	CorrelationOverflow
};

[[nodiscard]] RelayRestorePlanResult makeRelayRestorePlan(
	const std::array<RelayChannelConfiguration, relayChannelCount> &channelConfigurations,
	const RelayRestoreContext &context,
	RelayRestorePlan &plan) noexcept;
}