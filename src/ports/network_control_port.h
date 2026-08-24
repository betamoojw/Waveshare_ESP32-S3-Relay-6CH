#pragma once

#include <cstdint>

namespace switch_actuator::ports
{
using StartWifiScanHandler = bool (*)(void *context, std::uint32_t nowMs) noexcept;
using ConnectWifiProfileHandler = bool (*)(void *context, std::uint8_t profileIndex, std::uint32_t nowMs) noexcept;
using ApplyNetworkConfigurationHandler = void (*)(void *context, std::uint32_t nowMs) noexcept;

class NetworkControlPort final
{
public:
	constexpr NetworkControlPort() noexcept = default;
	constexpr NetworkControlPort(StartWifiScanHandler startScan,
		ConnectWifiProfileHandler connectProfile,
		ApplyNetworkConfigurationHandler applyConfiguration,
		void *context) noexcept
		: startScan_{startScan}, connectProfile_{connectProfile}, applyConfiguration_{applyConfiguration}, context_{context}
	{
	}

	[[nodiscard]] bool isValid() const noexcept
	{
		return startScan_ != nullptr && connectProfile_ != nullptr && applyConfiguration_ != nullptr;
	}
	[[nodiscard]] bool startWifiScan(const std::uint32_t nowMs) const noexcept
	{
		return startScan_ != nullptr && startScan_(context_, nowMs);
	}
	[[nodiscard]] bool connectWifiProfile(const std::uint8_t profileIndex, const std::uint32_t nowMs) const noexcept
	{
		return connectProfile_ != nullptr && connectProfile_(context_, profileIndex, nowMs);
	}
	void applyCommittedConfiguration(const std::uint32_t nowMs) const noexcept
	{
		if (applyConfiguration_ != nullptr) applyConfiguration_(context_, nowMs);
	}

private:
	StartWifiScanHandler startScan_{nullptr};
	ConnectWifiProfileHandler connectProfile_{nullptr};
	ApplyNetworkConfigurationHandler applyConfiguration_{nullptr};
	void *context_{nullptr};
};
}