#include "network_manager.h"

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

NetworkManager::NetworkManager(const hal::BoardDescriptor &board,
	app::ConfigurationService &configurationService,
	app::WifiManagementService &wifiManagementService,
	WifiAdapter &wifiAdapter,
	const ports::EthernetAdapterPort ethernetAdapter,
	Stream &provisioningStream) noexcept
	: board_{board}, configurationService_{configurationService}, wifiManagementService_{wifiManagementService},
	  wifiAdapter_{wifiAdapter}, ethernetAdapter_{ethernetAdapter}, improv_{&provisioningStream}
{
}

NetworkManager::~NetworkManager()
{
	shutdown();
}

void NetworkManager::initialize(const std::uint32_t nowMs) noexcept
{
	activeManager = this;
	if (board_.network.wifi)
	{
		wifiAdapter_.initialize(configuration().hostName.data());
		improv_.setCustomConnectWiFi(provisionWifi);
	}
	improv_.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32_S3, "SwitchActuator", "1.00", board_.model.data());
	status_.wifiAvailable = board_.network.wifi;
	status_.ethernetAvailable = board_.network.ethernet && ethernetAdapter_.isAvailable();
	initialized_ = true;
	if (!board_.network.wifi || !configuration().enabled)
	{
		transition(ports::NetworkLifecycleState::Disabled, nowMs);
		return;
	}
	beginNextProfile(nowMs);
}

void NetworkManager::shutdown() noexcept
{
	if (activeManager == this) activeManager = nullptr;
	if (!initialized_) return;
	ethernetAdapter_.shutdown();
	if (status_.wifiAvailable) wifiAdapter_.shutdown();
	status_ = {};
	retryScheduled_ = false;
	initialized_ = false;
}

void NetworkManager::update(const std::uint32_t nowMs) noexcept
{
	if (!initialized_)
	{
		return;
	}
	if (status_.wifiAvailable)
	{
		std::uint8_t noData{0};
		static_cast<void>(improv_.handleBuffer(&noData, 0));
		wifiAdapter_.updateScan(status_.wifiScan);
	}
	if (!configuration().enabled || !board_.network.wifi)
	{
		stopRecoveryAp();
		transition(ports::NetworkLifecycleState::Disabled, nowMs);
		return;
	}
	const auto wifi = wifiAdapter_.connection();
	if (wifi.online)
	{
		status_.infrastructureOnline = true;
		status_.activeTransport = ports::NetworkTransport::Wifi;
		status_.rssi = wifi.rssi;
		status_.ipv4Address = wifi.ipv4Address;
		status_.gateway = wifi.gateway;
		status_.dns = wifi.dns;
		if (status_.state != ports::NetworkLifecycleState::OnlineWifi)
		{
			status_.lastConnectedAtMs = nowMs;
			retryScheduled_ = false;
			retryDelayMs_ = 1000;
			stopRecoveryAp();
			transition(ports::NetworkLifecycleState::OnlineWifi, nowMs);
		}
		return;
	}
	status_.infrastructureOnline = false;
	status_.activeTransport = ports::NetworkTransport::None;
	if (status_.state == ports::NetworkLifecycleState::OnlineWifi)
	{
		retryAtMs_ = nowMs + retryDelayMs_;
		retryScheduled_ = true;
		retryDelayMs_ = retryDelayMs_ < maximumRetryDelayMs / 2 ? retryDelayMs_ * 2 : maximumRetryDelayMs;
		transition(ports::NetworkLifecycleState::ConnectingWifi, nowMs);
	}
	if (status_.state == ports::NetworkLifecycleState::ConnectingWifi && nowMs - attemptStartedAtMs_ >= profileAttemptTimeoutMs)
	{
		beginNextProfile(nowMs);
	}
	if (status_.state == ports::NetworkLifecycleState::RecoveryAp && retryScheduled_ &&
		static_cast<std::int32_t>(nowMs - retryAtMs_) >= 0)
	{
		retryScheduled_ = false;
		beginNextProfile(nowMs);
	}
	if (status_.recoveryApActive && !configuration().recoveryAp.remainActiveWhileOffline &&
		configuration().recoveryAp.timeoutMs != 0 && nowMs - recoveryApStartedAtMs_ >= configuration().recoveryAp.timeoutMs)
	{
		stopRecoveryAp();
		transition(ports::NetworkLifecycleState::ConnectingWifi, nowMs);
	}
}

void NetworkManager::ingestProvisioning(const std::uint8_t *const data, const std::size_t length) noexcept
{
	if (initialized_ && status_.wifiAvailable && data != nullptr && length != 0)
	{
		static_cast<void>(improv_.handleBuffer(const_cast<std::uint8_t *>(data), static_cast<std::uint16_t>(length)));
	}
}

