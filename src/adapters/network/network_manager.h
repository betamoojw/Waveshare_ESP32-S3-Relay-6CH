#pragma once

#include "../../app/configuration_service.h"
#include "../../ports/network_status_port.h"
#include "../bsp/board_descriptor.h"

#include <Arduino.h>
#include <ImprovWiFiLibrary.h>

#include <cstdint>

namespace switch_actuator::adapters::network
{
class NetworkManager final
{
public:
	NetworkManager(const bsp::BoardDescriptor &board, app::ConfigurationService &configurationService, Stream &provisioningStream) noexcept;
	void initialize(std::uint32_t nowMs) noexcept;
	void update(std::uint32_t nowMs) noexcept;
	void ingestProvisioning(const std::uint8_t *data, std::size_t length) noexcept;
	[[nodiscard]] bool provisionWifiProfile(std::uint8_t profileIndex, const char *ssid, const char *passphrase,
		std::uint32_t nowMs) noexcept;
	[[nodiscard]] ports::NetworkStatusPort statusPort() noexcept;

private:
	static const ports::NetworkStatusSnapshot &statusCallback(void *context) noexcept;
	static bool provisionWifi(const char *ssid, const char *passphrase);
	[[nodiscard]] bool configureProfile(std::uint8_t index) noexcept;
	void beginNextProfile(std::uint32_t nowMs) noexcept;
	void startRecoveryAp(std::uint32_t nowMs) noexcept;
	void stopRecoveryAp() noexcept;
	void transition(ports::NetworkLifecycleState state, std::uint32_t nowMs) noexcept;
	[[nodiscard]] const domain::NetworkConfiguration &configuration() const noexcept;

	const bsp::BoardDescriptor &board_;
	app::ConfigurationService &configurationService_;
	ImprovWiFi improv_;
	ports::NetworkStatusSnapshot status_{};
	std::uint32_t attemptStartedAtMs_{0};
	std::uint32_t retryAtMs_{0};
	std::uint32_t retryDelayMs_{1000};
	std::uint8_t nextProfileIndex_{0};
	bool initialized_{false};
};
}