#include "wifi_adapter.h"

#include <WiFi.h>

#include <algorithm>
#include <cstdio>

namespace switch_actuator::adapters::network
{
namespace
{
void copyIpv4Address(const IPAddress &source, std::array<std::uint8_t, 4> &destination) noexcept
{
	for (std::size_t index = 0; index < destination.size(); ++index)
	{
		destination[index] = source[index];
	}
}
}

void WifiAdapter::initialize(const char *const hostName) noexcept
{
	WiFi.persistent(false);
	WiFi.mode(WIFI_STA);
	WiFi.setAutoReconnect(false);
	WiFi.setHostname(hostName);
}

void WifiAdapter::shutdown() noexcept
{
	WiFi.scanDelete();
	WiFi.softAPdisconnect(true);
	WiFi.disconnect(true, false);
	WiFi.mode(WIFI_OFF);
}

void WifiAdapter::disconnect() noexcept
{
	WiFi.disconnect(false, false);
}

bool WifiAdapter::connect(const domain::WifiProfile &profile) noexcept
{
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
	return true;
}

WifiConnectionSnapshot WifiAdapter::connection() const noexcept
{
	WifiConnectionSnapshot result{};
	result.online = WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE;
	if (!result.online)
	{
		return result;
	}
	result.rssi = WiFi.RSSI();
	copyIpv4Address(WiFi.localIP(), result.ipv4Address);
	copyIpv4Address(WiFi.gatewayIP(), result.gateway);
	copyIpv4Address(WiFi.dnsIP(), result.dns);
	return result;
}

bool WifiAdapter::startScan(ports::WifiScanSnapshot &snapshot, const std::uint32_t nowMs) noexcept
{
	WiFi.scanDelete();
	snapshot.resultCount = 0;
	snapshot.startedAtMs = nowMs;
	snapshot.state = ports::WifiScanState::Scanning;
	++snapshot.sequence;
	if (WiFi.scanNetworks(true, true) != WIFI_SCAN_FAILED)
	{
		return true;
	}
	snapshot.state = ports::WifiScanState::Failed;
	++snapshot.sequence;
	return false;
}

void WifiAdapter::updateScan(ports::WifiScanSnapshot &snapshot) noexcept
{
	if (snapshot.state != ports::WifiScanState::Scanning)
	{
		return;
	}
	const auto scanResult = WiFi.scanComplete();
	if (scanResult == WIFI_SCAN_RUNNING)
	{
		return;
	}
	if (scanResult == WIFI_SCAN_FAILED)
	{
		snapshot.state = ports::WifiScanState::Failed;
		++snapshot.sequence;
		return;
	}
	const auto resultCount = std::min<std::size_t>(static_cast<std::size_t>(scanResult), snapshot.results.size());
	for (std::size_t index = 0; index < resultCount; ++index)
	{
		auto &destination = snapshot.results[index];
		const auto ssid = WiFi.SSID(static_cast<std::int32_t>(index));
		std::snprintf(destination.ssid.data(), destination.ssid.size(), "%s", ssid.c_str());
		destination.rssi = WiFi.RSSI(static_cast<std::int32_t>(index));
		destination.channel = static_cast<std::uint8_t>(WiFi.channel(static_cast<std::int32_t>(index)));
		destination.secured = WiFi.encryptionType(static_cast<std::int32_t>(index)) != WIFI_AUTH_OPEN;
	}
	snapshot.resultCount = static_cast<std::uint8_t>(resultCount);
	snapshot.state = ports::WifiScanState::Complete;
	++snapshot.sequence;
	WiFi.scanDelete();
}

bool WifiAdapter::startRecoveryAp(const char *const ssid,
	const char *const password,
	const std::uint8_t channel) noexcept
{
	WiFi.mode(WIFI_AP_STA);
	return WiFi.softAP(ssid, password, channel);
}

void WifiAdapter::stopRecoveryAp() noexcept
{
	WiFi.softAPdisconnect(true);
	WiFi.mode(WIFI_STA);
}
}