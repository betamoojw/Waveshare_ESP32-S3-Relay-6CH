#include "littlefs_configuration_source.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include <algorithm>
#include <array>
#include <cstdio>

namespace switch_actuator::adapters::filesystem
{
namespace
{
constexpr std::string_view configurationDirectory{"/config"};
constexpr std::string_view backupDirectory{"/config/.backup"};
constexpr std::string_view stagingDirectory{"/config/.staging"};
constexpr std::size_t copyBufferSize{256};
constexpr std::size_t maximumPathBytes{64};
constexpr std::array<std::string_view, 7> sectionFileNames{
	"system.json",
	"network.json",
	"wifi.json",
	"ethernet.json",
	"knx.json",
	"modbus.json",
	"ui.json",
};

[[nodiscard]] bool makePath(const std::string_view directory,
	const std::string_view fileName,
	std::array<char, maximumPathBytes> &path) noexcept
{
	const auto length = std::snprintf(path.data(), path.size(), "%.*s/%.*s",
		static_cast<int>(directory.size()),
		directory.data(),
		static_cast<int>(fileName.size()),
		fileName.data());
	return length > 0 && static_cast<std::size_t>(length) < path.size();
}

[[nodiscard]] bool readObject(const char *const path,
	const std::size_t maximumBytes,
	JsonDocument &document) noexcept
{
	auto file = LittleFS.open(path, FILE_READ);
	if (!file || file.size() == 0 || file.size() > maximumBytes)
	{
		return false;
	}
	return deserializeJson(document, file) == DeserializationError::Ok && document.is<JsonObject>();
}

[[nodiscard]] const char *parityText(const domain::SerialParity parity) noexcept
{
	return parity == domain::SerialParity::Even ? "even" : parity == domain::SerialParity::Odd ? "odd" : "none";
}

[[nodiscard]] const char *restorePolicyText(const domain::RestorePolicy policy) noexcept
{
	return policy == domain::RestorePolicy::LastKnown ? "lastKnown" :
		policy == domain::RestorePolicy::ConfiguredDefault ? "configuredDefault" : "allOff";
}

void formatUuid(const std::array<std::uint8_t, domain::deviceUuidSize> &uuid,
	std::array<char, 37> &output) noexcept
{
	std::snprintf(output.data(), output.size(),
		"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
		uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
}
}

LittleFsConfigurationSource::LittleFsConfigurationSource(const std::string_view embeddedFallback) noexcept
	: embeddedFallback_{embeddedFallback}
{
}

LittleFsInitializeResult LittleFsConfigurationSource::initialize() noexcept
{
	mounted_ = LittleFS.begin(false);
	return mounted_ ? LittleFsInitializeResult::Initialized : LittleFsInitializeResult::MountFailure;
}

ports::ConfigurationSource LittleFsConfigurationSource::port() noexcept
{
	return {loadCallback, this};
}

ports::ConfigurationFilePort LittleFsConfigurationSource::filePort() noexcept
{
	return {ports::ConfigurationSource{loadFileCallback, this}, storeCallback, this};
}

ports::ConfigurationSourceResult LittleFsConfigurationSource::loadCallback(void *const context,
	domain::Configuration &configuration) noexcept
{
	return context != nullptr ? static_cast<LittleFsConfigurationSource *>(context)->load(configuration)
								  : ports::ConfigurationSourceResult::Unavailable;
}

ports::ConfigurationSourceResult LittleFsConfigurationSource::loadFileCallback(void *const context,
	domain::Configuration &configuration) noexcept
{
	return context != nullptr ? static_cast<LittleFsConfigurationSource *>(context)->loadPrimary(configuration)
								  : ports::ConfigurationSourceResult::Unavailable;
}

ports::ConfigurationFileStoreResult LittleFsConfigurationSource::storeCallback(void *const context,
	const domain::Configuration &configuration) noexcept
{
	return context != nullptr ? static_cast<LittleFsConfigurationSource *>(context)->store(configuration)
								  : ports::ConfigurationFileStoreResult::Unavailable;
}

ports::ConfigurationSourceResult LittleFsConfigurationSource::load(domain::Configuration &configuration) noexcept
{
	if (mounted_)
	{
		const auto primaryResult = loadBundle(configurationDirectory, configuration);
		if (primaryResult == ports::ConfigurationSourceResult::Loaded)
		{
			static_cast<void>(backupBundle());
			return primaryResult;
		}

		domain::Configuration backupConfiguration{};
		if (loadBundle(backupDirectory, backupConfiguration) == ports::ConfigurationSourceResult::Loaded)
		{
			configuration = backupConfiguration;
			static_cast<void>(restoreBundle());
			return ports::ConfigurationSourceResult::Loaded;
		}
	}

	return embeddedFallback_.port().load(configuration);
}

ports::ConfigurationSourceResult LittleFsConfigurationSource::loadPrimary(
	domain::Configuration &configuration) noexcept
{
	return mounted_ ? loadBundle(configurationDirectory, configuration)
					 : ports::ConfigurationSourceResult::Unavailable;
}

ports::ConfigurationFileStoreResult LittleFsConfigurationSource::store(
	const domain::Configuration &configuration) noexcept
{
	if (!mounted_)
	{
		return ports::ConfigurationFileStoreResult::Unavailable;
	}
	if (domain::validateConfiguration(configuration) != domain::ConfigurationValidationError::None)
	{
		return ports::ConfigurationFileStoreResult::InvalidConfiguration;
	}
	if ((!LittleFS.exists(configurationDirectory.data()) && !LittleFS.mkdir(configurationDirectory.data())) ||
		(!LittleFS.exists(stagingDirectory.data()) && !LittleFS.mkdir(stagingDirectory.data())))
	{
		return ports::ConfigurationFileStoreResult::IoFailure;
	}

	std::array<char, maximumPathBytes> path{};
	JsonDocument system;
	system["schemaVersion"] = domain::currentConfigurationSchemaVersion;
	auto identity = system["identity"].to<JsonObject>();
	identity["productId"] = configuration.productId.value.data();
	identity["boardModel"] = configuration.boardModel.data();
	identity["hardwareRevision"] = configuration.hardwareRevision.data();
	identity["deviceSerial"] = configuration.deviceSerial.data();
	std::array<char, 37> uuid{};
	formatUuid(configuration.deviceUuid, uuid);
	identity["deviceUuid"] = uuid.data();
	identity["manufacturingDate"] = configuration.manufacturingDate.iso8601.data();
	identity["manufacturingBatch"] = configuration.manufacturingBatch;
	auto relays = system["relayChannels"].to<JsonArray>();
	for (const auto &relay : configuration.relayChannels)
	{
		auto object = relays.add<JsonObject>();
		object["enabled"] = relay.enabled;
		object["restorePolicy"] = restorePolicyText(relay.restorePolicy);
		object["configuredDefault"] = relay.configuredDefault == domain::RelayState::On ? "on" : "off";
	}
	auto indicators = system["indicators"].to<JsonObject>();
	indicators["maximumBrightness"] = configuration.indicators.maximumBrightness;
	indicators["maximumBuzzerDutyPercent"] = configuration.indicators.maximumBuzzerDutyPercent;
	if (!makePath(stagingDirectory, sectionFileNames[0], path) || !writeDocumentAtomically(path.data(), system))
	{
		return ports::ConfigurationFileStoreResult::IoFailure;
	}

	JsonDocument network;
	network["enabled"] = configuration.network.enabled;
	network["hostName"] = configuration.network.hostName.data();
	auto recoveryAp = network["recoveryAp"].to<JsonObject>();
	recoveryAp["enabled"] = configuration.network.recoveryAp.enabled;
	recoveryAp["ssidPrefix"] = configuration.network.recoveryAp.ssidPrefix.data();
	recoveryAp["channel"] = configuration.network.recoveryAp.channel;
	recoveryAp["timeoutMs"] = configuration.network.recoveryAp.timeoutMs;
	recoveryAp["remainActiveWhileOffline"] = configuration.network.recoveryAp.remainActiveWhileOffline;
	if (!makePath(stagingDirectory, sectionFileNames[1], path) || !writeDocumentAtomically(path.data(), network))
	{
		return ports::ConfigurationFileStoreResult::IoFailure;
	}

	JsonDocument wifi;
	auto profiles = wifi["profiles"].to<JsonArray>();
	for (const auto &profile : configuration.network.wifiProfiles)
	{
		auto object = profiles.add<JsonObject>();
		object["enabled"] = profile.enabled;
		if (profile.ssid.front() != '\0')
		{
			object["ssid"] = profile.ssid.data();
		}
		if (profile.passphrase.front() != '\0')
		{
			object["passphrase"] = profile.passphrase.data();
		}
		auto ipv4 = object["ipv4"].to<JsonObject>();
		ipv4["mode"] = profile.ipv4.mode == domain::IpMode::Static ? "static" : "dhcp";
		auto address = ipv4["address"].to<JsonArray>();
		auto subnetMask = ipv4["subnetMask"].to<JsonArray>();
		auto gateway = ipv4["gateway"].to<JsonArray>();
		auto dns = ipv4["dns"].to<JsonArray>();
		for (std::size_t index = 0; index < profile.ipv4.address.size(); ++index)
		{
			address.add(profile.ipv4.address[index]);
			subnetMask.add(profile.ipv4.subnetMask[index]);
			gateway.add(profile.ipv4.gateway[index]);
			dns.add(profile.ipv4.dns[index]);
		}
	}
	if (!makePath(stagingDirectory, sectionFileNames[2], path) || !writeDocumentAtomically(path.data(), wifi))
	{
		return ports::ConfigurationFileStoreResult::IoFailure;
	}

	JsonDocument ethernet;
	ethernet["enabled"] = false;
	ethernet["implementation"] = "none";
	if (!makePath(stagingDirectory, sectionFileNames[3], path) || !writeDocumentAtomically(path.data(), ethernet))
	{
		return ports::ConfigurationFileStoreResult::IoFailure;
	}

	JsonDocument knx;
	knx["enabled"] = configuration.knx.enabled;
	knx["individualAddress"] = configuration.knx.individualAddress;
	knx["startupTransmitDelayMs"] = configuration.knx.startupTransmitDelayMs;
	knx["minimumTelegramIntervalMs"] = configuration.knx.minimumTelegramIntervalMs;
	knx["cyclicStatusIntervalMs"] = configuration.knx.cyclicStatusIntervalMs;
	knx["heartbeatIntervalMs"] = configuration.knx.heartbeatIntervalMs;
	knx["readSwitchObject"] = configuration.knx.readSwitchObject;
	knx["heartbeatGroupAddress"] = configuration.knx.heartbeatGroupAddress;
	knx["centralSwitchGroupAddress"] = configuration.knx.centralSwitchGroupAddress;
	knx["centralOffGroupAddress"] = configuration.knx.centralOffGroupAddress;
	knx["deviceFaultGroupAddress"] = configuration.knx.deviceFaultGroupAddress;
	auto channels = knx["channels"].to<JsonArray>();
	for (const auto &channel : configuration.knx.channels)
	{
		auto object = channels.add<JsonObject>();
		object["switchGroupAddress"] = channel.switchGroupAddress;
		object["statusGroupAddress"] = channel.statusGroupAddress;
		object["faultGroupAddress"] = channel.faultGroupAddress;
		object["commandPolarityInverted"] = channel.commandPolarityInverted;
		object["statusPolarityInverted"] = channel.statusPolarityInverted;
		object["sendStatusAfterStartup"] = channel.sendStatusAfterStartup;
		object["participatesInCentralSwitch"] = channel.participatesInCentralSwitch;
		object["participatesInCentralOff"] = channel.participatesInCentralOff;
	}
	if (!makePath(stagingDirectory, sectionFileNames[4], path) || !writeDocumentAtomically(path.data(), knx))
	{
		return ports::ConfigurationFileStoreResult::IoFailure;
	}

	JsonDocument modbus;
	modbus["unitId"] = configuration.modbus.unitId;
	modbus["baudRate"] = configuration.modbus.baudRate;
	modbus["parity"] = parityText(configuration.modbus.parity);
	modbus["dataBits"] = configuration.modbus.dataBits;
	modbus["stopBits"] = configuration.modbus.stopBits;
	if (!makePath(stagingDirectory, sectionFileNames[5], path) || !writeDocumentAtomically(path.data(), modbus))
	{
		return ports::ConfigurationFileStoreResult::IoFailure;
	}

	JsonDocument ui;
	ui["enabled"] = configuration.web.enabled;
	ui["securityProvisioned"] = configuration.web.securityProvisioned;
	if (!makePath(stagingDirectory, sectionFileNames[6], path) || !writeDocumentAtomically(path.data(), ui))
	{
		return ports::ConfigurationFileStoreResult::IoFailure;
	}

	domain::Configuration verification{};
	if (loadBundle(stagingDirectory, verification) != ports::ConfigurationSourceResult::Loaded)
	{
		return ports::ConfigurationFileStoreResult::IoFailure;
	}
	domain::Configuration current{};
	if (loadBundle(configurationDirectory, current) == ports::ConfigurationSourceResult::Loaded && !backupBundle())
	{
		return ports::ConfigurationFileStoreResult::IoFailure;
	}
	for (const auto fileName : sectionFileNames)
	{
		std::array<char, maximumPathBytes> sourcePath{};
		std::array<char, maximumPathBytes> destinationPath{};
		if (!makePath(stagingDirectory, fileName, sourcePath) ||
			!makePath(configurationDirectory, fileName, destinationPath) ||
			!copyFileAtomically(sourcePath.data(), destinationPath.data()))
		{
			return ports::ConfigurationFileStoreResult::IoFailure;
		}
	}
	return ports::ConfigurationFileStoreResult::Stored;
}

ports::ConfigurationSourceResult LittleFsConfigurationSource::loadBundle(const std::string_view directory,
	domain::Configuration &configuration) noexcept
{
	std::array<char, maximumPathBytes> path{};
	JsonDocument system;
	if (!makePath(directory, sectionFileNames[0], path) || !readObject(path.data(), buffer_.size(), system))
	{
		return ports::ConfigurationSourceResult::Unavailable;
	}

	JsonDocument aggregate;
	aggregate["schemaVersion"] = system["schemaVersion"];
	aggregate["identity"] = system["identity"];
	aggregate["relayChannels"] = system["relayChannels"];
	aggregate["indicators"] = system["indicators"];

	JsonDocument network;
	if (!makePath(directory, sectionFileNames[1], path) || !readObject(path.data(), buffer_.size(), network))
	{
		return ports::ConfigurationSourceResult::Unavailable;
	}
	aggregate["network"] = network.as<JsonObjectConst>();

	JsonDocument wifi;
	if (!makePath(directory, sectionFileNames[2], path) || !readObject(path.data(), buffer_.size(), wifi) ||
		!wifi["profiles"].is<JsonArray>())
	{
		return ports::ConfigurationSourceResult::Invalid;
	}
	aggregate["network"]["wifiProfiles"] = wifi["profiles"];

	JsonDocument ethernet;
	if (!makePath(directory, sectionFileNames[3], path) || !readObject(path.data(), buffer_.size(), ethernet) ||
		!ethernet["enabled"].is<bool>() || ethernet["enabled"].as<bool>())
	{
		return ports::ConfigurationSourceResult::Invalid;
	}

	JsonDocument section;
	for (std::size_t index = 4; index < sectionFileNames.size(); ++index)
	{
		section.clear();
		if (!makePath(directory, sectionFileNames[index], path) || !readObject(path.data(), buffer_.size(), section))
		{
			return ports::ConfigurationSourceResult::Unavailable;
		}
		constexpr std::array<std::string_view, 3> keys{"knx", "modbus", "web"};
		aggregate[keys[index - 4].data()] = section.as<JsonObjectConst>();
	}

	const auto serializedSize = measureJson(aggregate);
	if (serializedSize == 0 || serializedSize > buffer_.size() ||
		serializeJson(aggregate, buffer_.data(), buffer_.size()) != serializedSize)
	{
		return ports::ConfigurationSourceResult::Invalid;
	}

	configuration::JsonConfigurationSource source{{buffer_.data(), serializedSize}};
	return source.port().load(configuration);
}

bool LittleFsConfigurationSource::backupBundle() noexcept
{
	if (!LittleFS.exists(backupDirectory.data()) && !LittleFS.mkdir(backupDirectory.data()))
	{
		return false;
	}
	for (const auto fileName : sectionFileNames)
	{
		std::array<char, maximumPathBytes> sourcePath{};
		std::array<char, maximumPathBytes> destinationPath{};
		if (!makePath(configurationDirectory, fileName, sourcePath) ||
			!makePath(backupDirectory, fileName, destinationPath) ||
			!copyFileAtomically(sourcePath.data(), destinationPath.data()))
		{
			return false;
		}
	}
	return true;
}

bool LittleFsConfigurationSource::restoreBundle() noexcept
{
	for (const auto fileName : sectionFileNames)
	{
		std::array<char, maximumPathBytes> sourcePath{};
		std::array<char, maximumPathBytes> destinationPath{};
		if (!makePath(backupDirectory, fileName, sourcePath) ||
			!makePath(configurationDirectory, fileName, destinationPath) ||
			!copyFileAtomically(sourcePath.data(), destinationPath.data()))
		{
			return false;
		}
	}
	return true;
}

bool LittleFsConfigurationSource::copyFileAtomically(const std::string_view sourcePath,
	const std::string_view destinationPath) noexcept
{
	auto source = LittleFS.open(sourcePath.data(), FILE_READ);
	if (!source)
	{
		return false;
	}

	std::array<char, maximumPathBytes> temporaryPath{};
	const auto temporaryLength = std::snprintf(temporaryPath.data(), temporaryPath.size(), "%.*s.tmp",
		static_cast<int>(destinationPath.size()), destinationPath.data());
	if (temporaryLength <= 0 || static_cast<std::size_t>(temporaryLength) >= temporaryPath.size())
	{
		return false;
	}
	LittleFS.remove(temporaryPath.data());
	auto temporary = LittleFS.open(temporaryPath.data(), FILE_WRITE);
	if (!temporary)
	{
		return false;
	}

	std::array<std::uint8_t, copyBufferSize> copyBuffer{};
	std::size_t bytesWritten{0};
	while (source.available() > 0)
	{
		const auto bytesRead = source.read(copyBuffer.data(), copyBuffer.size());
		if (bytesRead == 0 || temporary.write(copyBuffer.data(), bytesRead) != bytesRead)
		{
			temporary.close();
			LittleFS.remove(temporaryPath.data());
			return false;
		}
		bytesWritten += bytesRead;
	}
	temporary.close();
	if (bytesWritten != source.size())
	{
		LittleFS.remove(temporaryPath.data());
		return false;
	}

	LittleFS.remove(destinationPath.data());
	if (!LittleFS.rename(temporaryPath.data(), destinationPath.data()))
	{
		LittleFS.remove(temporaryPath.data());
		return false;
	}
	return true;
}

bool LittleFsConfigurationSource::writeDocumentAtomically(const std::string_view path,
	const JsonDocument &document) noexcept
{
	std::array<char, maximumPathBytes> temporaryPath{};
	const auto temporaryLength = std::snprintf(temporaryPath.data(), temporaryPath.size(), "%.*s.tmp",
		static_cast<int>(path.size()), path.data());
	if (temporaryLength <= 0 || static_cast<std::size_t>(temporaryLength) >= temporaryPath.size())
	{
		return false;
	}
	LittleFS.remove(temporaryPath.data());
	auto file = LittleFS.open(temporaryPath.data(), FILE_WRITE);
	if (!file || serializeJson(document, file) == 0)
	{
		file.close();
		LittleFS.remove(temporaryPath.data());
		return false;
	}
	file.close();
	LittleFS.remove(path.data());
	if (!LittleFS.rename(temporaryPath.data(), path.data()))
	{
		LittleFS.remove(temporaryPath.data());
		return false;
	}
	return true;
}
}