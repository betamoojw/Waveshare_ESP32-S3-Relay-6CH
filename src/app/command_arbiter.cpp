#include "command_arbiter.h"

namespace switch_actuator::app
{
ArbitrationDecision CommandArbiter::evaluate(const domain::RelayCommand &command) const noexcept
{
	if (command.channel.value >= safetyLockouts_.size())
	{
		return ArbitrationDecision::InvalidChannel;
	}

	if (!safetyLockouts_[command.channel.value])
	{
		return ArbitrationDecision::Allowed;
	}

	const auto isSafetyCommand = command.source == domain::CommandSource::Safety;
	const auto requestsSafeState = command.action == domain::RelayAction::SetOff;
	return isSafetyCommand || requestsSafeState ? ArbitrationDecision::Allowed : ArbitrationDecision::SafetyLockout;
}

bool CommandArbiter::setSafetyLockout(const domain::RelayChannelId channel, const bool active) noexcept
{
	if (channel.value >= safetyLockouts_.size())
	{
		return false;
	}

	safetyLockouts_[channel.value] = active;
	return true;
}

bool CommandArbiter::isSafetyLocked(const domain::RelayChannelId channel) const noexcept
{
	return channel.value < safetyLockouts_.size() && safetyLockouts_[channel.value];
}

void CommandArbiter::reset() noexcept
{
	safetyLockouts_.fill(false);
}
}