#include "switching_policy_service.h"

namespace switch_actuator::app
{
SwitchingPolicyService::SwitchingPolicyService(RelayCommandQueue &commandQueue,
														 const CommandArbiter &commandArbiter) noexcept
	: commandQueue_{commandQueue}, commandArbiter_{commandArbiter}
{
}

SwitchingPolicyResult SwitchingPolicyService::requestChannel(const domain::RelayChannelId channel,
																								const domain::RelayAction action,
																								const domain::CommandSource source,
																								const std::uint32_t correlationId,
																								const std::uint32_t receivedAtMs) noexcept
{
	const domain::RelayCommand command{channel, action, source, correlationId, receivedAtMs};
	const auto validation = validate(command);
	if (validation != SwitchingPolicyResult::Accepted)
	{
		return validation;
	}

	RelayCommandBatch batch{};
	batch.commands[0] = command;
	batch.count = 1;
	return enqueue(batch);
}

SwitchingPolicyResult SwitchingPolicyService::requestGroup(
	const std::array<bool, domain::relayChannelCount> &participants,
	const domain::RelayAction action,
	const domain::CommandSource source,
	const std::uint32_t correlationId,
	const std::uint32_t receivedAtMs) noexcept
{
	if (action != domain::RelayAction::SetOff && action != domain::RelayAction::SetOn)
	{
		return SwitchingPolicyResult::InvalidAction;
	}
	if (!isValid(source))
	{
		return SwitchingPolicyResult::InvalidSource;
	}

	std::array<domain::RelayState, domain::relayChannelCount> targetStates{};
	targetStates.fill(action == domain::RelayAction::SetOn ? domain::RelayState::On : domain::RelayState::Off);
	return requestStates(participants, targetStates, source, correlationId, receivedAtMs);
}

SwitchingPolicyResult SwitchingPolicyService::requestStates(
	const std::array<bool, domain::relayChannelCount> &participants,
	const std::array<domain::RelayState, domain::relayChannelCount> &targetStates,
	const domain::CommandSource source,
	const std::uint32_t correlationId,
	const std::uint32_t receivedAtMs) noexcept
{
	if (!isValid(source))
	{
		return SwitchingPolicyResult::InvalidSource;
	}

	RelayCommandBatch batch{};
	for (std::size_t channel = 0; channel < participants.size(); ++channel)
	{
		if (!participants[channel])
		{
			continue;
		}
		const domain::RelayCommand command{{static_cast<std::uint8_t>(channel)},
			targetStates[channel] == domain::RelayState::On ? domain::RelayAction::SetOn : domain::RelayAction::SetOff,
			source,
			correlationId,
			receivedAtMs};
		const auto validation = validate(command);
		if (validation != SwitchingPolicyResult::Accepted)
		{
			return validation;
		}
		batch.commands[batch.count++] = command;
	}
	return batch.count == 0 ? SwitchingPolicyResult::NoParticipants : enqueue(batch);
}

SwitchingPolicyResult SwitchingPolicyService::requestBatch(const RelayCommandBatch &batch) noexcept
{
	if (batch.count == 0 || batch.count > batch.commands.size())
	{
		return SwitchingPolicyResult::NoParticipants;
	}
	std::array<bool, domain::relayChannelCount> channelSeen{};
	for (std::size_t index = 0; index < batch.count; ++index)
	{
		const auto validation = validate(batch.commands[index]);
		if (validation != SwitchingPolicyResult::Accepted)
		{
			return validation;
		}
		const auto channel = batch.commands[index].channel.value;
		if (channelSeen[channel])
		{
			return SwitchingPolicyResult::InvalidChannel;
		}
		channelSeen[channel] = true;
	}
	return enqueue(batch);
}

bool SwitchingPolicyService::isValid(const domain::RelayAction action) noexcept
{
	return action == domain::RelayAction::SetOff || action == domain::RelayAction::SetOn ||
		action == domain::RelayAction::Toggle;
}

bool SwitchingPolicyService::isValid(const domain::CommandSource source) noexcept
{
	return source == domain::CommandSource::Safety || source == domain::CommandSource::Button ||
		source == domain::CommandSource::Knx || source == domain::CommandSource::Modbus ||
		source == domain::CommandSource::Web || source == domain::CommandSource::Cli ||
		source == domain::CommandSource::Restore;
}

SwitchingPolicyResult SwitchingPolicyService::validate(const domain::RelayCommand &command) const noexcept
{
	if (command.channel.value >= domain::relayChannelCount)
	{
		return SwitchingPolicyResult::InvalidChannel;
	}
	if (!isValid(command.action))
	{
		return SwitchingPolicyResult::InvalidAction;
	}
	if (!isValid(command.source))
	{
		return SwitchingPolicyResult::InvalidSource;
	}
	return commandArbiter_.evaluate(command) == ArbitrationDecision::Allowed ? SwitchingPolicyResult::Accepted :
		SwitchingPolicyResult::SafetyLockout;
}

SwitchingPolicyResult SwitchingPolicyService::enqueue(const RelayCommandBatch &batch) noexcept
{
	return commandQueue_.enqueue(batch) == RelayCommandEnqueueResult::Accepted ? SwitchingPolicyResult::Accepted :
		SwitchingPolicyResult::QueueFull;
}
}