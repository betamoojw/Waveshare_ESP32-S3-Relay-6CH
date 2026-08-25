#include "relay_policy.h"

#include <algorithm>
#include <limits>

namespace switch_actuator::domain
{
namespace
{
[[nodiscard]] bool isValid(const RelayState state) noexcept
{
	return state == RelayState::Off || state == RelayState::On;
}

[[nodiscard]] bool isValid(const RestorePolicy policy) noexcept
{
	return policy == RestorePolicy::AllOff || policy == RestorePolicy::LastKnown ||
		   policy == RestorePolicy::ConfiguredDefault;
}

}

RelayState resolveRelayBootState(const RelayBootState bootState,
	const RelayChannelConfiguration &configuration,
	const PersistedRelayState &persisted,
	const std::size_t channel) noexcept
{
	if (!configuration.enabled || channel >= persisted.states.size())
	{
		return RelayState::Off;
	}
	const auto configuredState = isValid(configuration.configuredDefault) ?
		configuration.configuredDefault : RelayState::Off;
	const auto lastState = persisted.valid && isValid(persisted.states[channel]) ?
		persisted.states[channel] : RelayState::Off;

	switch (bootState)
	{
	case RelayBootState::Off:
	case RelayBootState::SafeState:
		return RelayState::Off;
	case RelayBootState::On:
		return RelayState::On;
	case RelayBootState::ConfiguredState:
		return configuredState;
	case RelayBootState::LastState:
		return lastState;
	case RelayBootState::Restore:
		if (configuration.restorePolicy == RestorePolicy::ConfiguredDefault)
		{
			return configuredState;
		}
		if (configuration.restorePolicy == RestorePolicy::LastKnown)
		{
			return lastState;
		}
		return RelayState::Off;
	}
	return RelayState::Off;
}

RelaySafetyEvent relaySafetyEventForReset(const ResetCategory category) noexcept
{
	switch (category)
	{
	case ResetCategory::PowerOn:
		return RelaySafetyEvent::PowerOn;
	case ResetCategory::ControlledRestart:
		return RelaySafetyEvent::SoftwareReboot;
	case ResetCategory::Brownout:
		return RelaySafetyEvent::Brownout;
	case ResetCategory::Watchdog:
		return RelaySafetyEvent::WatchdogReset;
	case ResetCategory::Panic:
	case ResetCategory::RepeatedBoot:
	case ResetCategory::Unknown:
		return RelaySafetyEvent::WatchdogReset;
	}
	return RelaySafetyEvent::WatchdogReset;
}

RelayBootState relayBootStateFor(const RelaySafetyEvent event) noexcept
{
	switch (event)
	{
	case RelaySafetyEvent::PowerOn:
	case RelaySafetyEvent::SoftwareReboot:
		return RelayBootState::Restore;
	case RelaySafetyEvent::Brownout:
	case RelaySafetyEvent::WatchdogReset:
		return RelayBootState::SafeState;
	case RelaySafetyEvent::OtaReboot:
		return RelayBootState::ConfiguredState;
	case RelaySafetyEvent::FactoryReset:
		return RelayBootState::Off;
	case RelaySafetyEvent::ConfigurationUpdate:
	case RelaySafetyEvent::NetworkFailure:
		return RelayBootState::LastState;
	}
	return RelayBootState::SafeState;
}

RelayRestorePlanResult makeRelayRestorePlan(
	const std::array<RelayChannelConfiguration, relayChannelCount> &channelConfigurations,
	const RelayRestoreContext &context,
	RelayRestorePlan &plan) noexcept
{
	plan = {};
	if (std::any_of(channelConfigurations.begin(), channelConfigurations.end(), [](const auto &configuration) {
			return !isValid(configuration.restorePolicy) || !isValid(configuration.configuredDefault) ||
				   (!configuration.enabled && configuration.restorePolicy == RestorePolicy::ConfiguredDefault &&
					configuration.configuredDefault == RelayState::On);
		}))
	{
		return RelayRestorePlanResult::InvalidConfiguration;
	}
	if (context.persisted.valid && std::any_of(context.persisted.states.begin(), context.persisted.states.end(), [](const auto state) {
			return !isValid(state);
		}))
	{
		return RelayRestorePlanResult::InvalidPersistedState;
	}
	if (relayChannelCount - 1U > std::numeric_limits<std::uint32_t>::max() - context.firstCorrelationId)
	{
		return RelayRestorePlanResult::CorrelationOverflow;
	}

	for (std::uint8_t channel = 0; channel < relayChannelCount; ++channel)
	{
		const auto state = resolveRelayBootState(
			relayBootStateFor(context.event), channelConfigurations[channel], context.persisted, channel);
		plan.commands[channel] = RelayCommand{
			RelayChannelId{channel},
			state == RelayState::On ? RelayAction::SetOn : RelayAction::SetOff,
			CommandSource::Restore,
			context.firstCorrelationId + channel,
			context.nowMs,
		};
	}
	plan.count = plan.commands.size();
	return RelayRestorePlanResult::Planned;
}
}