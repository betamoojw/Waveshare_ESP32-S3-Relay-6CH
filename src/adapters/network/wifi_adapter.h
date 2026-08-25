#pragma once

#include "../../domain/configuration.h"
#include "../../ports/network_status_port.h"

#include <cstdint>

namespace switch_actuator::adapters::network
{
struct WifiConnectionSnapshot final
{
	bool online{false};
	std::int32_t rssi{0};
	std::array<std::uint8_t, 4> ipv4Address{};
	std::array<std::uint8_t, 4> gateway{};
	std::array<std::uint8_t, 4> dns{};
};

class WifiAdapter final
{
public:
	void initialize(const char *hostName) noexcept;
	void shutdown() noexcept;
	void disconnect() noexcept;
	[[nodiscard]] bool connect(const domain::WifiProfile &profile) noexcept;
	[[nodiscard]] WifiConnectionSnapshot connection() const noexcept;
	[[nodiscard]] bool startScan(ports::WifiScanSnapshot &snapshot, std::uint32_t nowMs) noexcept;
	void updateScan(ports::WifiScanSnapshot &snapshot) noexcept;
	[[nodiscard]] bool startRecoveryAp(const char *ssid, const char *password, std::uint8_t channel) noexcept;
	void stopRecoveryAp() noexcept;
};
}