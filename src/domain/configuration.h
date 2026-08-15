#pragma once

#include "relay_types.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace switch_actuator::domain
{
inline constexpr std::uint16_t currentConfigurationSchemaVersion{3};
inline constexpr std::size_t boardModelCapacity{32};
inline constexpr std::size_t hardwareRevisionCapacity{16};
inline constexpr std::size_t deviceSerialCapacity{32};
inline constexpr std::size_t deviceUuidSize{16};
inline constexpr std::size_t networkHostNameCapacity{32};
inline constexpr std::size_t wifiSsidCapacity{33};
inline constexpr std::size_t wifiPassphraseCapacity{64};
inline constexpr std::size_t recoveryApPrefixCapacity{24};
inline constexpr std::size_t wifiProfileCount{3};

enum class IpMode : std::uint8_t { Dhcp, Static };

struct Ipv4Configuration final
{
	IpMode mode{IpMode::Dhcp};
	std::array<std::uint8_t, 4> address{};
	std::array<std::uint8_t, 4> subnetMask{};
	std::array<std::uint8_t, 4> gateway{};
	std::array<std::uint8_t, 4> dns{};
};

struct WifiProfile final
{
	bool enabled{false};
	std::array<char, wifiSsidCapacity> ssid{};
	std::array<char, wifiPassphraseCapacity> passphrase{};
	Ipv4Configuration ipv4{};
};

struct RecoveryApConfiguration final
{
	bool enabled{true};
	std::array<char, recoveryApPrefixCapacity> ssidPrefix{"Relay"};
	std::uint8_t channel{1};
	std::uint32_t timeoutMs{0};
	bool remainActiveWhileOffline{true};
};

struct NetworkConfiguration final
{
	bool enabled{true};
	std::array<char, networkHostNameCapacity> hostName{"relay-actuator"};
	std::array<WifiProfile, wifiProfileCount> wifiProfiles{};
	RecoveryApConfiguration recoveryAp{};
};

enum class RestorePolicy : std::uint8_t
{
	AllOff,
	LastKnown,
	ConfiguredDefault
};

enum class SerialParity : std::uint8_t
{
	None,
	Even,
	Odd
};

struct ModbusConfiguration final
{
	std::uint8_t unitId{10};
	std::uint32_t baudRate{115200};
	SerialParity parity{SerialParity::None};
	std::uint8_t dataBits{8};
	std::uint8_t stopBits{1};
};

struct RelayChannelConfiguration final
{
	bool enabled{true};
	RestorePolicy restorePolicy{RestorePolicy::AllOff};
	RelayState configuredDefault{RelayState::Off};
};

struct KnxChannelConfiguration final
{
	std::uint16_t switchGroupAddress{0};
	std::uint16_t statusGroupAddress{0};
	std::uint16_t faultGroupAddress{0};
	bool commandPolarityInverted{false};
	bool statusPolarityInverted{false};
	bool sendStatusAfterStartup{true};
	bool participatesInCentralSwitch{true};
	bool participatesInCentralOff{true};
};

struct KnxConfiguration final
{
	bool enabled{false};
	std::uint16_t individualAddress{0};
	std::uint32_t startupTransmitDelayMs{3000};
	std::uint16_t minimumTelegramIntervalMs{100};
	std::uint32_t cyclicStatusIntervalMs{0};
	std::uint32_t heartbeatIntervalMs{0};
	bool readSwitchObject{false};
	std::uint16_t heartbeatGroupAddress{0};
	std::uint16_t centralSwitchGroupAddress{0};
	std::uint16_t centralOffGroupAddress{0};
	std::uint16_t deviceFaultGroupAddress{0};
	std::array<KnxChannelConfiguration, relayChannelCount> channels{};
};

struct WebConfiguration final
{
	bool enabled{false};
	bool securityProvisioned{false};
};

struct IndicatorConfiguration final
{
	std::uint8_t maximumBrightness{96};
	std::uint8_t maximumBuzzerDutyPercent{10};
};

struct Configuration final
{
	std::uint16_t schemaVersion{currentConfigurationSchemaVersion};
	std::uint32_t generation{0};
	std::array<char, boardModelCapacity> boardModel{};
	std::array<char, hardwareRevisionCapacity> hardwareRevision{};
	std::array<char, deviceSerialCapacity> deviceSerial{};
	std::array<std::uint8_t, deviceUuidSize> deviceUuid{};
	ModbusConfiguration modbus{};
	std::array<RelayChannelConfiguration, relayChannelCount> relayChannels{};
	KnxConfiguration knx{};
	NetworkConfiguration network{};
	WebConfiguration web{};
	IndicatorConfiguration indicators{};
};

enum class ConfigurationValidationError : std::uint8_t
{
	None,
	UnsupportedSchema,
	MissingBoardModel,
	MissingHardwareRevision,
	MissingDeviceSerial,
	MissingDeviceUuid,
	InvalidModbusUnitId,
	UnsupportedBaudRate,
	InvalidSerialFormat,
	InvalidRelayConfiguration,
	InvalidKnxConfiguration,
	InvalidNetworkConfiguration,
	MissingWebSecurity,
	InvalidIndicatorPolicy
};

[[nodiscard]] ConfigurationValidationError validateConfiguration(const Configuration &configuration) noexcept;
[[nodiscard]] Configuration makeSafeConfiguration() noexcept;
}