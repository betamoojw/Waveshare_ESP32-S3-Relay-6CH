#include "modbus_rtu_adapter.h"

#include "modbus_error_representation.h"
#include "../../domain/version_compatibility.h"

#include <array>
#include <cstring>
#include <limits>
#include <string_view>

namespace switch_actuator::adapters::modbus
{
namespace
{
constexpr std::int32_t pollTimeoutMs{2};
constexpr std::uint32_t maximumClientResponseBytes{45};
constexpr std::uint32_t clientTimingMarginCharacters{4};
constexpr std::int32_t minimumClientResponseTimeoutMs{20};

void copyIdentification(char *const destination, const std::string_view source) noexcept
{
	std::memset(destination, 0, NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH);
	const auto copyLength = source.size() < NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH - 1
							? source.size()
							: NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH - 1;
	if (copyLength != 0)
	{
		std::memcpy(destination, source.data(), copyLength);
	}
}
}

ModbusRtuAdapter::ModbusRtuAdapter(const ModbusRtuDependencies dependencies) noexcept
	: dependencies_{dependencies}
{
}

ModbusInitializeResult ModbusRtuAdapter::initialize(const ModbusRtuConfiguration &configuration) noexcept
{
	initialized_ = false;
	if (dependencies_.serialRead == nullptr || dependencies_.serialWrite == nullptr || dependencies_.snapshotProvider == nullptr ||
		dependencies_.writeBatchHandler == nullptr || dependencies_.diagnostics == nullptr)
	{
		return ModbusInitializeResult::InvalidDependencies;
	}
	if (!validConfiguration(configuration))
	{
		return ModbusInitializeResult::InvalidConfiguration;
	}

	configuration_ = configuration;
	if (!createRole(ports::ModbusRtuRole::Server))
	{
		return ModbusInitializeResult::LibraryFailure;
	}
	correlationId_ = 0;
	requestHandled_ = false;
	initialized_ = true;
	return ModbusInitializeResult::Initialized;
}

ModbusPollResult ModbusRtuAdapter::poll() noexcept
{
	if (!initialized_)
	{
		return ModbusPollResult::NotInitialized;
	}
	if (role_ == ports::ModbusRtuRole::Client)
	{
		return ModbusPollResult::Idle;
	}

	requestHandled_ = false;
	const auto result = nmbs_server_poll(&protocol_);
	if (result == NMBS_ERROR_NONE)
	{
		return requestHandled_ ? ModbusPollResult::RequestHandled : ModbusPollResult::Idle;
	}
	if (result == NMBS_ERROR_TIMEOUT)
	{
		return ModbusPollResult::Idle;
	}
	if (result == NMBS_ERROR_CRC)
	{
		dependencies_.diagnostics->recordModbus(app::ModbusDiagnosticEvent::CrcError);
		return ModbusPollResult::ProtocolError;
	}
	if (result == NMBS_ERROR_TRANSPORT)
	{
		static_cast<void>(dependencies_.diagnostics->recordFault(
			domain::FaultCode::ModbusTransportError, domain::FaultSeverity::Warning, dependencies_.diagnostics->snapshot().uptimeMs));
		return ModbusPollResult::TransportError;
	}

	dependencies_.diagnostics->recordModbus(app::ModbusDiagnosticEvent::MalformedFrame);
	return ModbusPollResult::ProtocolError;
}

ports::ModbusRtuControlPort ModbusRtuAdapter::controlPort() noexcept
{
	return {this, setRole, role, readClientHoldingRegisters, writeClientRegister};
}

bool ModbusRtuAdapter::isInitialized() const noexcept
{
	return initialized_;
}

bool ModbusRtuAdapter::setRole(void *const context, const ports::ModbusRtuRole role) noexcept
{
	if (context == nullptr)
	{
		return false;
	}
	auto &adapter = *static_cast<ModbusRtuAdapter *>(context);
	if (!adapter.initialized_)
	{
		return false;
	}
	return adapter.role_ == role || adapter.createRole(role);
}

ports::ModbusRtuRole ModbusRtuAdapter::role(const void *const context) noexcept
{
	return context != nullptr ? static_cast<const ModbusRtuAdapter *>(context)->role_ : ports::ModbusRtuRole::Server;
}

ports::ModbusClientResult ModbusRtuAdapter::readClientHoldingRegisters(void *const context,
																			  const std::uint8_t destination,
																			  const std::uint16_t address,
																			  const std::uint16_t quantity,
																			  std::uint16_t *const output,
																			  const std::size_t outputCapacity) noexcept
{
	if (context == nullptr)
	{
		return ports::ModbusClientResult::NotInitialized;
	}
	auto &adapter = *static_cast<ModbusRtuAdapter *>(context);
	if (!adapter.initialized_)
	{
		return ports::ModbusClientResult::NotInitialized;
	}
	if (adapter.role_ != ports::ModbusRtuRole::Client)
	{
		return ports::ModbusClientResult::WrongRole;
	}
	if (destination < 1 || destination > 247 || quantity == 0 || quantity > 20 || output == nullptr ||
		outputCapacity < quantity)
	{
		return ports::ModbusClientResult::InvalidArgument;
	}
	nmbs_set_destination_rtu_address(&adapter.protocol_, destination);
	return adapter.clientResult(nmbs_read_holding_registers(&adapter.protocol_, address, quantity, output));
}

ports::ModbusClientResult ModbusRtuAdapter::writeClientRegister(void *const context,
																		 const std::uint8_t destination,
																		 const std::uint16_t address,
																		 const std::uint16_t value) noexcept
{
	if (context == nullptr)
	{
		return ports::ModbusClientResult::NotInitialized;
	}
	auto &adapter = *static_cast<ModbusRtuAdapter *>(context);
	if (!adapter.initialized_)
	{
		return ports::ModbusClientResult::NotInitialized;
	}
	if (adapter.role_ != ports::ModbusRtuRole::Client)
	{
		return ports::ModbusClientResult::WrongRole;
	}
	if (destination < 1 || destination > 247)
	{
		return ports::ModbusClientResult::InvalidArgument;
	}
	nmbs_set_destination_rtu_address(&adapter.protocol_, destination);
	return adapter.clientResult(nmbs_write_single_register(&adapter.protocol_, address, value));
}

std::int32_t ModbusRtuAdapter::serialRead(std::uint8_t *const buffer,
											 const std::uint16_t count,
											 const std::int32_t timeoutMs,
											 void *const context) noexcept
{
	auto &adapter = *static_cast<ModbusRtuAdapter *>(context);
	return adapter.dependencies_.serialRead(adapter.dependencies_.serialContext, buffer, count, timeoutMs);
}

std::int32_t ModbusRtuAdapter::serialWrite(const std::uint8_t *const buffer,
											  const std::uint16_t count,
											  const std::int32_t timeoutMs,
											  void *const context) noexcept
{
	auto &adapter = *static_cast<ModbusRtuAdapter *>(context);
	return adapter.dependencies_.serialWrite(adapter.dependencies_.serialContext, buffer, count, timeoutMs);
}

nmbs_error ModbusRtuAdapter::readCoils(const std::uint16_t address,
									   const std::uint16_t quantity,
									   nmbs_bitfield output,
									   const std::uint8_t,
									   void *const context) noexcept
{
	auto &adapter = *static_cast<ModbusRtuAdapter *>(context);
	RegisterMapSnapshot snapshot{};
	const auto snapshotResult = adapter.provideSnapshot(snapshot);
	if (snapshotResult != NMBS_ERROR_NONE)
	{
		return snapshotResult;
	}
	std::array<bool, domain::relayChannelCount> values{};
	const auto mapResult = adapter.registerMap_.readCoils(address, quantity, snapshot, values.data(), values.size());
	if (mapResult != RegisterMapResult::Success)
	{
		return toException(mapResult);
	}
	output[0] = 0;
	for (std::size_t index = 0; index < quantity; ++index)
	{
		nmbs_bitfield_write(output, index, values[index]);
	}
	adapter.requestHandled_ = true;
	adapter.dependencies_.diagnostics->recordModbus(app::ModbusDiagnosticEvent::ValidRequest);
	return NMBS_ERROR_NONE;
}

nmbs_error ModbusRtuAdapter::readDiscreteInputs(const std::uint16_t address,
														 const std::uint16_t quantity,
														 nmbs_bitfield output,
														 const std::uint8_t,
														 void *const context) noexcept
{
	auto &adapter = *static_cast<ModbusRtuAdapter *>(context);
	RegisterMapSnapshot snapshot{};
	const auto snapshotResult = adapter.provideSnapshot(snapshot);
	if (snapshotResult != NMBS_ERROR_NONE)
	{
		return snapshotResult;
	}
	std::array<bool, domain::relayChannelCount> values{};
	const auto mapResult = adapter.registerMap_.readDiscreteInputs(address, quantity, snapshot, values.data(), values.size());
	if (mapResult != RegisterMapResult::Success)
	{
		return toException(mapResult);
	}
	output[0] = 0;
	for (std::size_t index = 0; index < quantity; ++index)
	{
		nmbs_bitfield_write(output, index, values[index]);
	}
	adapter.requestHandled_ = true;
	adapter.dependencies_.diagnostics->recordModbus(app::ModbusDiagnosticEvent::ValidRequest);
	return NMBS_ERROR_NONE;
}

nmbs_error ModbusRtuAdapter::readHoldingRegisters(const std::uint16_t address,
															 const std::uint16_t quantity,
															 std::uint16_t *const output,
															 const std::uint8_t,
															 void *const context) noexcept
{
	auto &adapter = *static_cast<ModbusRtuAdapter *>(context);
	RegisterMapSnapshot snapshot{};
	const auto snapshotResult = adapter.provideSnapshot(snapshot);
	if (snapshotResult != NMBS_ERROR_NONE)
	{
		return snapshotResult;
	}
	const auto result = toException(adapter.registerMap_.readHoldingRegisters(address, quantity, snapshot, output, quantity));
	if (result == NMBS_ERROR_NONE)
	{
		adapter.requestHandled_ = true;
		adapter.dependencies_.diagnostics->recordModbus(app::ModbusDiagnosticEvent::ValidRequest);
	}
	return result;
}

nmbs_error ModbusRtuAdapter::readInputRegisters(const std::uint16_t address,
														  const std::uint16_t quantity,
														  std::uint16_t *const output,
														  const std::uint8_t,
														  void *const context) noexcept
{
	auto &adapter = *static_cast<ModbusRtuAdapter *>(context);
	RegisterMapSnapshot snapshot{};
	const auto snapshotResult = adapter.provideSnapshot(snapshot);
	if (snapshotResult != NMBS_ERROR_NONE)
	{
		return snapshotResult;
	}
	const auto result = toException(adapter.registerMap_.readInputRegisters(address, quantity, snapshot, output, quantity));
	if (result == NMBS_ERROR_NONE)
	{
		adapter.requestHandled_ = true;
		adapter.dependencies_.diagnostics->recordModbus(app::ModbusDiagnosticEvent::ValidRequest);
	}
	return result;
}

nmbs_error ModbusRtuAdapter::writeSingleCoil(const std::uint16_t address,
													  const bool value,
													  const std::uint8_t,
													  void *const context) noexcept
{
	auto &adapter = *static_cast<ModbusRtuAdapter *>(context);
	HoldingWriteBatch batch{};
	const auto result = adapter.registerMap_.parseCoilWrite(address,
		&value,
		1,
		domain::CommandSource::Modbus,
		adapter.nextCorrelationId(),
		adapter.dependencies_.diagnostics->snapshot().uptimeMs,
		batch);
	return result == RegisterMapResult::Success ? adapter.submit(batch) : toException(result);
}

nmbs_error ModbusRtuAdapter::writeMultipleCoils(const std::uint16_t address,
														 const std::uint16_t quantity,
														 const nmbs_bitfield values,
														 const std::uint8_t,
														 void *const context) noexcept
{
	if (quantity > domain::relayChannelCount)
	{
		return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
	}
	auto &adapter = *static_cast<ModbusRtuAdapter *>(context);
	std::array<bool, domain::relayChannelCount> decoded{};
	for (std::size_t index = 0; index < quantity; ++index)
	{
		decoded[index] = nmbs_bitfield_read(values, index);
	}
	HoldingWriteBatch batch{};
	const auto result = adapter.registerMap_.parseCoilWrite(address,
		decoded.data(),
		quantity,
		domain::CommandSource::Modbus,
		adapter.nextCorrelationId(),
		adapter.dependencies_.diagnostics->snapshot().uptimeMs,
		batch);
	return result == RegisterMapResult::Success ? adapter.submit(batch) : toException(result);
}

nmbs_error ModbusRtuAdapter::writeSingleRegister(const std::uint16_t address,
														  const std::uint16_t value,
														  const std::uint8_t unitId,
														  void *const context) noexcept
{
	return writeMultipleRegisters(address, 1, &value, unitId, context);
}

nmbs_error ModbusRtuAdapter::writeMultipleRegisters(const std::uint16_t address,
														   const std::uint16_t quantity,
														   const std::uint16_t *const values,
														   const std::uint8_t unitId,
														   void *const context) noexcept
{
	auto &adapter = *static_cast<ModbusRtuAdapter *>(context);
	HoldingWriteBatch batch{};
	const auto result = adapter.registerMap_.parseHoldingWrite(address,
		values,
		quantity,
		domain::CommandSource::Modbus,
		adapter.nextCorrelationId(),
		adapter.dependencies_.diagnostics->snapshot().uptimeMs,
		batch);
	if (result == RegisterMapResult::Success && unitId == NMBS_BROADCAST_ADDRESS &&
		(batch.kind == HoldingWriteKind::UartSettings || batch.kind == HoldingWriteKind::UnitId))
	{
		return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
	}
	return result == RegisterMapResult::Success ? adapter.submit(batch) : toException(result);
}

nmbs_error ModbusRtuAdapter::readDeviceIdentification(const std::uint8_t objectId,
																 char buffer[NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH]) noexcept
{
	switch (objectId)
	{
	case 0x00:
		copyIdentification(buffer, "BetaMoojw");
		return NMBS_ERROR_NONE;
	case 0x01:
		copyIdentification(buffer, "Waveshare-ESP32S3-Relay6CH");
		return NMBS_ERROR_NONE;
	case 0x02:
		copyIdentification(buffer, domain::compatibility::firmware);
		return NMBS_ERROR_NONE;
	default:
		return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
	}
}

nmbs_error ModbusRtuAdapter::readDeviceIdentificationMap(nmbs_bitfield_256 map) noexcept
{
	std::memset(map, 0, sizeof(nmbs_bitfield_256));
	nmbs_bitfield_set(map, 0x00);
	nmbs_bitfield_set(map, 0x01);
	nmbs_bitfield_set(map, 0x02);
	return NMBS_ERROR_NONE;
}

bool ModbusRtuAdapter::validConfiguration(const ModbusRtuConfiguration &configuration) noexcept
{
	return configuration.unitId >= 1 && configuration.unitId <= 247 && configuration.baudRate > 0 &&
		   configuration.dataBits == 8 &&
		   (configuration.stopBits == 1 || configuration.stopBits == 2) &&
		   (configuration.parity == domain::SerialParity::None || configuration.parity == domain::SerialParity::Even ||
			configuration.parity == domain::SerialParity::Odd);
}

std::int32_t ModbusRtuAdapter::byteTimeoutMs(const ModbusRtuConfiguration &configuration) noexcept
{
	const auto bitsPerCharacter = 1U + configuration.dataBits + configuration.stopBits +
								  (configuration.parity == domain::SerialParity::None ? 0U : 1U);
	const auto twoCharacterMicroseconds = (2'000'000ULL * bitsPerCharacter + configuration.baudRate - 1U) / configuration.baudRate;
	const auto timeout = (twoCharacterMicroseconds + 999U) / 1000U;
	return static_cast<std::int32_t>(timeout == 0 ? 1 : timeout);
}

std::int32_t ModbusRtuAdapter::clientResponseTimeoutMs(const ModbusRtuConfiguration &configuration) noexcept
{
	const auto bitsPerCharacter = 1U + configuration.dataBits + configuration.stopBits +
								  (configuration.parity == domain::SerialParity::None ? 0U : 1U);
	const auto frameCharacters = maximumClientResponseBytes + clientTimingMarginCharacters;
	const auto doubledFrameMicroseconds =
		(2'000'000ULL * frameCharacters * bitsPerCharacter + configuration.baudRate - 1U) / configuration.baudRate;
	const auto timeoutMs = static_cast<std::int32_t>((doubledFrameMicroseconds + 999U) / 1000U);
	return timeoutMs < minimumClientResponseTimeoutMs ? minimumClientResponseTimeoutMs : timeoutMs;
}

nmbs_error ModbusRtuAdapter::toException(const RegisterMapResult result) noexcept
{
	switch (result)
	{
	case RegisterMapResult::Success:
		return NMBS_ERROR_NONE;
	case RegisterMapResult::IllegalAddress:
		return represent(domain::ErrorCode::NotFound);
	case RegisterMapResult::IllegalValue:
		return represent(domain::ErrorCode::InvalidArgument);
	case RegisterMapResult::InvalidBuffer:
	default:
		return represent(domain::ErrorCode::InternalError);
	}
}

nmbs_error ModbusRtuAdapter::provideSnapshot(RegisterMapSnapshot &snapshot) noexcept
{
	return dependencies_.snapshotProvider(dependencies_.snapshotContext, snapshot) ? NMBS_ERROR_NONE
																								 : NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;
}

nmbs_error ModbusRtuAdapter::submit(const HoldingWriteBatch &batch) noexcept
{
	const auto error = dependencies_.writeBatchHandler(dependencies_.writeBatchContext, batch);
	if (!error.has_value())
	{
		requestHandled_ = true;
		dependencies_.diagnostics->recordModbus(app::ModbusDiagnosticEvent::ValidRequest);
		return NMBS_ERROR_NONE;
	}
	if (*error == domain::ErrorCode::InvalidArgument || *error == domain::ErrorCode::ConfigurationError)
	{
		dependencies_.diagnostics->recordModbus(app::ModbusDiagnosticEvent::IllegalValue);
	}
	else if (*error == domain::ErrorCode::Busy)
	{
		dependencies_.diagnostics->recordCommandQueueFull(dependencies_.diagnostics->snapshot().uptimeMs);
	}
	return represent(*error);
}

std::uint32_t ModbusRtuAdapter::nextCorrelationId() noexcept
{
	if (correlationId_ == std::numeric_limits<std::uint32_t>::max())
	{
		correlationId_ = 1;
	}
	else
	{
		++correlationId_;
	}
	return correlationId_;
}

bool ModbusRtuAdapter::createRole(const ports::ModbusRtuRole role) noexcept
{
	nmbs_platform_conf platform{};
	nmbs_platform_conf_create(&platform);
	platform.transport = NMBS_TRANSPORT_RTU;
	platform.read = serialRead;
	platform.write = serialWrite;
	platform.arg = this;

	nmbs_t replacement{};
	nmbs_error result{NMBS_ERROR_INVALID_ARGUMENT};
	if (role == ports::ModbusRtuRole::Server)
	{
		nmbs_callbacks callbacks{};
		nmbs_callbacks_create(&callbacks);
		callbacks.read_coils = readCoils;
		callbacks.read_discrete_inputs = readDiscreteInputs;
		callbacks.read_holding_registers = readHoldingRegisters;
		callbacks.read_input_registers = readInputRegisters;
		callbacks.write_single_coil = writeSingleCoil;
		callbacks.write_multiple_coils = writeMultipleCoils;
		callbacks.write_single_register = writeSingleRegister;
		callbacks.write_multiple_registers = writeMultipleRegisters;
		callbacks.read_device_identification = readDeviceIdentification;
		callbacks.read_device_identification_map = readDeviceIdentificationMap;
		callbacks.arg = this;
		result = nmbs_server_create(&replacement, configuration_.unitId, &platform, &callbacks);
	}
	else
	{
		result = nmbs_client_create(&replacement, &platform);
	}
	if (result != NMBS_ERROR_NONE)
	{
		return false;
	}

	nmbs_set_read_timeout(&replacement,
		role == ports::ModbusRtuRole::Server ? pollTimeoutMs : clientResponseTimeoutMs(configuration_));
	nmbs_set_byte_timeout(&replacement, byteTimeoutMs(configuration_));
	protocol_ = replacement;
	role_ = role;
	requestHandled_ = false;
	return true;
}

ports::ModbusClientResult ModbusRtuAdapter::clientResult(const nmbs_error error) noexcept
{
	if (error == NMBS_ERROR_NONE)
	{
		dependencies_.diagnostics->recordModbus(app::ModbusDiagnosticEvent::ValidRequest);
		return ports::ModbusClientResult::Success;
	}
	if (error == NMBS_ERROR_TRANSPORT || error == NMBS_ERROR_TIMEOUT)
	{
		static_cast<void>(dependencies_.diagnostics->recordFault(domain::FaultCode::ModbusTransportError,
			domain::FaultSeverity::Warning,
			dependencies_.diagnostics->snapshot().uptimeMs));
		return ports::ModbusClientResult::TransportError;
	}
	if (error == NMBS_ERROR_CRC)
	{
		dependencies_.diagnostics->recordModbus(app::ModbusDiagnosticEvent::CrcError);
	}
	else
	{
		dependencies_.diagnostics->recordModbus(app::ModbusDiagnosticEvent::MalformedFrame);
	}
	return ports::ModbusClientResult::ProtocolError;
}
}