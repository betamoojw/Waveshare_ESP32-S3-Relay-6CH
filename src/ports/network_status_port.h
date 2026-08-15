#pragma once

#include <cstdint>

namespace switch_actuator::ports
{
enum class NetworkLifecycleState : std::uint8_t { Disabled, ConnectingWifi, OnlineWifi, RecoveryAp };

struct NetworkStatusSnapshot final
{
	NetworkLifecycleState state{NetworkLifecycleState::Disabled};
	std::uint32_t sequence{0};
	std::uint32_t lastStateChangeAtMs{0};
	std::uint32_t lastConnectedAtMs{0};
	std::uint8_t activeProfileIndex{0xFF};
	bool infrastructureOnline{false};
	bool recoveryApActive{false};
	std::int32_t rssi{0};
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