ports::NetworkStatusPort NetworkManager::statusPort() noexcept { return {statusCallback, this}; }

ports::NetworkControlPort NetworkManager::controlPort() noexcept
{
	return {startWifiScanCallback, connectWifiProfileCallback, applyConfigurationCallback, this};
}

hal::NetworkHal NetworkManager::hal() noexcept
{
	return {statusPort(), controlPort()};
}

const ports::NetworkStatusSnapshot &NetworkManager::statusCallback(void *const context) noexcept
{
	return static_cast<NetworkManager *>(context)->status_;
}

bool NetworkManager::startWifiScanCallback(void *const context, const std::uint32_t nowMs) noexcept
{
	return static_cast<NetworkManager *>(context)->startWifiScan(nowMs);
}

bool NetworkManager::connectWifiProfileCallback(void *const context,
	const std::uint8_t profileIndex,
	const std::uint32_t nowMs) noexcept
{
	return static_cast<NetworkManager *>(context)->connectWifiProfile(profileIndex, nowMs);
}

void NetworkManager::applyConfigurationCallback(void *const context, const std::uint32_t nowMs) noexcept
{
	static_cast<NetworkManager *>(context)->applyCommittedConfiguration(nowMs);
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
	app::WifiProfilePatch patch{};
	patch.index = profileIndex;
	patch.enabled = true;
	patch.passphraseUpdate = app::WifiSecretUpdate::Replace;
	patch.expectedGeneration = configurationService_.active().generation;
	std::snprintf(patch.ssid.data(), patch.ssid.size(), "%s", ssid);
	std::snprintf(patch.passphrase.data(), patch.passphrase.size(), "%s", passphrase);
	if (wifiManagementService_.saveProfile(patch) != app::WifiManagementResult::Applied)
	{
		return false;
	}
	return connectWifiProfile(profileIndex, nowMs);
}

bool NetworkManager::connectWifiProfile(const std::uint8_t profileIndex, const std::uint32_t nowMs) noexcept
{
	if (!initialized_ || !board_.network.wifi || profileIndex >= configuration().wifiProfiles.size() ||
		!configuration().wifiProfiles[profileIndex].enabled)
	{
		return false;
	}
	stopRecoveryAp();
	wifiAdapter_.disconnect();
	nextProfileIndex_ = profileIndex;
	if (!configureProfile(profileIndex))
	{
		return false;
	}
	attemptStartedAtMs_ = nowMs;
	transition(ports::NetworkLifecycleState::ConnectingWifi, nowMs);
	return true;
}

void NetworkManager::applyCommittedConfiguration(const std::uint32_t nowMs) noexcept
{
	stopRecoveryAp();
	wifiAdapter_.disconnect();
	nextProfileIndex_ = 0;
	beginNextProfile(nowMs);
}

bool NetworkManager::startWifiScan(const std::uint32_t nowMs) noexcept
{
	if (!initialized_ || !board_.network.wifi || status_.wifiScan.state == ports::WifiScanState::Scanning)
	{
		return false;
	}
	return wifiAdapter_.startScan(status_.wifiScan, nowMs);
}

bool NetworkManager::configureProfile(const std::uint8_t index) noexcept
{
	const auto &profile = configuration().wifiProfiles[index];
	if (!profile.enabled)
	{
		return false;
	}
	if (!wifiAdapter_.connect(profile)) return false;
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
		retryScheduled_ = true;
		return;
	}
	if (status_.recoveryApActive)
	{
		retryAtMs_ = nowMs + retryDelayMs_;
		retryScheduled_ = true;
		transition(ports::NetworkLifecycleState::RecoveryAp, nowMs);
		return;
	}
	char ssid[33]{};
	char password[17]{};
	const auto &uuid = configurationService_.active().deviceUuid;
	std::snprintf(ssid, sizeof(ssid), "%s-%02X%02X", configuration().recoveryAp.ssidPrefix.data(), uuid[14], uuid[15]);
	std::snprintf(password, sizeof(password), "Rly%02X%02X%02X%02X%02X%02X", uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
	status_.recoveryApActive = wifiAdapter_.startRecoveryAp(ssid, password, configuration().recoveryAp.channel);
	recoveryApStartedAtMs_ = nowMs;
	retryAtMs_ = nowMs + retryDelayMs_;
	retryScheduled_ = true;
	transition(ports::NetworkLifecycleState::RecoveryAp, nowMs);
}

void NetworkManager::stopRecoveryAp() noexcept
{
	if (status_.recoveryApActive)
	{
		wifiAdapter_.stopRecoveryAp();
		status_.recoveryApActive = false;
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