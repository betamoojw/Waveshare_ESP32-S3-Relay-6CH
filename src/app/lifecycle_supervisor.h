#pragma once

#include "../ports/event_sink.h"

#include <cstdint>

namespace switch_actuator::app
{
enum class LifecycleState : std::uint8_t
{
	Booting,
	Configuring,
	Operational,
	Degraded,
	Fault,
	Restarting
};

enum class LifecycleReason : std::uint8_t
{
	Startup,
	ConfigurationStarted,
	ConfigurationValid,
	ConfigurationInvalid,
	AdapterUnavailable,
	AdapterRecovered,
	CriticalFault,
	RestartRequested
};

struct LifecycleChanged final
{
	LifecycleState previousState;
	LifecycleState state;
	LifecycleReason reason;
	std::uint32_t transitionSequence;
	std::uint32_t occurredAtMs;
};

enum class LifecycleResult : std::uint8_t
{
	Applied,
	NoChange,
	InvalidTransition,
	InvalidEventSink,
	EventRejected
};

class LifecycleSupervisor final
{
public:
	using LifecycleEventSink = ports::EventSink<LifecycleChanged>;

	explicit LifecycleSupervisor(LifecycleEventSink eventSink) noexcept;

	[[nodiscard]] LifecycleResult initialize(std::uint32_t nowMs) noexcept;
	[[nodiscard]] LifecycleResult beginConfiguration(std::uint32_t nowMs) noexcept;
	[[nodiscard]] LifecycleResult enterOperational(std::uint32_t nowMs) noexcept;
	[[nodiscard]] LifecycleResult enterDegraded(LifecycleReason reason, std::uint32_t nowMs) noexcept;
	[[nodiscard]] LifecycleResult enterFault(LifecycleReason reason, std::uint32_t nowMs) noexcept;
	[[nodiscard]] LifecycleResult requestRestart(std::uint32_t nowMs) noexcept;

	[[nodiscard]] LifecycleState state() const noexcept;
	[[nodiscard]] LifecycleReason lastReason() const noexcept;
	[[nodiscard]] std::uint32_t transitionSequence() const noexcept;
	[[nodiscard]] std::uint32_t lastTransitionAtMs() const noexcept;
	[[nodiscard]] bool acceptsOrdinaryCommands() const noexcept;
	[[nodiscard]] bool isInitialized() const noexcept;

private:
	[[nodiscard]] static bool canTransition(LifecycleState from, LifecycleState to) noexcept;
	[[nodiscard]] LifecycleResult transitionTo(LifecycleState target, LifecycleReason reason, std::uint32_t nowMs) noexcept;

	LifecycleEventSink eventSink_;
	LifecycleState state_{LifecycleState::Booting};
	LifecycleReason lastReason_{LifecycleReason::Startup};
	std::uint32_t transitionSequence_{0};
	std::uint32_t lastTransitionAtMs_{0};
	bool initialized_{false};
};
}