#pragma once

#include "BoardDescriptor.h"

namespace switch_actuator::hal
{
[[nodiscard]] const BoardDescriptor &board() noexcept;
}