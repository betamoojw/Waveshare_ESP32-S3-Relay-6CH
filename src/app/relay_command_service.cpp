#include "relay_command_service.h"

#include <limits>

namespace switch_actuator::app
{
RelayCommandService::RelayCommandService(const hal::IRelay relay,
															 const RelayEventSink eventSink,
															 CommandArbiter &commandArbiter) noexcept
	: relay_{relay}, eventSink_{eventSink}, commandArbiter_{commandArbiter}
{
}

RelayServiceInitializeResult RelayCommandService::initialize(const std::uint32_t nowMs) noexcept
{
	initialized_ = false;
	if (!relay_.isValid() || !eventSink_.isValid())
	{
		return RelayServiceInitializeResult::InvalidPort;
	}

	bool allOutputsSafe = true;
	commandArbiter_.reset();
	for (std::uint8_t channel = 0; channel < domain::relayChannelCount; ++channel)
	{
		auto &snapshot = snapshots_[channel];
		snapshot = {};
		snapshot.lastTransitionAtMs = nowMs;
		if (relay_.apply(domain::RelayChannelId{channel}, domain::RelayState::Off) != hal::RelayHalResult::Applied)
		{
			snapshot.fault = domain::RelayFault::OutputFailure;
			snapshot.lockedOut = true;
			allOutputsSafe = false;
		}
	}

	if (!allOutputsSafe)
	{
		return RelayServiceInitializeResult::OutputFailure;
	}

	initialized_ = true;
	return RelayServiceInitializeResult::Initialized;
}

RelayCommandResult RelayCommandService::execute(const domain::RelayCommand &command) noexcept
{
	const auto validationResult = validate(command);
	if (validationResult != RelayCommandReason::None)
	{
		return reject(command, validationResult);
	}

	auto &snapshot = snapshots_[command.channel.value];
	const auto desiredState = requestedState(snapshot.appliedState, command.action);
	snapshot.requestedState = desiredState;
	snapshot.lastCommandSource = command.source;
	if (desiredState == snapshot.appliedState)
	{
		return {RelayCommandStatus::Accepted, RelayCommandReason::None, command.correlationId, snapshot.appliedState};
	}

	if (relay_.apply(command.channel, desiredState) != hal::RelayHalResult::Applied)
	{
		snapshot.fault = domain::RelayFault::OutputFailure;
		snapshot.lockedOut = true;
		static_cast<void>(forceSafeOff(command.channel, snapshot, command.correlationId, command.receivedAtMs));
		return {RelayCommandStatus::Rejected, RelayCommandReason::OutputFailure, command.correlationId, snapshot.appliedState};
	}

	snapshot.appliedState = desiredState;
	snapshot.lastTransitionAtMs = command.receivedAtMs;
	incrementSequence(snapshot.transitionSequence);
	const auto eventReason = publishStateChanged(command, snapshot);
	return {RelayCommandStatus::Accepted, eventReason, command.correlationId, snapshot.appliedState};
}

RelayCommandBatchResult RelayCommandService::executeBatch(const domain::RelayCommand *const commands,
															  const std::size_t commandCount) noexcept
{
	if (!initialized_)
	{
		return {RelayCommandStatus::Rejected, RelayCommandReason::NotInitialized, commandCount, 0};
	}
	if (commands == nullptr || commandCount == 0)
	{
		return {RelayCommandStatus::Rejected, RelayCommandReason::EmptyBatch, commandCount, 0};
	}
	if (commandCount > snapshots_.size())
	{
		return {RelayCommandStatus::Rejected, RelayCommandReason::InvalidChannel, commandCount, 0};
	}

	std::array<bool, domain::relayChannelCount> channelSeen{};
	std::array<bool, domain::relayChannelCount> changed{};
	std::array<bool, domain::relayChannelCount> outputApplied{};
	std::array<domain::RelayState, domain::relayChannelCount> previousStates{};
	std::array<domain::RelayState, domain::relayChannelCount> desiredStates{};
	for (std::size_t index = 0; index < commandCount; ++index)
	{
		const auto validationResult = validate(commands[index]);
		if (validationResult != RelayCommandReason::None)
		{
			return {RelayCommandStatus::Rejected, validationResult, commandCount, 0};
		}

		const auto channel = commands[index].channel.value;
		if (channelSeen[channel])
		{
			return {RelayCommandStatus::Rejected, RelayCommandReason::DuplicateChannel, commandCount, 0};
		}
		channelSeen[channel] = true;
		previousStates[index] = snapshots_[channel].appliedState;
		desiredStates[index] = requestedState(previousStates[index], commands[index].action);
		changed[index] = desiredStates[index] != previousStates[index];
	}

	for (std::size_t index = 0; index < commandCount; ++index)
	{
		if (!changed[index])
		{
			continue;
		}
		if (relay_.apply(commands[index].channel, desiredStates[index]) == hal::RelayHalResult::Applied)
		{
			outputApplied[index] = true;
			continue;
		}

		auto &failedSnapshot = snapshots_[commands[index].channel.value];
		failedSnapshot.fault = domain::RelayFault::OutputFailure;
		failedSnapshot.lockedOut = true;
		static_cast<void>(forceSafeOff(commands[index].channel,
			failedSnapshot,
			commands[index].correlationId,
			commands[index].receivedAtMs));
		for (std::size_t rollbackIndex = index; rollbackIndex-- > 0;)
		{
			if (!outputApplied[rollbackIndex])
			{
				continue;
			}
			if (relay_.apply(commands[rollbackIndex].channel, previousStates[rollbackIndex]) !=
				hal::RelayHalResult::Applied)
			{
				auto &rollbackSnapshot = snapshots_[commands[rollbackIndex].channel.value];
				rollbackSnapshot.fault = domain::RelayFault::OutputFailure;
				rollbackSnapshot.lockedOut = true;
				static_cast<void>(forceSafeOff(commands[rollbackIndex].channel,
					rollbackSnapshot,
					commands[rollbackIndex].correlationId,
					commands[rollbackIndex].receivedAtMs));
			}
		}
		return {RelayCommandStatus::Rejected, RelayCommandReason::OutputFailure, commandCount, 0};
	}

	std::size_t changedCount{0};
	auto resultReason = RelayCommandReason::None;
	for (std::size_t index = 0; index < commandCount; ++index)
	{
		auto &snapshot = snapshots_[commands[index].channel.value];
		snapshot.requestedState = desiredStates[index];
		snapshot.lastCommandSource = commands[index].source;
		if (!changed[index])
		{
			continue;
		}
		snapshot.appliedState = desiredStates[index];
		snapshot.lastTransitionAtMs = commands[index].receivedAtMs;
		incrementSequence(snapshot.transitionSequence);
		++changedCount;
		if (publishStateChanged(commands[index], snapshot) == RelayCommandReason::EventRejected)
		{
			resultReason = RelayCommandReason::EventRejected;
		}
	}
	return {RelayCommandStatus::Accepted, resultReason, commandCount, changedCount};
}

RelayCommandResult RelayCommandService::setSafetyLockout(const domain::RelayChannelId channel,
															 const bool active,
															 const std::uint32_t correlationId,
															 const std::uint32_t nowMs) noexcept
{
	const domain::RelayCommand safetyCommand{channel, domain::RelayAction::SetOff, domain::CommandSource::Safety, correlationId, nowMs};
	if (!initialized_)
	{
		return reject(safetyCommand, RelayCommandReason::NotInitialized);
	}
	if (channel.value >= snapshots_.size())
	{
		return reject(safetyCommand, RelayCommandReason::InvalidChannel);
	}

	auto &snapshot = snapshots_[channel.value];
	if (!active)
	{
		static_cast<void>(commandArbiter_.setSafetyLockout(channel, false));
		snapshot.lockedOut = snapshot.fault != domain::RelayFault::None;
		return {RelayCommandStatus::Accepted, RelayCommandReason::None, correlationId, snapshot.appliedState};
	}

	static_cast<void>(commandArbiter_.setSafetyLockout(channel, true));
	snapshot.lockedOut = true;
	return execute(safetyCommand);
}

const domain::RelaySnapshot *RelayCommandService::snapshot(const domain::RelayChannelId channel) const noexcept
{
	return channel.value < snapshots_.size() ? &snapshots_[channel.value] : nullptr;
}

const std::array<domain::RelaySnapshot, domain::relayChannelCount> &RelayCommandService::snapshots() const noexcept
{
	return snapshots_;
}

bool RelayCommandService::isInitialized() const noexcept
{
	return initialized_;
}

bool RelayCommandService::isValid(const domain::RelayAction action) noexcept
{
	switch (action)
	{
	case domain::RelayAction::SetOff:
	case domain::RelayAction::SetOn:
	case domain::RelayAction::Toggle:
		return true;
	default:
		return false;
	}
}

bool RelayCommandService::isValid(const domain::CommandSource source) noexcept
{
	switch (source)
	{
	case domain::CommandSource::Safety:
	case domain::CommandSource::Button:
	case domain::CommandSource::Knx:
	case domain::CommandSource::Modbus:
	case domain::CommandSource::Web:
	case domain::CommandSource::Cli:
	case domain::CommandSource::Restore:
		return true;
	default:
		return false;
	}
}

RelayCommandReason RelayCommandService::validate(const domain::RelayCommand &command) const noexcept
{
	if (!initialized_)
	{
		return RelayCommandReason::NotInitialized;
	}
	if (command.channel.value >= snapshots_.size())
	{
		return RelayCommandReason::InvalidChannel;
	}
	if (!isValid(command.action))
	{
		return RelayCommandReason::InvalidAction;
	}
	if (!isValid(command.source))
	{
		return RelayCommandReason::InvalidSource;
	}
	if (commandArbiter_.evaluate(command) == ArbitrationDecision::SafetyLockout)
	{
		return RelayCommandReason::SafetyLockout;
	}

	const auto &snapshot = snapshots_[command.channel.value];
	if (snapshot.fault != domain::RelayFault::None && command.source != domain::CommandSource::Safety &&
		command.action != domain::RelayAction::SetOff)
	{
		return RelayCommandReason::SafetyLockout;
	}
	return RelayCommandReason::None;
}

domain::RelayState RelayCommandService::requestedState(const domain::RelayState current, const domain::RelayAction action) noexcept
{
	switch (action)
	{
	case domain::RelayAction::SetOn:
		return domain::RelayState::On;
	case domain::RelayAction::Toggle:
		return current == domain::RelayState::On ? domain::RelayState::Off : domain::RelayState::On;
	case domain::RelayAction::SetOff:
	default:
		return domain::RelayState::Off;
	}
}

void RelayCommandService::incrementSequence(std::uint32_t &sequence) noexcept
{
	if (sequence != std::numeric_limits<std::uint32_t>::max())
	{
		++sequence;
	}
}

RelayCommandResult RelayCommandService::reject(const domain::RelayCommand &command, const RelayCommandReason reason) const noexcept
{
	const auto finalState = command.channel.value < snapshots_.size() ? snapshots_[command.channel.value].appliedState : domain::RelayState::Off;
	return {RelayCommandStatus::Rejected, reason, command.correlationId, finalState};
}

RelayCommandReason RelayCommandService::publishStateChanged(const domain::RelayCommand &command,
																	 const domain::RelaySnapshot &snapshot) const noexcept
{
	const domain::RelayStateChanged event{
		command.channel,
		snapshot.appliedState,
		command.source,
		command.correlationId,
		snapshot.transitionSequence,
		snapshot.lastTransitionAtMs,
	};
	return eventSink_.publish(event) ? RelayCommandReason::None : RelayCommandReason::EventRejected;
}

bool RelayCommandService::forceSafeOff(const domain::RelayChannelId channel,
									   domain::RelaySnapshot &snapshot,
									   const std::uint32_t correlationId,
									   const std::uint32_t nowMs) noexcept
{
	if (relay_.apply(channel, domain::RelayState::Off) != hal::RelayHalResult::Applied)
	{
		return false;
	}

	snapshot.requestedState = domain::RelayState::Off;
	if (snapshot.appliedState == domain::RelayState::Off)
	{
		return true;
	}

	snapshot.appliedState = domain::RelayState::Off;
	snapshot.lastCommandSource = domain::CommandSource::Safety;
	snapshot.lastTransitionAtMs = nowMs;
	incrementSequence(snapshot.transitionSequence);
	const domain::RelayCommand safetyCommand{channel, domain::RelayAction::SetOff, domain::CommandSource::Safety, correlationId, nowMs};
	static_cast<void>(publishStateChanged(safetyCommand, snapshot));
	return true;
}
}