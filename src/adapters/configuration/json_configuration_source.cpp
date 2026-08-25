#include "json_configuration_source.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace switch_actuator::adapters::configuration
{
extern const char defaultConfigurationJsonStart[] asm("_binary_config_default_configuration_json_start");
extern const char defaultConfigurationJsonEnd[] asm("_binary_config_default_configuration_json_end");

namespace
{
template <std::size_t Capacity>
[[nodiscard]] bool readText(const JsonVariantConst value, std::array<char, Capacity> &output) noexcept
{
	if (!value.is<const char *>())
	{
		return false;
	}
	const auto *text = value.as<const char *>();
	const auto length = text == nullptr ? 0 : std::strlen(text);
	if (length == 0 || length >= Capacity)
	{
		return false;
	}
	output.fill('\0');
	std::copy_n(text, length, output.begin());
	return true;
}

[[nodiscard]] int hexNibble(const char value) noexcept
{
	if (value >= '0' && value <= '9')
	{
		return value - '0';
	}
	if (value >= 'a' && value <= 'f')
	{
		return value - 'a' + 10;
	}
	if (value >= 'A' && value <= 'F')
	{
		return value - 'A' + 10;
	}
	return -1;
}

[[nodiscard]] bool readUuid(const JsonVariantConst value,
															std::array<std::uint8_t, domain::deviceUuidSize> &output) noexcept
{
	if (!value.is<const char *>())
	{
		return false;
	}
	const auto *text = value.as<const char *>();
	if (text == nullptr || std::strlen(text) != 36 || text[8] != '-' || text[13] != '-' || text[18] != '-' || text[23] != '-')
	{
		return false;
	}
	std::size_t byteIndex{0};
	for (std::size_t index = 0; index < 36;)
	{
		if (text[index] == '-')
		{
			++index;
			continue;
		}
		if (index + 1 >= 36 || byteIndex >= output.size())
		{
			return false;
		}
		const auto high = hexNibble(text[index]);
		const auto low = hexNibble(text[index + 1]);
		if (high < 0 || low < 0)
		{
			return false;
		}
		output[byteIndex++] = static_cast<std::uint8_t>((high << 4) | low);
		index += 2;
	}
	return byteIndex == output.size();
}

[[nodiscard]] bool readParity(const JsonVariantConst value, domain::SerialParity &output) noexcept
{
	if (!value.is<const char *>())
	{
		return false;
	}
	const auto *text = value.as<const char *>();
	if (std::strcmp(text, "none") == 0)
	{
		output = domain::SerialParity::None;
		return true;
	}
	if (std::strcmp(text, "even") == 0)
	{
		output = domain::SerialParity::Even;
		return true;
	}
	if (std::strcmp(text, "odd") == 0)
	{
		output = domain::SerialParity::Odd;
		return true;
	}
	return false;
}

[[nodiscard]] bool readRestorePolicy(const JsonVariantConst value, domain::RestorePolicy &output) noexcept
{
	if (!value.is<const char *>())
	{
		return false;
	}
	const auto *text = value.as<const char *>();
	if (std::strcmp(text, "allOff") == 0)
	{
		output = domain::RestorePolicy::AllOff;
		return true;
	}
	if (std::strcmp(text, "lastKnown") == 0)
	{
		output = domain::RestorePolicy::LastKnown;
		return true;
	}
	if (std::strcmp(text, "configuredDefault") == 0)
	{
		output = domain::RestorePolicy::ConfiguredDefault;
		return true;
	}
	return false;
}

[[nodiscard]] bool readRelayState(const JsonVariantConst value, domain::RelayState &output) noexcept
{
	if (!value.is<const char *>())
	{
		return false;
	}
	const auto *text = value.as<const char *>();
	if (std::strcmp(text, "off") == 0)
	{
		output = domain::RelayState::Off;
		return true;
	}
	if (std::strcmp(text, "on") == 0)
	{
		output = domain::RelayState::On;
		return true;
	}
	return false;
}

template <std::size_t Capacity>
[[nodiscard]] bool readAddressArray(const JsonVariantConst value, std::array<std::uint16_t, Capacity> &output) noexcept
{
	if (!value.is<JsonArrayConst>())
	{
		return false;
	}
	const auto array = value.as<JsonArrayConst>();
	if (array.size() != Capacity)
	{
		return false;
	}
	for (std::size_t index = 0; index < Capacity; ++index)
	{
		if (!array[index].is<std::uint16_t>())
		{
			return false;
		}
		output[index] = array[index].as<std::uint16_t>();
	}
	return true;
}

[[nodiscard]] bool readKnxChannels(const JsonVariantConst value,
										std::array<domain::KnxChannelConfiguration, domain::relayChannelCount> &output) noexcept
{
	if (!value.is<JsonArrayConst>())
	{
		return false;
	}
	const auto channels = value.as<JsonArrayConst>();
	if (channels.size() != output.size())
	{
		return false;
	}
	for (std::size_t index = 0; index < output.size(); ++index)
	{
		const auto channel = channels[index];
		if (!channel.is<JsonObjectConst>() || !channel["switchGroupAddress"].is<std::uint16_t>() ||
			!channel["statusGroupAddress"].is<std::uint16_t>() || !channel["faultGroupAddress"].is<std::uint16_t>() ||
			!channel["commandPolarityInverted"].is<bool>() || !channel["statusPolarityInverted"].is<bool>() ||
			!channel["sendStatusAfterStartup"].is<bool>() || !channel["participatesInCentralSwitch"].is<bool>() ||
			!channel["participatesInCentralOff"].is<bool>())
		{
			return false;
		}
		auto &parsed = output[index];
		parsed.switchGroupAddress = channel["switchGroupAddress"].as<std::uint16_t>();
		parsed.statusGroupAddress = channel["statusGroupAddress"].as<std::uint16_t>();
		parsed.faultGroupAddress = channel["faultGroupAddress"].as<std::uint16_t>();
		parsed.commandPolarityInverted = channel["commandPolarityInverted"].as<bool>();
		parsed.statusPolarityInverted = channel["statusPolarityInverted"].as<bool>();
		parsed.sendStatusAfterStartup = channel["sendStatusAfterStartup"].as<bool>();
		parsed.participatesInCentralSwitch = channel["participatesInCentralSwitch"].as<bool>();
		parsed.participatesInCentralOff = channel["participatesInCentralOff"].as<bool>();
	}
	return true;
}

[[nodiscard]] bool readLegacyKnxChannels(const JsonObjectConst knx,
										  std::array<domain::KnxChannelConfiguration, domain::relayChannelCount> &output) noexcept
{
	std::array<std::uint16_t, domain::relayChannelCount> switchAddresses{};
	std::array<std::uint16_t, domain::relayChannelCount> statusAddresses{};
	if (!readAddressArray(knx["switchGroupAddresses"], switchAddresses) ||
		!readAddressArray(knx["statusGroupAddresses"], statusAddresses))
	{
		return false;
	}
	for (std::size_t index = 0; index < output.size(); ++index)
	{
		output[index].switchGroupAddress = switchAddresses[index];
		output[index].statusGroupAddress = statusAddresses[index];
	}
	return true;
}

template <std::size_t Capacity>
[[nodiscard]] bool readOptionalText(const JsonVariantConst value, std::array<char, Capacity> &output) noexcept
{
	if (value.isNull())
	{
		return true;
	}
	if (!value.is<const char *>())
	{
		return false;
	}
	const auto *text = value.as<const char *>();
	if (text == nullptr)
	{
		return false;
	}
	if (*text == '\0')
	{
		output.fill('\0');
		return true;
	}
	return readText(value, output);
}

[[nodiscard]] bool readIpv4Address(const JsonVariantConst value, std::array<std::uint8_t, 4> &output) noexcept
{
	if (!value.is<JsonArrayConst>() || value.size() != output.size())
	{
		return false;
	}
	for (std::size_t index = 0; index < output.size(); ++index)
	{
		if (!value[index].is<std::uint8_t>())
		{
			return false;
		}
		output[index] = value[index].as<std::uint8_t>();
	}
	return true;
}

[[nodiscard]] bool readIpv4Configuration(const JsonVariantConst value, domain::Ipv4Configuration &output) noexcept
{
	if (value.isNull())
	{
		return true;
	}
	if (!value.is<JsonObjectConst>() || !value["mode"].is<const char *>())
	{
		return false;
	}
	const auto *const mode = value["mode"].as<const char *>();
	if (std::strcmp(mode, "dhcp") == 0)
	{
		output.mode = domain::IpMode::Dhcp;
	}
	else if (std::strcmp(mode, "static") == 0)
	{
		output.mode = domain::IpMode::Static;
	}
	else
	{
		return false;
	}
	return readIpv4Address(value["address"], output.address) &&
		readIpv4Address(value["subnetMask"], output.subnetMask) &&
		readIpv4Address(value["gateway"], output.gateway) && readIpv4Address(value["dns"], output.dns);
}

[[nodiscard]] bool readNetworkConfiguration(const JsonObjectConst root, domain::NetworkConfiguration &network) noexcept
{
	const auto value = root["network"];
	if (value.isNull())
	{
		return true;
	}
	if (!value.is<JsonObjectConst>())
	{
		return false;
	}
	const auto object = value.as<JsonObjectConst>();
	const auto profiles = object["wifiProfiles"];
	const auto recoveryAp = object["recoveryAp"];
	if (!object["enabled"].is<bool>() || !readText(object["hostName"], network.hostName) ||
		!profiles.is<JsonArrayConst>() || profiles.as<JsonArrayConst>().size() != network.wifiProfiles.size() ||
		!recoveryAp.is<JsonObjectConst>())
	{
		return false;
	}
	network.enabled = object["enabled"].as<bool>();
	for (std::size_t index = 0; index < network.wifiProfiles.size(); ++index)
	{
		const auto source = profiles[index];
		auto &target = network.wifiProfiles[index];
		if (!source.is<JsonObjectConst>() || !source["enabled"].is<bool>() ||
			!readOptionalText(source["ssid"], target.ssid) || !readOptionalText(source["passphrase"], target.passphrase) ||
			!readIpv4Configuration(source["ipv4"], target.ipv4))
		{
			return false;
		}
		target.enabled = source["enabled"].as<bool>();
	}
	const auto recovery = recoveryAp.as<JsonObjectConst>();
	if (!recovery["enabled"].is<bool>() || !readText(recovery["ssidPrefix"], network.recoveryAp.ssidPrefix) ||
		!recovery["channel"].is<std::uint8_t>() || !recovery["timeoutMs"].is<std::uint32_t>() ||
		!recovery["remainActiveWhileOffline"].is<bool>())
	{
		return false;
	}
	network.recoveryAp.enabled = recovery["enabled"].as<bool>();
	network.recoveryAp.channel = recovery["channel"].as<std::uint8_t>();
	network.recoveryAp.timeoutMs = recovery["timeoutMs"].as<std::uint32_t>();
	network.recoveryAp.remainActiveWhileOffline = recovery["remainActiveWhileOffline"].as<bool>();
	return true;
}
}

ports::ConfigurationSource JsonConfigurationSource::port() noexcept
{
	return {loadCallback, this};
}

ports::ConfigurationSourceResult JsonConfigurationSource::loadCallback(void *const context,
																						 domain::Configuration &configuration) noexcept
{
	return context != nullptr ? static_cast<const JsonConfigurationSource *>(context)->load(configuration)
								  : ports::ConfigurationSourceResult::Unavailable;
}

ports::ConfigurationSourceResult JsonConfigurationSource::load(domain::Configuration &configuration) const noexcept
{
	if (json_.empty())
	{
		return ports::ConfigurationSourceResult::Unavailable;
	}
	JsonDocument document;
	if (deserializeJson(document, json_.data(), json_.size()) != DeserializationError::Ok || !document.is<JsonObject>())
	{
		return ports::ConfigurationSourceResult::Invalid;
	}

	const auto root = document.as<JsonObjectConst>();
	const auto identity = root["identity"];
	const auto modbus = root["modbus"];
	const auto relays = root["relayChannels"];
	const auto knx = root["knx"];
	const auto web = root["web"];
	const auto indicators = root["indicators"];
	if (!root["schemaVersion"].is<std::uint16_t>() || !identity.is<JsonObjectConst>() || !modbus.is<JsonObjectConst>() ||
		!relays.is<JsonArrayConst>() || !knx.is<JsonObjectConst>() || !web.is<JsonObjectConst>() ||
		!indicators.is<JsonObjectConst>())
	{
		return ports::ConfigurationSourceResult::Invalid;
	}

	domain::Configuration parsed{};
	const auto sourceSchemaVersion = root["schemaVersion"].as<std::uint16_t>();
	if (sourceSchemaVersion < 1 || sourceSchemaVersion > domain::currentConfigurationSchemaVersion)
	{
		return ports::ConfigurationSourceResult::Invalid;
	}
	parsed.schemaVersion = domain::currentConfigurationSchemaVersion;
	parsed.generation = 0;
	if ((sourceSchemaVersion >= 4 && (!readText(identity["productId"], parsed.productId.value) ||
			!readOptionalText(identity["manufacturingDate"], parsed.manufacturingDate.iso8601) ||
			!identity["manufacturingBatch"].is<std::uint32_t>())) ||
		!readText(identity["boardModel"], parsed.boardModel) ||
		!readText(identity["hardwareRevision"], parsed.hardwareRevision) ||
		!readText(identity["deviceSerial"], parsed.deviceSerial) || !readUuid(identity["deviceUuid"], parsed.deviceUuid) ||
		!modbus["unitId"].is<std::uint8_t>() || !modbus["baudRate"].is<std::uint32_t>() ||
		!readParity(modbus["parity"], parsed.modbus.parity) || !modbus["dataBits"].is<std::uint8_t>() ||
		!modbus["stopBits"].is<std::uint8_t>())
	{
		return ports::ConfigurationSourceResult::Invalid;
	}
	if (sourceSchemaVersion >= 4)
	{
		parsed.manufacturingBatch = identity["manufacturingBatch"].as<std::uint32_t>();
	}
	parsed.modbus.unitId = modbus["unitId"].as<std::uint8_t>();
	parsed.modbus.baudRate = modbus["baudRate"].as<std::uint32_t>();
	parsed.modbus.dataBits = modbus["dataBits"].as<std::uint8_t>();
	parsed.modbus.stopBits = modbus["stopBits"].as<std::uint8_t>();

	const auto relayArray = relays.as<JsonArrayConst>();
	if (relayArray.size() != parsed.relayChannels.size())
	{
		return ports::ConfigurationSourceResult::Invalid;
	}
	for (std::size_t index = 0; index < parsed.relayChannels.size(); ++index)
	{
		const auto relay = relayArray[index];
		if (!relay.is<JsonObjectConst>() || !relay["enabled"].is<bool>() ||
			!readRestorePolicy(relay["restorePolicy"], parsed.relayChannels[index].restorePolicy) ||
			!readRelayState(relay["configuredDefault"], parsed.relayChannels[index].configuredDefault))
		{
			return ports::ConfigurationSourceResult::Invalid;
		}
		parsed.relayChannels[index].enabled = relay["enabled"].as<bool>();
	}

	if (!knx["enabled"].is<bool>() || !knx["individualAddress"].is<std::uint16_t>() || !web["enabled"].is<bool>() ||
		!web["securityProvisioned"].is<bool>() || !indicators["maximumBrightness"].is<std::uint8_t>() ||
		!indicators["maximumBuzzerDutyPercent"].is<std::uint8_t>())
	{
		return ports::ConfigurationSourceResult::Invalid;
	}
	parsed.knx.enabled = knx["enabled"].as<bool>();
	parsed.knx.individualAddress = knx["individualAddress"].as<std::uint16_t>();
	if (sourceSchemaVersion == 1)
	{
		if (!readLegacyKnxChannels(knx.as<JsonObjectConst>(), parsed.knx.channels))
		{
			return ports::ConfigurationSourceResult::Invalid;
		}
	}
	else
	{
		if (!knx["startupTransmitDelayMs"].is<std::uint32_t>() ||
			!knx["minimumTelegramIntervalMs"].is<std::uint16_t>() ||
			!knx["cyclicStatusIntervalMs"].is<std::uint32_t>() || !knx["heartbeatIntervalMs"].is<std::uint32_t>() ||
			!knx["readSwitchObject"].is<bool>() || !knx["heartbeatGroupAddress"].is<std::uint16_t>() ||
			!knx["centralSwitchGroupAddress"].is<std::uint16_t>() ||
			!knx["centralOffGroupAddress"].is<std::uint16_t>() ||
			!knx["deviceFaultGroupAddress"].is<std::uint16_t>() || !readKnxChannels(knx["channels"], parsed.knx.channels))
		{
			return ports::ConfigurationSourceResult::Invalid;
		}
		parsed.knx.startupTransmitDelayMs = knx["startupTransmitDelayMs"].as<std::uint32_t>();
		parsed.knx.minimumTelegramIntervalMs = knx["minimumTelegramIntervalMs"].as<std::uint16_t>();
		parsed.knx.cyclicStatusIntervalMs = knx["cyclicStatusIntervalMs"].as<std::uint32_t>();
		parsed.knx.heartbeatIntervalMs = knx["heartbeatIntervalMs"].as<std::uint32_t>();
		parsed.knx.readSwitchObject = knx["readSwitchObject"].as<bool>();
		parsed.knx.heartbeatGroupAddress = knx["heartbeatGroupAddress"].as<std::uint16_t>();
		parsed.knx.centralSwitchGroupAddress = knx["centralSwitchGroupAddress"].as<std::uint16_t>();
		parsed.knx.centralOffGroupAddress = knx["centralOffGroupAddress"].as<std::uint16_t>();
		parsed.knx.deviceFaultGroupAddress = knx["deviceFaultGroupAddress"].as<std::uint16_t>();
	}
	if (!readNetworkConfiguration(root, parsed.network))
	{
		return ports::ConfigurationSourceResult::Invalid;
	}
	parsed.web.enabled = web["enabled"].as<bool>();
	parsed.web.securityProvisioned = web["securityProvisioned"].as<bool>();
	parsed.indicators.maximumBrightness = indicators["maximumBrightness"].as<std::uint8_t>();
	parsed.indicators.maximumBuzzerDutyPercent = indicators["maximumBuzzerDutyPercent"].as<std::uint8_t>();
	if (domain::validateConfiguration(parsed) != domain::ConfigurationValidationError::None)
	{
		return ports::ConfigurationSourceResult::Invalid;
	}

	configuration = parsed;
	return ports::ConfigurationSourceResult::Loaded;
}

std::string_view embeddedDefaultConfigurationJson() noexcept
{
	return {defaultConfigurationJsonStart,
		static_cast<std::size_t>(defaultConfigurationJsonEnd - defaultConfigurationJsonStart)};
}
}