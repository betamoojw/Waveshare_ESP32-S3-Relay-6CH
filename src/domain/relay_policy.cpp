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

[[nodiscard]] bool isAbnormal(const ResetCategory category) noexcept
{
	return category == ResetCategory::Brownout || category == ResetCategory::Watchdog || category == ResetCategory::Panic ||
		   category == ResetCategory::RepeatedBoot || category == ResetCategory::Unknown;
}

[[nodiscard]] RelayState restoredState(const RelayChannelConfiguration &configuration,
															const RelayRestoreContext &context,
															const std::size_t channel) noexcept
{
	if (!configuration.enabled || configuration.restorePolicy == RestorePolicy::AllOff)
	{
		return RelayState::Off;
	}
	if (configuration.restorePolicy == RestorePolicy::ConfiguredDefault)
	{
		return configuration.configuredDefault;
	}
	if (!context.persisted.valid || (isAbnormal(context.resetCategory) && !context.allowLastKnownAfterAbnormalReset))
	{
		return RelayState::Off;
	}
	return context.persisted.states[channel];
}
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
		const auto state = restoredState(channelConfigurations[channel], context, channel);
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