#include "configuration.h"

#include <algorithm>

namespace switch_actuator::domain
{
namespace
{
template <std::size_t Capacity>
[[nodiscard]] bool hasBoundedText(const std::array<char, Capacity> &text) noexcept
{
	return text.front() != '\0' && std::find(text.begin(), text.end(), '\0') != text.end();
}

[[nodiscard]] bool hasProvisionedUuid(const std::array<std::uint8_t, deviceUuidSize> &uuid) noexcept
{
	return std::any_of(uuid.begin(), uuid.end(), [](const auto byte) { return byte != 0; });
}

[[nodiscard]] bool isSupportedBaudRate(const std::uint32_t baudRate) noexcept
{
	constexpr std::array<std::uint32_t, 5> supportedBaudRates{9600, 19200, 38400, 57600, 115200};
	return std::find(supportedBaudRates.begin(), supportedBaudRates.end(), baudRate) != supportedBaudRates.end();
}

[[nodiscard]] bool isValid(const SerialParity parity) noexcept
{
	return parity == SerialParity::None || parity == SerialParity::Even || parity == SerialParity::Odd;
}

[[nodiscard]] bool isValid(const RestorePolicy policy) noexcept
{
	return policy == RestorePolicy::AllOff || policy == RestorePolicy::LastKnown || policy == RestorePolicy::ConfiguredDefault;
}

[[nodiscard]] bool isValid(const RelayState state) noexcept
{
	return state == RelayState::Off || state == RelayState::On;
}
}

ConfigurationValidationError validateConfiguration(const Configuration &configuration) noexcept
{
	if (configuration.schemaVersion != currentConfigurationSchemaVersion)
	{
		return ConfigurationValidationError::UnsupportedSchema;
	}
	if (!hasBoundedText(configuration.boardModel))
	{
		return ConfigurationValidationError::MissingBoardModel;
	}
	if (!hasBoundedText(configuration.hardwareRevision))
	{
		return ConfigurationValidationError::MissingHardwareRevision;
	}
	if (!hasBoundedText(configuration.deviceSerial))
	{
		return ConfigurationValidationError::MissingDeviceSerial;
	}
	if (!hasProvisionedUuid(configuration.deviceUuid))
	{
		return ConfigurationValidationError::MissingDeviceUuid;
	}
	if (configuration.modbus.unitId < 1 || configuration.modbus.unitId > 247)
	{
		return ConfigurationValidationError::InvalidModbusUnitId;
	}
	if (!isSupportedBaudRate(configuration.modbus.baudRate))
	{
		return ConfigurationValidationError::UnsupportedBaudRate;
	}
	if (!isValid(configuration.modbus.parity) || (configuration.modbus.dataBits != 7 && configuration.modbus.dataBits != 8) ||
		(configuration.modbus.stopBits != 1 && configuration.modbus.stopBits != 2))
	{
		return ConfigurationValidationError::InvalidSerialFormat;
	}
	if (std::any_of(configuration.relayChannels.begin(), configuration.relayChannels.end(), [](const auto &channel) {
			return !isValid(channel.restorePolicy) || !isValid(channel.configuredDefault) ||
				   (channel.restorePolicy == RestorePolicy::ConfiguredDefault && !channel.enabled && channel.configuredDefault == RelayState::On);
		}))
	{
		return ConfigurationValidationError::InvalidRelayConfiguration;
	}
	if (configuration.knx.enabled && configuration.knx.individualAddress == 0)
	{
		return ConfigurationValidationError::InvalidKnxConfiguration;
	}
	if (configuration.web.enabled && !configuration.web.securityProvisioned)
	{
		return ConfigurationValidationError::MissingWebSecurity;
	}
	if (configuration.indicators.maximumBuzzerDutyPercent > 10)
	{
		return ConfigurationValidationError::InvalidIndicatorPolicy;
	}

	return ConfigurationValidationError::None;
}

Configuration makeSafeConfiguration() noexcept
{
	Configuration configuration{};
	for (auto &channel : configuration.relayChannels)
	{
		channel.enabled = true;
		channel.restorePolicy = RestorePolicy::AllOff;
		channel.configuredDefault = RelayState::Off;
	}
	configuration.knx.enabled = false;
	configuration.web.enabled = false;
	configuration.indicators.maximumBrightness = 32;
	configuration.indicators.maximumBuzzerDutyPercent = 0;
	return configuration;
}
}