#pragma once

#include "board_descriptor.h"

namespace switch_actuator::adapters::bsp
{
inline constexpr BoardDescriptor waveshareEsp32S3Relay6Ch{
	"Waveshare ESP32-S3-Relay-6CH",
	"1.x",
	{1, 2, 41, 42, 45, 46},
	RelayPolarity::ActiveHigh,
	0,
	ButtonPullMode::PullUp,
	true,
	17,
	18,
	21,
	38,
	true,
	false,
	EthernetImplementation::None,
};
}