#pragma once

#include "../../ports/ethernet_adapter_port.h"

namespace switch_actuator::adapters::network
{
class NullEthernetAdapter final
{
public:
	[[nodiscard]] ports::EthernetAdapterPort port() noexcept
	{
		return {initialize, shutdown, update, snapshot, this};
	}

private:
	static bool initialize(void *, const char *) noexcept { return false; }
	static void shutdown(void *) noexcept {}
	static void update(void *, std::uint32_t) noexcept {}
	static const ports::EthernetAdapterSnapshot &snapshot(void *context) noexcept
	{
		return static_cast<NullEthernetAdapter *>(context)->snapshot_;
	}

	ports::EthernetAdapterSnapshot snapshot_{};
};
}