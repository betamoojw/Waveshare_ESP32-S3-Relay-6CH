#pragma once

#include <cstdint>

namespace switch_actuator::adapters::knx
{
enum class KnxInitializeResult : std::uint8_t
{
	Initialized,
	Disabled,
	Unavailable,
	InvalidDependencies
};

enum class KnxPollResult : std::uint8_t
{
	Idle,
	TelegramHandled,
	Unavailable,
	NotInitialized
};
}