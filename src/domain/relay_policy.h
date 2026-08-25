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

enum class RelayBootState : std::uint8_t
{
	Off,
	On,
	Restore,
	LastState,
	SafeState,
	ConfiguredState
};

enum class RelaySafetyEvent : std::uint8_t
{
	PowerOn,
	Brownout,
	WatchdogReset,
	SoftwareReboot,
	OtaReboot,
	FactoryReset,
	ConfigurationUpdate,
	NetworkFailure
};

struct PersistedRelayState final
{
	std::array<RelayState, relayChannelCount> states{};
	bool valid{false};
};

struct RelayRestoreContext final
{
	RelaySafetyEvent event{RelaySafetyEvent::WatchdogReset};
	PersistedRelayState persisted{};
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

[[nodiscard]] RelaySafetyEvent relaySafetyEventForReset(ResetCategory category) noexcept;
[[nodiscard]] RelayBootState relayBootStateFor(RelaySafetyEvent event) noexcept;
[[nodiscard]] RelayState resolveRelayBootState(RelayBootState bootState,
	const RelayChannelConfiguration &configuration,
	const PersistedRelayState &persisted,
	std::size_t channel) noexcept;

[[nodiscard]] RelayRestorePlanResult makeRelayRestorePlan(
	const std::array<RelayChannelConfiguration, relayChannelCount> &channelConfigurations,
	const RelayRestoreContext &context,
	RelayRestorePlan &plan) noexcept;
}