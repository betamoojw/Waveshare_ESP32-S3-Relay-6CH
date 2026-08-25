#pragma once

#include "../hal/RelayHal.h"

namespace switch_actuator::ports
{
using RelayOutputResult = hal::RelayHalResult;
using RelayOutputHandler = hal::RelayApplyHandler;
using RelayOutputPort = hal::RelayHal;
}