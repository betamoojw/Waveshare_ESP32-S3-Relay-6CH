#pragma once

#include "../../src/hal/BoardDescriptor.h"

#include <array>

namespace switch_actuator::boards::waveshare_s3_relay_6ch
{
inline constexpr std::array<std::uint8_t, 6> relayPins{1, 2, 41, 42, 45, 46};

inline constexpr hal::BoardDescriptor descriptor{
	"SA-6CH-S3",
	"Waveshare ESP32-S3-Relay-6CH",
	"HW-A01",
	static_cast<std::uint8_t>(relayPins.size()),
	relayPins.data(),
	hal::RelayPolarity::ActiveHigh,
	{0, hal::ButtonPullMode::PullUp, true},
	{38, 21},
	{17, 18, true},
	{true, false, hal::EthernetImplementation::None},
};

static_assert(hal::isValid(descriptor), "Invalid Waveshare ESP32-S3 Relay 6CH descriptor");
}