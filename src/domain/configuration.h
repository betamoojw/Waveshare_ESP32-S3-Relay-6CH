#pragma once

#include "relay_types.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace switch_actuator::domain
{
inline constexpr std::uint16_t currentConfigurationSchemaVersion{1};
inline constexpr std::size_t boardModelCapacity{32};
inline constexpr std::size_t hardwareRevisionCapacity{16};
inline constexpr std::size_t deviceSerialCapacity{32};
inline constexpr std::size_t deviceUuidSize{16};

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

struct KnxConfiguration final
{
	bool enabled{false};
	std::uint16_t individualAddress{0};
	std::array<std::uint16_t, relayChannelCount> switchGroupAddresses{};
	std::array<std::uint16_t, relayChannelCount> statusGroupAddresses{};
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
	MissingWebSecurity,
	InvalidIndicatorPolicy
};

[[nodiscard]] ConfigurationValidationError validateConfiguration(const Configuration &configuration) noexcept;
[[nodiscard]] Configuration makeSafeConfiguration() noexcept;
}