#pragma once

#include "../../app/configuration_service.h"
#include "../../app/wifi_management_service.h"
#include "../../ports/network_control_port.h"
#include "../../ports/network_status_port.h"
#include "../../hal/BoardDescriptor.h"
#include "../../hal/NetworkHal.h"
#include "../../ports/ethernet_adapter_port.h"
#include "wifi_adapter.h"

#include <Arduino.h>
#include <ImprovWiFiLibrary.h>

#include <cstdint>

namespace switch_actuator::adapters::network
{
class NetworkManager final
{
public:
	NetworkManager(const hal::BoardDescriptor &board,
		app::ConfigurationService &configurationService,
		app::WifiManagementService &wifiManagementService,
		WifiAdapter &wifiAdapter,
		ports::EthernetAdapterPort ethernetAdapter,
		Stream &provisioningStream) noexcept;
	~NetworkManager();

	NetworkManager(const NetworkManager &) = delete;
	NetworkManager &operator=(const NetworkManager &) = delete;
	NetworkManager(NetworkManager &&) = delete;
	NetworkManager &operator=(NetworkManager &&) = delete;

	void initialize(std::uint32_t nowMs) noexcept;
	void shutdown() noexcept;
	void update(std::uint32_t nowMs) noexcept;
	void ingestProvisioning(const std::uint8_t *data, std::size_t length) noexcept;
	[[nodiscard]] bool provisionWifiProfile(std::uint8_t profileIndex, const char *ssid, const char *passphrase,
		std::uint32_t nowMs) noexcept;
	[[nodiscard]] bool startWifiScan(std::uint32_t nowMs) noexcept;
	[[nodiscard]] bool connectWifiProfile(std::uint8_t profileIndex, std::uint32_t nowMs) noexcept;
	void applyCommittedConfiguration(std::uint32_t nowMs) noexcept;
	[[nodiscard]] ports::NetworkStatusPort statusPort() noexcept;
	[[nodiscard]] ports::NetworkControlPort controlPort() noexcept;
	[[nodiscard]] hal::NetworkHal hal() noexcept;

private:
	static const ports::NetworkStatusSnapshot &statusCallback(void *context) noexcept;
	static bool startWifiScanCallback(void *context, std::uint32_t nowMs) noexcept;
	static bool connectWifiProfileCallback(void *context, std::uint8_t profileIndex, std::uint32_t nowMs) noexcept;
	static void applyConfigurationCallback(void *context, std::uint32_t nowMs) noexcept;
	static bool provisionWifi(const char *ssid, const char *passphrase);
	[[nodiscard]] bool configureProfile(std::uint8_t index) noexcept;
	void beginNextProfile(std::uint32_t nowMs) noexcept;
	void startRecoveryAp(std::uint32_t nowMs) noexcept;
	void stopRecoveryAp() noexcept;
	void transition(ports::NetworkLifecycleState state, std::uint32_t nowMs) noexcept;
	[[nodiscard]] const domain::NetworkConfiguration &configuration() const noexcept;

	const hal::BoardDescriptor &board_;
	app::ConfigurationService &configurationService_;
	app::WifiManagementService &wifiManagementService_;
	WifiAdapter &wifiAdapter_;
	ports::EthernetAdapterPort ethernetAdapter_;
	ImprovWiFi improv_;
	ports::NetworkStatusSnapshot status_{};
	std::uint32_t attemptStartedAtMs_{0};
	std::uint32_t recoveryApStartedAtMs_{0};
	std::uint32_t retryAtMs_{0};
	std::uint32_t retryDelayMs_{1000};
	std::uint8_t nextProfileIndex_{0};
	bool retryScheduled_{false};
	bool initialized_{false};
};
}