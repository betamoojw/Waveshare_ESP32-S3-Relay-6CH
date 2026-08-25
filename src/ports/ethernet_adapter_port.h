#pragma once

#include <array>
#include <cstdint>

namespace switch_actuator::ports
{
struct EthernetAdapterSnapshot final
{
	bool available{false};
	bool linkUp{false};
	bool online{false};
	std::array<std::uint8_t, 4> ipv4Address{};
	std::array<std::uint8_t, 4> gateway{};
	std::array<std::uint8_t, 4> dns{};
};

using EthernetInitializeHandler = bool (*)(void *context, const char *hostName) noexcept;
using EthernetShutdownHandler = void (*)(void *context) noexcept;
using EthernetUpdateHandler = void (*)(void *context, std::uint32_t nowMs) noexcept;
using EthernetSnapshotHandler = const EthernetAdapterSnapshot &(*)(void *context) noexcept;

class EthernetAdapterPort final
{
public:
	constexpr EthernetAdapterPort() noexcept = default;
	constexpr EthernetAdapterPort(EthernetInitializeHandler initialize,
		EthernetShutdownHandler shutdown,
		EthernetUpdateHandler update,
		EthernetSnapshotHandler snapshot,
		void *context) noexcept
		: initialize_{initialize}, shutdown_{shutdown}, update_{update}, snapshot_{snapshot}, context_{context}
	{
	}

	[[nodiscard]] bool isAvailable() const noexcept { return snapshot().available; }
	[[nodiscard]] bool initialize(const char *hostName) const noexcept
	{
		return initialize_ != nullptr && initialize_(context_, hostName);
	}
	void shutdown() const noexcept { if (shutdown_ != nullptr) shutdown_(context_); }
	void update(const std::uint32_t nowMs) const noexcept { if (update_ != nullptr) update_(context_, nowMs); }
	[[nodiscard]] const EthernetAdapterSnapshot &snapshot() const noexcept
	{
		static const EthernetAdapterSnapshot unavailable{};
		return snapshot_ == nullptr ? unavailable : snapshot_(context_);
	}

private:
	EthernetInitializeHandler initialize_{nullptr};
	EthernetShutdownHandler shutdown_{nullptr};
	EthernetUpdateHandler update_{nullptr};
	EthernetSnapshotHandler snapshot_{nullptr};
	void *context_{nullptr};
};
}