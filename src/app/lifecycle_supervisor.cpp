#include "lifecycle_supervisor.h"

namespace switch_actuator::app
{
LifecycleSupervisor::LifecycleSupervisor(const LifecycleEventSink eventSink) noexcept
	: eventSink_{eventSink}
{
}

LifecycleResult LifecycleSupervisor::initialize(const std::uint32_t nowMs) noexcept
{
	if (!eventSink_.isValid())
	{
		return LifecycleResult::InvalidEventSink;
	}

	state_ = LifecycleState::Booting;
	lastReason_ = LifecycleReason::Startup;
	transitionSequence_ = 0;
	lastTransitionAtMs_ = nowMs;
	initialized_ = true;
	return LifecycleResult::Applied;
}

LifecycleResult LifecycleSupervisor::beginConfiguration(const std::uint32_t nowMs) noexcept
{
	return transitionTo(LifecycleState::Configuring, LifecycleReason::ConfigurationStarted, nowMs);
}

LifecycleResult LifecycleSupervisor::enterOperational(const std::uint32_t nowMs) noexcept
{
	const auto reason = state_ == LifecycleState::Degraded ? LifecycleReason::AdapterRecovered : LifecycleReason::ConfigurationValid;
	return transitionTo(LifecycleState::Operational, reason, nowMs);
}

LifecycleResult LifecycleSupervisor::enterDegraded(const LifecycleReason reason, const std::uint32_t nowMs) noexcept
{
	if (reason != LifecycleReason::AdapterUnavailable && reason != LifecycleReason::ConfigurationInvalid)
	{
		return LifecycleResult::InvalidTransition;
	}

	return transitionTo(LifecycleState::Degraded, reason, nowMs);
}

LifecycleResult LifecycleSupervisor::enterFault(const LifecycleReason reason, const std::uint32_t nowMs) noexcept
{
	if (reason != LifecycleReason::CriticalFault && reason != LifecycleReason::ConfigurationInvalid)
	{
		return LifecycleResult::InvalidTransition;
	}

	return transitionTo(LifecycleState::Fault, reason, nowMs);
}

LifecycleResult LifecycleSupervisor::requestRestart(const std::uint32_t nowMs) noexcept
{
	return transitionTo(LifecycleState::Restarting, LifecycleReason::RestartRequested, nowMs);
}

LifecycleState LifecycleSupervisor::state() const noexcept
{
	return state_;
}

LifecycleReason LifecycleSupervisor::lastReason() const noexcept
{
	return lastReason_;
}

std::uint32_t LifecycleSupervisor::transitionSequence() const noexcept
{
	return transitionSequence_;
}

std::uint32_t LifecycleSupervisor::lastTransitionAtMs() const noexcept
{
	return lastTransitionAtMs_;
}

bool LifecycleSupervisor::acceptsOrdinaryCommands() const noexcept
{
	return initialized_ && (state_ == LifecycleState::Operational || state_ == LifecycleState::Degraded);
}

bool LifecycleSupervisor::isInitialized() const noexcept
{
	return initialized_;
}

bool LifecycleSupervisor::canTransition(const LifecycleState from, const LifecycleState to) noexcept
{
	switch (from)
	{
	case LifecycleState::Booting:
		return to == LifecycleState::Configuring || to == LifecycleState::Fault || to == LifecycleState::Restarting;
	case LifecycleState::Configuring:
		return to == LifecycleState::Operational || to == LifecycleState::Degraded || to == LifecycleState::Fault ||
			   to == LifecycleState::Restarting;
	case LifecycleState::Operational:
		return to == LifecycleState::Degraded || to == LifecycleState::Fault || to == LifecycleState::Restarting;
	case LifecycleState::Degraded:
		return to == LifecycleState::Operational || to == LifecycleState::Fault || to == LifecycleState::Restarting;
	case LifecycleState::Fault:
		return to == LifecycleState::Restarting;
	case LifecycleState::Restarting:
	default:
		return false;
	}
}

LifecycleResult LifecycleSupervisor::transitionTo(const LifecycleState target,
													  const LifecycleReason reason,
													  const std::uint32_t nowMs) noexcept
{
	if (!initialized_)
	{
		return LifecycleResult::InvalidTransition;
	}
	if (target == state_)
	{
		return LifecycleResult::NoChange;
	}
	if (!canTransition(state_, target))
	{
		return LifecycleResult::InvalidTransition;
	}

	const auto previousState = state_;
	state_ = target;
	lastReason_ = reason;
	lastTransitionAtMs_ = nowMs;
	++transitionSequence_;

	const LifecycleChanged event{previousState, state_, reason, transitionSequence_, nowMs};
	return eventSink_.publish(event) ? LifecycleResult::Applied : LifecycleResult::EventRejected;
}
}