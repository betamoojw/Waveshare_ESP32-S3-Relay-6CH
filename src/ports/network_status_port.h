#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace switch_actuator::ports
{
enum class NetworkLifecycleState : std::uint8_t
{
	Disabled,
	ConnectingWifi,
	OnlineWifi,
	RecoveryAp,
	ConnectingEthernet,
	OnlineEthernet
};
enum class NetworkTransport : std::uint8_t { None, Wifi, Ethernet };
enum class WifiScanState : std::uint8_t { Idle, Scanning, Complete, Failed };

inline constexpr std::size_t maximumWifiScanResults{16};
inline constexpr std::size_t wifiScanSsidCapacity{33};

struct WifiScanResult final
{
	std::array<char, wifiScanSsidCapacity> ssid{};
	std::int32_t rssi{0};
	std::uint8_t channel{0};
	bool secured{false};
};

struct WifiScanSnapshot final
{
	WifiScanState state{WifiScanState::Idle};
	std::uint32_t sequence{0};
	std::uint32_t startedAtMs{0};
	std::array<WifiScanResult, maximumWifiScanResults> results{};
	std::uint8_t resultCount{0};
};

struct NetworkStatusSnapshot final
{
	NetworkLifecycleState state{NetworkLifecycleState::Disabled};
	std::uint32_t sequence{0};
	std::uint32_t lastStateChangeAtMs{0};
	std::uint32_t lastConnectedAtMs{0};
	std::uint8_t activeProfileIndex{0xFF};
	NetworkTransport activeTransport{NetworkTransport::None};
	bool wifiAvailable{false};
	bool ethernetAvailable{false};
	bool infrastructureOnline{false};
	bool recoveryApActive{false};
	std::int32_t rssi{0};
	std::array<std::uint8_t, 4> ipv4Address{};
	std::array<std::uint8_t, 4> gateway{};
	std::array<std::uint8_t, 4> dns{};
	WifiScanSnapshot wifiScan{};
};

using NetworkStatusHandler = const NetworkStatusSnapshot &(*)(void *context) noexcept;

class NetworkStatusPort final
{
public:
	constexpr NetworkStatusPort() noexcept = default;
	constexpr NetworkStatusPort(NetworkStatusHandler handler, void *context) noexcept : handler_{handler}, context_{context} {}
	[[nodiscard]] const NetworkStatusSnapshot &snapshot() const noexcept
	{
		static const NetworkStatusSnapshot unavailable{};
		return handler_ == nullptr ? unavailable : handler_(context_);
	}
	[[nodiscard]] bool isOnline() const noexcept { return snapshot().infrastructureOnline; }
private:
	NetworkStatusHandler handler_{nullptr};
	void *context_{nullptr};
};
}