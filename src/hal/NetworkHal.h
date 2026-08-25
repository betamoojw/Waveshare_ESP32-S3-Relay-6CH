#pragma once

#include "../ports/network_control_port.h"
#include "../ports/network_status_port.h"

namespace switch_actuator::hal
{
struct NetworkHal final
{
	ports::NetworkStatusPort status{};
	ports::NetworkControlPort control{};
};
}