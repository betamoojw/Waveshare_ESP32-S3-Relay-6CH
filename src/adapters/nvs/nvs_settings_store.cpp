#include "nvs_settings_store.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace switch_actuator::adapters::nvs
{
namespace
{
constexpr char settingsNamespace[]{"switch_cfg"};
constexpr char slotAKey[]{"cfg_a"};
constexpr char slotBKey[]{"cfg_b"};
constexpr char activeSlotKey[]{"active"};
constexpr char bootCountKey[]{"boot_count"};
constexpr char diagnosticSlotAKey[]{"diag_a"};
constexpr char diagnosticSlotBKey[]{"diag_b"};
constexpr char diagnosticActiveSlotKey[]{"diag_active"};
constexpr std::uint8_t slotA{0};
constexpr std::uint8_t slotB{1};
constexpr std::uint8_t noActiveSlot{0xFF};
constexpr std::uint32_t recordMagic{0x53414346};
constexpr std::size_t recordHeaderSize{16};
constexpr std::uint16_t legacyConfigurationSchemaVersion{1};
constexpr std::uint16_t previousConfigurationSchemaVersion{3};
constexpr std::size_t legacyConfigurationPayloadSize{159};
constexpr std::size_t previousConfigurationPayloadSize{633};
constexpr std::size_t configurationPayloadSize{672};
constexpr std::size_t legacyConfigurationRecordSize{recordHeaderSize + legacyConfigurationPayloadSize};
constexpr std::size_t previousConfigurationRecordSize{recordHeaderSize + previousConfigurationPayloadSize};
constexpr std::size_t configurationRecordSize{recordHeaderSize + configurationPayloadSize};
constexpr std::uint32_t diagnosticRecordMagic{0x53414443};
constexpr std::uint16_t diagnosticRecordVersion{1};
constexpr std::size_t diagnosticCounterCount{9};
constexpr std::size_t diagnosticPayloadSize{diagnosticCounterCount * sizeof(std::uint32_t)};
constexpr std::size_t diagnosticRecordSize{recordHeaderSize + diagnosticPayloadSize};

using Payload = std::array<std::uint8_t, configurationPayloadSize>;
using PreviousPayload = std::array<std::uint8_t, previousConfigurationPayloadSize>;
using LegacyPayload = std::array<std::uint8_t, legacyConfigurationPayloadSize>;
using Record = std::array<std::uint8_t, configurationRecordSize>;
using DiagnosticPayload = std::array<std::uint8_t, diagnosticPayloadSize>;
using DiagnosticRecord = std::array<std::uint8_t, diagnosticRecordSize>;

enum class SlotState : std::uint8_t
{
	Missing,
	Valid,
	Corrupt
};

struct SlotRecord final
{
	SlotState state{SlotState::Missing};
	domain::Configuration configuration{};
	Record encoded{};
	std::size_t encodedLength{0};
};

struct DiagnosticSlotRecord final
{
	SlotState state{SlotState::Missing};
	domain::PersistentDiagnosticCounters counters{};
	std::uint32_t generation{0};
	DiagnosticRecord encoded{};
};

class ByteWriter final
{
public:
	explicit ByteWriter(Payload &payload) noexcept
		: payload_{payload}
	{
	}

	void writeU8(const std::uint8_t value) noexcept
	{
		payload_[position_++] = value;
	}

	void writeU16(const std::uint16_t value) noexcept
	{
		writeU8(static_cast<std::uint8_t>(value));
		writeU8(static_cast<std::uint8_t>(value >> 8U));
	}

	void writeU32(const std::uint32_t value) noexcept
	{
		writeU16(static_cast<std::uint16_t>(value));
		writeU16(static_cast<std::uint16_t>(value >> 16U));
	}

	template <typename Value, std::size_t Size>
	void writeArray(const std::array<Value, Size> &values) noexcept
	{
		for (const auto value : values)
		{
			writeU8(static_cast<std::uint8_t>(value));
		}
	}

	[[nodiscard]] bool complete() const noexcept
	{
		return position_ == payload_.size();
	}

private:
	Payload &payload_;
	std::size_t position_{0};
};

template <std::size_t PayloadSize>
class ByteReader final
{
public:
	explicit ByteReader(const std::array<std::uint8_t, PayloadSize> &payload) noexcept
		: payload_{payload}
	{
	}

	[[nodiscard]] std::uint8_t readU8() noexcept
	{
		return payload_[position_++];
	}

	[[nodiscard]] std::uint16_t readU16() noexcept
	{
		const auto low = static_cast<std::uint16_t>(readU8());
		return static_cast<std::uint16_t>(low | static_cast<std::uint16_t>(readU8()) << 8U);
	}

	[[nodiscard]] std::uint32_t readU32() noexcept
	{
		const auto low = static_cast<std::uint32_t>(readU16());
		return low | static_cast<std::uint32_t>(readU16()) << 16U;
	}

	template <typename Array>
	void readArray(Array &values) noexcept
	{
		for (auto &value : values)
		{
			value = static_cast<typename Array::value_type>(readU8());
		}
	}

	[[nodiscard]] bool complete() const noexcept
	{
		return position_ == payload_.size();
	}

private:
	const std::array<std::uint8_t, PayloadSize> &payload_;
	std::size_t position_{0};
};

void writeU16(Record &record, const std::size_t offset, const std::uint16_t value) noexcept
{
	record[offset] = static_cast<std::uint8_t>(value);
	record[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}

void writeU32(Record &record, const std::size_t offset, const std::uint32_t value) noexcept
{
	writeU16(record, offset, static_cast<std::uint16_t>(value));
	writeU16(record, offset + 2, static_cast<std::uint16_t>(value >> 16U));
}

[[nodiscard]] std::uint16_t readU16(const Record &record, const std::size_t offset) noexcept
{
	return static_cast<std::uint16_t>(record[offset] | static_cast<std::uint16_t>(record[offset + 1]) << 8U);
}

[[nodiscard]] std::uint32_t readU32(const Record &record, const std::size_t offset) noexcept
{
	return static_cast<std::uint32_t>(readU16(record, offset)) | static_cast<std::uint32_t>(readU16(record, offset + 2)) << 16U;
}

template <std::size_t Size>
[[nodiscard]] std::uint32_t crc32(const std::array<std::uint8_t, Size> &payload) noexcept
{
	std::uint32_t crc{0xFFFFFFFF};
	for (const auto byte : payload)
	{
		crc ^= byte;
		for (std::uint8_t bit = 0; bit < 8; ++bit)
		{
			const auto mask = static_cast<std::uint32_t>(0U - (crc & 1U));
			crc = (crc >> 1U) ^ (0xEDB88320U & mask);
		}
	}
	return ~crc;
}

[[nodiscard]] bool encodePayload(const domain::Configuration &configuration, Payload &payload) noexcept
{
	ByteWriter writer{payload};
	writer.writeU16(configuration.schemaVersion);
	writer.writeU32(configuration.generation);
	writer.writeArray(configuration.productId.value);
	writer.writeArray(configuration.boardModel);
	writer.writeArray(configuration.hardwareRevision);
	writer.writeArray(configuration.deviceSerial);
	writer.writeArray(configuration.deviceUuid);
	writer.writeArray(configuration.manufacturingDate.iso8601);
	writer.writeU32(configuration.manufacturingBatch);
	writer.writeU8(configuration.modbus.unitId);
	writer.writeU32(configuration.modbus.baudRate);
	writer.writeU8(static_cast<std::uint8_t>(configuration.modbus.parity));
	writer.writeU8(configuration.modbus.dataBits);
	writer.writeU8(configuration.modbus.stopBits);
	for (const auto &channel : configuration.relayChannels)
	{
		writer.writeU8(channel.enabled ? 1 : 0);
		writer.writeU8(static_cast<std::uint8_t>(channel.restorePolicy));
		writer.writeU8(static_cast<std::uint8_t>(channel.configuredDefault));
	}
	writer.writeU8(configuration.knx.enabled ? 1 : 0);
	writer.writeU16(configuration.knx.individualAddress);
	writer.writeU32(configuration.knx.startupTransmitDelayMs);
	writer.writeU16(configuration.knx.minimumTelegramIntervalMs);
	writer.writeU32(configuration.knx.cyclicStatusIntervalMs);
	writer.writeU32(configuration.knx.heartbeatIntervalMs);
	writer.writeU8(configuration.knx.readSwitchObject ? 1 : 0);
	writer.writeU16(configuration.knx.heartbeatGroupAddress);
	writer.writeU16(configuration.knx.centralSwitchGroupAddress);
	writer.writeU16(configuration.knx.centralOffGroupAddress);
	writer.writeU16(configuration.knx.deviceFaultGroupAddress);
	for (const auto &channel : configuration.knx.channels)
	{
		writer.writeU16(channel.switchGroupAddress);
		writer.writeU16(channel.statusGroupAddress);
		writer.writeU16(channel.faultGroupAddress);
		writer.writeU8(channel.commandPolarityInverted ? 1 : 0);
		writer.writeU8(channel.statusPolarityInverted ? 1 : 0);
		writer.writeU8(channel.sendStatusAfterStartup ? 1 : 0);
		writer.writeU8(channel.participatesInCentralSwitch ? 1 : 0);
		writer.writeU8(channel.participatesInCentralOff ? 1 : 0);
	}
	writer.writeU8(configuration.network.enabled ? 1 : 0);
	writer.writeArray(configuration.network.hostName);
	for (const auto &profile : configuration.network.wifiProfiles)
	{
		writer.writeU8(profile.enabled ? 1 : 0);
		writer.writeArray(profile.ssid);
		writer.writeArray(profile.passphrase);
		writer.writeU8(static_cast<std::uint8_t>(profile.ipv4.mode));
		writer.writeArray(profile.ipv4.address);
		writer.writeArray(profile.ipv4.subnetMask);
		writer.writeArray(profile.ipv4.gateway);
		writer.writeArray(profile.ipv4.dns);
	}
	writer.writeU8(configuration.network.recoveryAp.enabled ? 1 : 0);
	writer.writeArray(configuration.network.recoveryAp.ssidPrefix);
	writer.writeU8(configuration.network.recoveryAp.channel);
	writer.writeU32(configuration.network.recoveryAp.timeoutMs);
	writer.writeU8(configuration.network.recoveryAp.remainActiveWhileOffline ? 1 : 0);
	writer.writeU8(configuration.web.enabled ? 1 : 0);
	writer.writeU8(configuration.web.securityProvisioned ? 1 : 0);
	writer.writeU8(configuration.indicators.maximumBrightness);
	writer.writeU8(configuration.indicators.maximumBuzzerDutyPercent);
	return writer.complete();
}

template <std::size_t PayloadSize>
[[nodiscard]] bool decodePayload(const std::array<std::uint8_t, PayloadSize> &payload,
	domain::Configuration &configuration,
	const bool hasProductionIdentity) noexcept
{
	ByteReader reader{payload};
	configuration = {};
	const auto storedSchemaVersion = reader.readU16();
	if (storedSchemaVersion != (hasProductionIdentity ? domain::currentConfigurationSchemaVersion : previousConfigurationSchemaVersion))
	{
		return false;
	}
	configuration.schemaVersion = domain::currentConfigurationSchemaVersion;
	configuration.generation = reader.readU32();
	if (hasProductionIdentity)
	{
		reader.readArray(configuration.productId.value);
	}
	reader.readArray(configuration.boardModel);
	reader.readArray(configuration.hardwareRevision);
	reader.readArray(configuration.deviceSerial);
	reader.readArray(configuration.deviceUuid);
	if (hasProductionIdentity)
	{
		reader.readArray(configuration.manufacturingDate.iso8601);
		configuration.manufacturingBatch = reader.readU32();
	}
	configuration.modbus.unitId = reader.readU8();
	configuration.modbus.baudRate = reader.readU32();
	configuration.modbus.parity = static_cast<domain::SerialParity>(reader.readU8());
	configuration.modbus.dataBits = reader.readU8();
	configuration.modbus.stopBits = reader.readU8();
	for (auto &channel : configuration.relayChannels)
	{
		const auto enabled = reader.readU8();
		if (enabled > 1)
		{
			return false;
		}
		channel.enabled = enabled != 0;
		channel.restorePolicy = static_cast<domain::RestorePolicy>(reader.readU8());
		channel.configuredDefault = static_cast<domain::RelayState>(reader.readU8());
	}
	const auto knxEnabled = reader.readU8();
	if (knxEnabled > 1)
	{
		return false;
	}
	configuration.knx.enabled = knxEnabled != 0;
	configuration.knx.individualAddress = reader.readU16();
	configuration.knx.startupTransmitDelayMs = reader.readU32();
	configuration.knx.minimumTelegramIntervalMs = reader.readU16();
	configuration.knx.cyclicStatusIntervalMs = reader.readU32();
	configuration.knx.heartbeatIntervalMs = reader.readU32();
	const auto readSwitchObject = reader.readU8();
	if (readSwitchObject > 1)
	{
		return false;
	}
	configuration.knx.readSwitchObject = readSwitchObject != 0;
	configuration.knx.heartbeatGroupAddress = reader.readU16();
	configuration.knx.centralSwitchGroupAddress = reader.readU16();
	configuration.knx.centralOffGroupAddress = reader.readU16();
	configuration.knx.deviceFaultGroupAddress = reader.readU16();
	for (auto &channel : configuration.knx.channels)
	{
		channel.switchGroupAddress = reader.readU16();
		channel.statusGroupAddress = reader.readU16();
		channel.faultGroupAddress = reader.readU16();
		const auto commandPolarityInverted = reader.readU8();
		const auto statusPolarityInverted = reader.readU8();
		const auto sendStatusAfterStartup = reader.readU8();
		const auto participatesInCentralSwitch = reader.readU8();
		const auto participatesInCentralOff = reader.readU8();
		if (commandPolarityInverted > 1 || statusPolarityInverted > 1 || sendStatusAfterStartup > 1 ||
			participatesInCentralSwitch > 1 || participatesInCentralOff > 1)
		{
			return false;
		}
		channel.commandPolarityInverted = commandPolarityInverted != 0;
		channel.statusPolarityInverted = statusPolarityInverted != 0;
		channel.sendStatusAfterStartup = sendStatusAfterStartup != 0;
		channel.participatesInCentralSwitch = participatesInCentralSwitch != 0;
		channel.participatesInCentralOff = participatesInCentralOff != 0;
	}
	const auto networkEnabled = reader.readU8();
	if (networkEnabled > 1)
	{
		return false;
	}
	configuration.network.enabled = networkEnabled != 0;
	reader.readArray(configuration.network.hostName);
	for (auto &profile : configuration.network.wifiProfiles)
	{
		const auto profileEnabled = reader.readU8();
		if (profileEnabled > 1)
		{
			return false;
		}
		profile.enabled = profileEnabled != 0;
		reader.readArray(profile.ssid);
		reader.readArray(profile.passphrase);
		profile.ipv4.mode = static_cast<domain::IpMode>(reader.readU8());
		reader.readArray(profile.ipv4.address);
		reader.readArray(profile.ipv4.subnetMask);
		reader.readArray(profile.ipv4.gateway);
		reader.readArray(profile.ipv4.dns);
	}
	const auto recoveryEnabled = reader.readU8();
	reader.readArray(configuration.network.recoveryAp.ssidPrefix);
	configuration.network.recoveryAp.channel = reader.readU8();
	configuration.network.recoveryAp.timeoutMs = reader.readU32();
	const auto remainActiveWhileOffline = reader.readU8();
	if (recoveryEnabled > 1 || remainActiveWhileOffline > 1)
	{
		return false;
	}
	configuration.network.recoveryAp.enabled = recoveryEnabled != 0;
	configuration.network.recoveryAp.remainActiveWhileOffline = remainActiveWhileOffline != 0;
	const auto webEnabled = reader.readU8();
	const auto securityProvisioned = reader.readU8();
	if (webEnabled > 1 || securityProvisioned > 1)
	{
		return false;
	}
	configuration.web.enabled = webEnabled != 0;
	configuration.web.securityProvisioned = securityProvisioned != 0;
	configuration.indicators.maximumBrightness = reader.readU8();
	configuration.indicators.maximumBuzzerDutyPercent = reader.readU8();
	return reader.complete() && domain::validateConfiguration(configuration) == domain::ConfigurationValidationError::None;
}

[[nodiscard]] bool decodeLegacyPayload(const LegacyPayload &payload, domain::Configuration &configuration) noexcept
{
	ByteReader reader{payload};
	configuration = {};
	if (reader.readU16() != legacyConfigurationSchemaVersion)
	{
		return false;
	}
	configuration.schemaVersion = domain::currentConfigurationSchemaVersion;
	configuration.generation = reader.readU32();
	reader.readArray(configuration.boardModel);
	reader.readArray(configuration.hardwareRevision);
	reader.readArray(configuration.deviceSerial);
	reader.readArray(configuration.deviceUuid);
	configuration.modbus.unitId = reader.readU8();
	configuration.modbus.baudRate = reader.readU32();
	configuration.modbus.parity = static_cast<domain::SerialParity>(reader.readU8());
	configuration.modbus.dataBits = reader.readU8();
	configuration.modbus.stopBits = reader.readU8();
	for (auto &channel : configuration.relayChannels)
	{
		const auto enabled = reader.readU8();
		if (enabled > 1)
		{
			return false;
		}
		channel.enabled = enabled != 0;
		channel.restorePolicy = static_cast<domain::RestorePolicy>(reader.readU8());
		channel.configuredDefault = static_cast<domain::RelayState>(reader.readU8());
	}
	const auto knxEnabled = reader.readU8();
	if (knxEnabled > 1)
	{
		return false;
	}
	configuration.knx.enabled = knxEnabled != 0;
	configuration.knx.individualAddress = reader.readU16();
	for (auto &channel : configuration.knx.channels)
	{
		channel.switchGroupAddress = reader.readU16();
	}
	for (auto &channel : configuration.knx.channels)
	{
		channel.statusGroupAddress = reader.readU16();
	}
	const auto webEnabled = reader.readU8();
	const auto securityProvisioned = reader.readU8();
	if (webEnabled > 1 || securityProvisioned > 1)
	{
		return false;
	}
	configuration.web.enabled = webEnabled != 0;
	configuration.web.securityProvisioned = securityProvisioned != 0;
	configuration.indicators.maximumBrightness = reader.readU8();
	configuration.indicators.maximumBuzzerDutyPercent = reader.readU8();
	return reader.complete() && domain::validateConfiguration(configuration) == domain::ConfigurationValidationError::None;
}

[[nodiscard]] bool encodeRecord(const domain::Configuration &configuration, Record &record) noexcept
{
	Payload payload{};
	if (!encodePayload(configuration, payload))
	{
		return false;
	}

	record.fill(0);
	writeU32(record, 0, recordMagic);
	writeU16(record, 4, configuration.schemaVersion);
	writeU16(record, 6, static_cast<std::uint16_t>(payload.size()));
	writeU32(record, 8, configuration.generation);
	writeU32(record, 12, crc32(payload));
	std::copy(payload.begin(), payload.end(), record.begin() + recordHeaderSize);
	return true;
}

[[nodiscard]] bool decodeRecord(const Record &record,
								const std::size_t recordLength,
								domain::Configuration &configuration) noexcept
{
	if (readU32(record, 0) != recordMagic || recordLength < recordHeaderSize)
	{
		return false;
	}
	const auto schemaVersion = readU16(record, 4);
	const auto payloadLength = readU16(record, 6);
	if (recordLength != recordHeaderSize + payloadLength)
	{
		return false;
	}
	if (schemaVersion == domain::currentConfigurationSchemaVersion && payloadLength == configurationPayloadSize)
	{
		Payload payload{};
		std::copy_n(record.begin() + recordHeaderSize, payload.size(), payload.begin());
		return crc32(payload) == readU32(record, 12) && decodePayload(payload, configuration, true) &&
			configuration.generation == readU32(record, 8);
	}
	if (schemaVersion == previousConfigurationSchemaVersion && payloadLength == previousConfigurationPayloadSize)
	{
		PreviousPayload payload{};
		std::copy_n(record.begin() + recordHeaderSize, payload.size(), payload.begin());
		return crc32(payload) == readU32(record, 12) && decodePayload(payload, configuration, false) &&
			configuration.generation == readU32(record, 8);
	}
	if (schemaVersion == legacyConfigurationSchemaVersion && payloadLength == legacyConfigurationPayloadSize)
	{
		LegacyPayload payload{};
		std::copy_n(record.begin() + recordHeaderSize, payload.size(), payload.begin());
		return crc32(payload) == readU32(record, 12) && decodeLegacyPayload(payload, configuration) &&
			configuration.generation == readU32(record, 8);
	}
	return false;
}

[[nodiscard]] const char *slotKey(const std::uint8_t slot) noexcept
{
	return slot == slotA ? slotAKey : slotBKey;
}

[[nodiscard]] SlotRecord readSlot(Preferences &preferences, const std::uint8_t slot) noexcept
{
	SlotRecord result{};
	const auto *key = slotKey(slot);
	const auto storedLength = preferences.getBytesLength(key);
	if (storedLength == 0)
	{
		return result;
	}
	if ((storedLength != legacyConfigurationRecordSize && storedLength != previousConfigurationRecordSize &&
			storedLength != configurationRecordSize) ||
		preferences.getBytes(key, result.encoded.data(), storedLength) != storedLength)
	{
		result.state = SlotState::Corrupt;
		return result;
	}

	result.encodedLength = storedLength;
	result.state = decodeRecord(result.encoded, storedLength, result.configuration) ? SlotState::Valid : SlotState::Corrupt;
	return result;
}

void writeDiagnosticU32(DiagnosticRecord &record, const std::size_t offset, const std::uint32_t value) noexcept
{
	for (std::size_t index = 0; index < sizeof(value); ++index)
	{
		record[offset + index] = static_cast<std::uint8_t>(value >> (index * 8U));
	}
}

[[nodiscard]] std::uint32_t readDiagnosticU32(const DiagnosticRecord &record, const std::size_t offset) noexcept
{
	std::uint32_t value{0};
	for (std::size_t index = 0; index < sizeof(value); ++index)
	{
		value |= static_cast<std::uint32_t>(record[offset + index]) << (index * 8U);
	}
	return value;
}

[[nodiscard]] bool diagnosticCountersEqual(const domain::PersistentDiagnosticCounters &left,
	const domain::PersistentDiagnosticCounters &right) noexcept
{
	return left.bootCount == right.bootCount && left.watchdogCount == right.watchdogCount &&
		left.brownoutCount == right.brownoutCount && left.configErrorCount == right.configErrorCount &&
		left.otaFailureCount == right.otaFailureCount && left.networkFailureCount == right.networkFailureCount &&
		left.modbusErrorCount == right.modbusErrorCount && left.knxErrorCount == right.knxErrorCount &&
		left.storageErrorCount == right.storageErrorCount;
}

[[nodiscard]] DiagnosticRecord encodeDiagnosticCounters(const domain::PersistentDiagnosticCounters &counters,
	const std::uint32_t generation) noexcept
{
	DiagnosticPayload payload{};
	DiagnosticRecord payloadWriter{};
	writeDiagnosticU32(payloadWriter, 0, counters.bootCount);
	writeDiagnosticU32(payloadWriter, 4, counters.watchdogCount);
	writeDiagnosticU32(payloadWriter, 8, counters.brownoutCount);
	writeDiagnosticU32(payloadWriter, 12, counters.configErrorCount);
	writeDiagnosticU32(payloadWriter, 16, counters.otaFailureCount);
	writeDiagnosticU32(payloadWriter, 20, counters.networkFailureCount);
	writeDiagnosticU32(payloadWriter, 24, counters.modbusErrorCount);
	writeDiagnosticU32(payloadWriter, 28, counters.knxErrorCount);
	writeDiagnosticU32(payloadWriter, 32, counters.storageErrorCount);
	std::copy_n(payloadWriter.begin(), payload.size(), payload.begin());

	DiagnosticRecord record{};
	writeDiagnosticU32(record, 0, diagnosticRecordMagic);
	record[4] = static_cast<std::uint8_t>(diagnosticRecordVersion);
	record[5] = static_cast<std::uint8_t>(diagnosticRecordVersion >> 8U);
	record[6] = static_cast<std::uint8_t>(diagnosticPayloadSize);
	record[7] = static_cast<std::uint8_t>(diagnosticPayloadSize >> 8U);
	writeDiagnosticU32(record, 8, generation);
	writeDiagnosticU32(record, 12, crc32(payload));
	std::copy(payload.begin(), payload.end(), record.begin() + recordHeaderSize);
	return record;
}

[[nodiscard]] bool decodeDiagnosticCounters(const DiagnosticRecord &record,
	domain::PersistentDiagnosticCounters &counters,
	std::uint32_t &generation) noexcept
{
	const auto version = static_cast<std::uint16_t>(record[4] | static_cast<std::uint16_t>(record[5]) << 8U);
	const auto payloadLength = static_cast<std::uint16_t>(record[6] | static_cast<std::uint16_t>(record[7]) << 8U);
	if (readDiagnosticU32(record, 0) != diagnosticRecordMagic || version != diagnosticRecordVersion ||
		payloadLength != diagnosticPayloadSize)
	{
		return false;
	}
	DiagnosticPayload payload{};
	std::copy(record.begin() + recordHeaderSize, record.end(), payload.begin());
	if (crc32(payload) != readDiagnosticU32(record, 12))
	{
		return false;
	}
	counters.bootCount = readDiagnosticU32(record, 16);
	counters.watchdogCount = readDiagnosticU32(record, 20);
	counters.brownoutCount = readDiagnosticU32(record, 24);
	counters.configErrorCount = readDiagnosticU32(record, 28);
	counters.otaFailureCount = readDiagnosticU32(record, 32);
	counters.networkFailureCount = readDiagnosticU32(record, 36);
	counters.modbusErrorCount = readDiagnosticU32(record, 40);
	counters.knxErrorCount = readDiagnosticU32(record, 44);
	counters.storageErrorCount = readDiagnosticU32(record, 48);
	generation = readDiagnosticU32(record, 8);
	return true;
}

[[nodiscard]] const char *diagnosticSlotKey(const std::uint8_t slot) noexcept
{
	return slot == slotA ? diagnosticSlotAKey : diagnosticSlotBKey;
}

[[nodiscard]] DiagnosticSlotRecord readDiagnosticSlot(Preferences &preferences, const std::uint8_t slot) noexcept
{
	DiagnosticSlotRecord result{};
	const auto *key = diagnosticSlotKey(slot);
	const auto storedLength = preferences.getBytesLength(key);
	if (storedLength == 0) return result;
	if (storedLength != result.encoded.size() ||
		preferences.getBytes(key, result.encoded.data(), result.encoded.size()) != result.encoded.size())
	{
		result.state = SlotState::Corrupt;
		return result;
	}
	result.state = decodeDiagnosticCounters(result.encoded, result.counters, result.generation) ?
		SlotState::Valid : SlotState::Corrupt;
	return result;
}

[[nodiscard]] bool persistDiagnosticCounters(Preferences &preferences,
	const domain::PersistentDiagnosticCounters &counters) noexcept
{
	const auto first = readDiagnosticSlot(preferences, slotA);
	const auto second = readDiagnosticSlot(preferences, slotB);
	const auto marker = preferences.getUChar(diagnosticActiveSlotKey, noActiveSlot);
	std::uint8_t activeSlot{noActiveSlot};
	std::uint32_t generation{0};
	if (first.state == SlotState::Valid && second.state == SlotState::Valid)
	{
		activeSlot = first.generation == second.generation ? (marker == slotB ? slotB : slotA) :
			(second.generation > first.generation ? slotB : slotA);
		generation = std::max(first.generation, second.generation);
	}
	else if (first.state == SlotState::Valid)
	{
		activeSlot = slotA;
		generation = first.generation;
	}
	else if (second.state == SlotState::Valid)
	{
		activeSlot = slotB;
		generation = second.generation;
	}
	const auto nextGeneration = generation == std::numeric_limits<std::uint32_t>::max() ? generation : generation + 1U;
	const auto targetSlot = activeSlot == slotA ? slotB : slotA;
	const auto encoded = encodeDiagnosticCounters(counters, nextGeneration);
	if (preferences.putBytes(diagnosticSlotKey(targetSlot), encoded.data(), encoded.size()) != encoded.size()) return false;
	const auto verified = readDiagnosticSlot(preferences, targetSlot);
	if (verified.state != SlotState::Valid || verified.generation != nextGeneration ||
		!diagnosticCountersEqual(verified.counters, counters))
	{
		return false;
	}
	return preferences.putUChar(diagnosticActiveSlotKey, targetSlot) == sizeof(targetSlot);
}
}

NvsSettingsStore::~NvsSettingsStore()
{
	if (initialized_)
	{
		preferences_.end();
	}
}

NvsInitializeResult NvsSettingsStore::initialize() noexcept
{
	if (initialized_)
	{
		return NvsInitializeResult::Initialized;
	}

	initialized_ = preferences_.begin(settingsNamespace, false);
	return initialized_ ? NvsInitializeResult::Initialized : NvsInitializeResult::OpenFailure;
}

DiagnosticCountersBootResult NvsSettingsStore::beginDiagnosticCounters(const domain::ResetCategory resetReason) noexcept
{
	DiagnosticCountersBootResult result{};
	const auto increment = [](std::uint32_t &counter) noexcept {
		if (counter != std::numeric_limits<std::uint32_t>::max()) ++counter;
	};
	if (!initialized_)
	{
		increment(result.counters.bootCount);
		if (resetReason == domain::ResetCategory::Watchdog) increment(result.counters.watchdogCount);
		if (resetReason == domain::ResetCategory::Brownout) increment(result.counters.brownoutCount);
		return result;
	}

	const auto first = readDiagnosticSlot(preferences_, slotA);
	const auto second = readDiagnosticSlot(preferences_, slotB);
	if (first.state == SlotState::Valid && second.state == SlotState::Valid)
	{
		const auto marker = preferences_.getUChar(diagnosticActiveSlotKey, noActiveSlot);
		result.counters = first.generation == second.generation ?
			(marker == slotB ? second.counters : first.counters) :
			(second.generation > first.generation ? second.counters : first.counters);
	}
	else if (first.state == SlotState::Valid || second.state == SlotState::Valid)
	{
		result.counters = first.state == SlotState::Valid ? first.counters : second.counters;
	}
	else
	{
		result.counters.bootCount = preferences_.getUInt(bootCountKey, 0);
	}
	if (first.state == SlotState::Corrupt || second.state == SlotState::Corrupt)
	{
		increment(result.counters.storageErrorCount);
	}
	increment(result.counters.bootCount);
	if (resetReason == domain::ResetCategory::Watchdog) increment(result.counters.watchdogCount);
	if (resetReason == domain::ResetCategory::Brownout) increment(result.counters.brownoutCount);
	result.persisted = persistDiagnosticCounters(preferences_, result.counters);
	if (result.persisted && preferences_.isKey(bootCountKey)) static_cast<void>(preferences_.remove(bootCountKey));
	return result;
}

bool NvsSettingsStore::saveDiagnosticCounters(const domain::PersistentDiagnosticCounters &counters) noexcept
{
	return initialized_ && persistDiagnosticCounters(preferences_, counters);
}

ports::SettingsStore NvsSettingsStore::port() noexcept
{
	return {loadCallback, saveCallback, eraseCallback, this};
}

bool NvsSettingsStore::isInitialized() const noexcept
{
	return initialized_;
}

ports::SettingsLoadResult NvsSettingsStore::loadCallback(void *const context, domain::Configuration &configuration) noexcept
{
	return static_cast<NvsSettingsStore *>(context)->load(configuration);
}

ports::SettingsSaveResult NvsSettingsStore::saveCallback(void *const context, const domain::Configuration &configuration) noexcept
{
	return static_cast<NvsSettingsStore *>(context)->save(configuration);
}

ports::SettingsEraseResult NvsSettingsStore::eraseCallback(void *const context) noexcept
{
	return static_cast<NvsSettingsStore *>(context)->erase();
}

ports::SettingsLoadResult NvsSettingsStore::load(domain::Configuration &configuration) noexcept
{
	if (!initialized_)
	{
		return ports::SettingsLoadResult::IoFailure;
	}

	const auto first = readSlot(preferences_, slotA);
	const auto second = readSlot(preferences_, slotB);
	if (first.state == SlotState::Valid && second.state == SlotState::Valid)
	{
		configuration = second.configuration.generation > first.configuration.generation ? second.configuration : first.configuration;
		return ports::SettingsLoadResult::Loaded;
	}
	if (first.state == SlotState::Valid || second.state == SlotState::Valid)
	{
		configuration = first.state == SlotState::Valid ? first.configuration : second.configuration;
		return ports::SettingsLoadResult::Loaded;
	}
	return first.state == SlotState::Missing && second.state == SlotState::Missing ? ports::SettingsLoadResult::NotFound
																				 : ports::SettingsLoadResult::Corrupt;
}

ports::SettingsSaveResult NvsSettingsStore::save(const domain::Configuration &configuration) noexcept
{
	if (!initialized_ || domain::validateConfiguration(configuration) != domain::ConfigurationValidationError::None)
	{
		return ports::SettingsSaveResult::IoFailure;
	}

	Record encoded{};
	if (!encodeRecord(configuration, encoded))
	{
		return ports::SettingsSaveResult::IoFailure;
	}

	const auto first = readSlot(preferences_, slotA);
	const auto second = readSlot(preferences_, slotB);
	const auto marker = preferences_.getUChar(activeSlotKey, noActiveSlot);
	std::uint8_t activeSlot{noActiveSlot};
	if (first.state == SlotState::Valid && second.state == SlotState::Valid)
	{
		if (first.configuration.generation == second.configuration.generation)
		{
			activeSlot = marker == slotB ? slotB : slotA;
		}
		else
		{
			activeSlot = second.configuration.generation > first.configuration.generation ? slotB : slotA;
		}
	}
	else if (first.state == SlotState::Valid)
	{
		activeSlot = slotA;
	}
	else if (second.state == SlotState::Valid)
	{
		activeSlot = slotB;
	}

	const auto targetSlot = activeSlot == slotA ? slotB : slotA;
	const auto *targetKey = slotKey(targetSlot);
	if (preferences_.putBytes(targetKey, encoded.data(), encoded.size()) != encoded.size())
	{
		return ports::SettingsSaveResult::IoFailure;
	}

	const auto written = readSlot(preferences_, targetSlot);
	if (written.state != SlotState::Valid || written.encoded != encoded)
	{
		return ports::SettingsSaveResult::VerificationFailure;
	}
	if (preferences_.putUChar(activeSlotKey, targetSlot) != sizeof(targetSlot))
	{
		return ports::SettingsSaveResult::IoFailure;
	}

	return ports::SettingsSaveResult::Saved;
}

ports::SettingsEraseResult NvsSettingsStore::erase() noexcept
{
	if (!initialized_ || !preferences_.clear())
	{
		return ports::SettingsEraseResult::IoFailure;
	}
	if (preferences_.isKey(slotAKey) || preferences_.isKey(slotBKey) || preferences_.isKey(activeSlotKey) ||
		preferences_.isKey(diagnosticSlotAKey) || preferences_.isKey(diagnosticSlotBKey) ||
		preferences_.isKey(diagnosticActiveSlotKey) || preferences_.isKey(bootCountKey))
	{
		return ports::SettingsEraseResult::VerificationFailure;
	}
	return ports::SettingsEraseResult::Erased;
}
}