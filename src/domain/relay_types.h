#pragma once

#include <cstdint>

namespace switch_actuator::domain
{
inline constexpr std::uint8_t relayChannelCount{6};

enum class RelayState : std::uint8_t
{
	Off,
	On
};

enum class RelayAction : std::uint8_t
{
	SetOff,
	SetOn,
	Toggle
};

enum class CommandSource : std::uint8_t
{
	Safety,
	Button,
	Knx,
	Modbus,
	Web,
	Cli,
	Restore
};

enum class RelayFault : std::uint8_t
{
	None,
	OutputFailure
};

struct RelayChannelId final
{
	std::uint8_t value;
};

struct RelayCommand final
{
	RelayChannelId channel;
	RelayAction action;
	CommandSource source;
	std::uint32_t correlationId;
	std::uint32_t receivedAtMs;
};

struct RelaySnapshot final
{
	RelayState requestedState{RelayState::Off};
	RelayState appliedState{RelayState::Off};
	CommandSource lastCommandSource{CommandSource::Safety};
	std::uint32_t transitionSequence{0};
	std::uint32_t lastTransitionAtMs{0};
	RelayFault fault{RelayFault::None};
	bool lockedOut{false};
};

struct RelayStateChanged final
{
	RelayChannelId channel;
	RelayState appliedState;
	CommandSource source;
	std::uint32_t correlationId;
	std::uint32_t transitionSequence;
	std::uint32_t occurredAtMs;
};
}