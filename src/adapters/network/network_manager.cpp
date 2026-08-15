#include "network_manager.h"

#include <WiFi.h>

#include <cstdio>
#include <cstring>

namespace switch_actuator::adapters::network
{
namespace
{
constexpr std::uint32_t profileAttemptTimeoutMs{20'000};
constexpr std::uint32_t maximumRetryDelayMs{60'000};
NetworkManager *activeManager{nullptr};
}

NetworkManager::NetworkManager(const bsp::BoardDescriptor &board,
	app::ConfigurationService &configurationService,
	Stream &provisioningStream) noexcept
	: board_{board}, configurationService_{configurationService}, improv_{&provisioningStream}
{
}

void NetworkManager::initialize(const std::uint32_t nowMs) noexcept
{
	activeManager = this;
	WiFi.persistent(false);
	WiFi.mode(WIFI_STA);
	WiFi.setAutoReconnect(false);
	WiFi.setHostname(configuration().hostName.data());
	improv_.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32_S3, "SwitchActuator", "1.00", board_.model.data());
	improv_.setCustomConnectWiFi(provisionWifi);
	initialized_ = true;
	if (!board_.wifiSupported || !configuration().enabled)
	{
		transition(ports::NetworkLifecycleState::Disabled, nowMs);
		return;
	}
	beginNextProfile(nowMs);
}

void NetworkManager::update(const std::uint32_t nowMs) noexcept
{
	if (!initialized_)
	{
		return;
	}
	std::uint8_t noData{0};
	static_cast<void>(improv_.handleBuffer(&noData, 0));
	if (!configuration().enabled || !board_.wifiSupported)
	{
		stopRecoveryAp();
		transition(ports::NetworkLifecycleState::Disabled, nowMs);
		return;
	}
	if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE)
	{
		status_.infrastructureOnline = true;
		status_.rssi = WiFi.RSSI();
		if (status_.state != ports::NetworkLifecycleState::OnlineWifi)
		{
			status_.lastConnectedAtMs = nowMs;
			stopRecoveryAp();
			transition(ports::NetworkLifecycleState::OnlineWifi, nowMs);
		}
		return;
	}
	status_.infrastructureOnline = false;
	if (status_.state == ports::NetworkLifecycleState::OnlineWifi)
	{
		retryAtMs_ = nowMs + retryDelayMs_;
		retryDelayMs_ = retryDelayMs_ < maximumRetryDelayMs / 2 ? retryDelayMs_ * 2 : maximumRetryDelayMs;
		transition(ports::NetworkLifecycleState::ConnectingWifi, nowMs);
	}
	if (status_.state == ports::NetworkLifecycleState::ConnectingWifi && nowMs - attemptStartedAtMs_ >= profileAttemptTimeoutMs)
	{
		beginNextProfile(nowMs);
	}
	if ((status_.state == ports::NetworkLifecycleState::RecoveryAp || status_.state == ports::NetworkLifecycleState::ConnectingWifi) &&
		nowMs >= retryAtMs_ && status_.state != ports::NetworkLifecycleState::ConnectingWifi)
	{
		beginNextProfile(nowMs);
	}
}

void NetworkManager::ingestProvisioning(const std::uint8_t *const data, const std::size_t length) noexcept
{
	if (initialized_ && data != nullptr && length != 0)
	{
		static_cast<void>(improv_.handleBuffer(const_cast<std::uint8_t *>(data), static_cast<std::uint16_t>(length)));
	}
}

ports::NetworkStatusPort NetworkManager::statusPort() noexcept { return {statusCallback, this}; }

const ports::NetworkStatusSnapshot &NetworkManager::statusCallback(void *const context) noexcept
{
	return static_cast<NetworkManager *>(context)->status_;
}

bool NetworkManager::provisionWifi(const char *const ssid, const char *const passphrase)
{
	return activeManager != nullptr && activeManager->provisionWifiProfile(0, ssid, passphrase, millis());
}

bool NetworkManager::provisionWifiProfile(const std::uint8_t profileIndex,
	const char *const ssid,
	const char *const passphrase,
	const std::uint32_t nowMs) noexcept
{
	if (profileIndex >= configuration().wifiProfiles.size() || ssid == nullptr || *ssid == '\0' || passphrase == nullptr ||
		std::strlen(ssid) >= domain::wifiSsidCapacity || std::strlen(passphrase) >= domain::wifiPassphraseCapacity)
	{
		return false;
	}
	auto replacement = configurationService_.active();
	auto &profile = replacement.network.wifiProfiles[profileIndex];
	profile = {};
	profile.enabled = true;
	std::snprintf(profile.ssid.data(), profile.ssid.size(), "%s", ssid);
	std::snprintf(profile.passphrase.data(), profile.passphrase.size(), "%s", passphrase);
	if (configurationService_.stage(replacement) != app::ConfigurationStageResult::Staged ||
		configurationService_.commit() == app::ConfigurationCommitResult::PersistenceFailure)
	{
		return false;
	}
	WiFi.disconnect(false, false);
	nextProfileIndex_ = profileIndex;
	beginNextProfile(nowMs);
	return true;
}

bool NetworkManager::configureProfile(const std::uint8_t index) noexcept
{
	const auto &profile = configuration().wifiProfiles[index];
	if (!profile.enabled)
	{
		return false;
	}
	if (profile.ipv4.mode == domain::IpMode::Static)
	{
		WiFi.config(IPAddress(profile.ipv4.address.data()), IPAddress(profile.ipv4.gateway.data()),
			IPAddress(profile.ipv4.subnetMask.data()), IPAddress(profile.ipv4.dns.data()));
	}
	else
	{
		WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
	}
	WiFi.begin(profile.ssid.data(), profile.passphrase.data());
	status_.activeProfileIndex = index;
	return true;
}

void NetworkManager::beginNextProfile(const std::uint32_t nowMs) noexcept
{
	for (std::size_t offset = 0; offset < configuration().wifiProfiles.size(); ++offset)
	{
		const auto index = static_cast<std::uint8_t>((nextProfileIndex_ + offset) % configuration().wifiProfiles.size());
		if (configureProfile(index))
		{
			nextProfileIndex_ = static_cast<std::uint8_t>((index + 1) % configuration().wifiProfiles.size());
			attemptStartedAtMs_ = nowMs;
			transition(ports::NetworkLifecycleState::ConnectingWifi, nowMs);
			return;
		}
	}
	startRecoveryAp(nowMs);
}

void NetworkManager::startRecoveryAp(const std::uint32_t nowMs) noexcept
{
	if (!configuration().recoveryAp.enabled)
	{
		transition(ports::NetworkLifecycleState::ConnectingWifi, nowMs);
		retryAtMs_ = nowMs + retryDelayMs_;
		return;
	}
	char ssid[33]{};
	char password[17]{};
	const auto &uuid = configurationService_.active().deviceUuid;
	std::snprintf(ssid, sizeof(ssid), "%s-%02X%02X", configuration().recoveryAp.ssidPrefix.data(), uuid[14], uuid[15]);
	std::snprintf(password, sizeof(password), "Rly%02X%02X%02X%02X%02X%02X", uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
	WiFi.mode(WIFI_AP_STA);
	status_.recoveryApActive = WiFi.softAP(ssid, password, configuration().recoveryAp.channel);
	retryAtMs_ = nowMs + retryDelayMs_;
	transition(ports::NetworkLifecycleState::RecoveryAp, nowMs);
}

void NetworkManager::stopRecoveryAp() noexcept
{
	if (status_.recoveryApActive)
	{
		WiFi.softAPdisconnect(true);
		status_.recoveryApActive = false;
		WiFi.mode(WIFI_STA);
	}
}

void NetworkManager::transition(const ports::NetworkLifecycleState state, const std::uint32_t nowMs) noexcept
{
	if (status_.state != state)
	{
		status_.state = state;
		++status_.sequence;
		status_.lastStateChangeAtMs = nowMs;
	}
}

const domain::NetworkConfiguration &NetworkManager::configuration() const noexcept
{
	return configurationService_.active().network;
}
}