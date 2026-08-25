#include "Board.h"

#ifndef SWITCH_ACTUATOR_BOARD
#define SWITCH_ACTUATOR_BOARD 0
#endif

#if SWITCH_ACTUATOR_BOARD == 0
#include "../../boards/waveshare_s3_relay_6ch/board_descriptor.h"
#elif SWITCH_ACTUATOR_BOARD == 1
#include "../../boards/custom_relay_6ch/board_descriptor.h"
#elif SWITCH_ACTUATOR_BOARD == 2
#include "../../boards/custom_relay_12ch/board_descriptor.h"
#else
#error "Unsupported SWITCH_ACTUATOR_BOARD value"
#endif

namespace switch_actuator::hal
{
const BoardDescriptor &board() noexcept
{
#if SWITCH_ACTUATOR_BOARD == 0
	return boards::waveshare_s3_relay_6ch::descriptor;
#elif SWITCH_ACTUATOR_BOARD == 1
	return boards::custom_relay_6ch::descriptor;
#else
	return boards::custom_relay_12ch::descriptor;
#endif
}
}