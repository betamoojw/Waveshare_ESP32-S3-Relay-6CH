#pragma once

#include "modbus_register_map.h"
#include "../../app/diagnostics_service.h"
#include "../../domain/configuration.h"
#include "../../domain/error.h"
#include "../../ports/modbus_rtu_control_port.h"
#include "nanomodbus/nanomodbus.h"

#include <cstdint>
#include <optional>

namespace switch_actuator::adapters::modbus
{
using SerialReadHandler = std::int32_t (*)(void *context,
										  std::uint8_t *buffer,
										  std::uint16_t count,
										  std::int32_t byteTimeoutMs) noexcept;
using SerialWriteHandler = std::int32_t (*)(void *context,
										   const std::uint8_t *buffer,
										   std::uint16_t count,
										   std::int32_t byteTimeoutMs) noexcept;
using SnapshotProvider = bool (*)(void *context, RegisterMapSnapshot &snapshot) noexcept;

using WriteBatchResult = std::optional<domain::ErrorCode>;
using WriteBatchHandler = WriteBatchResult (*)(void *context, const HoldingWriteBatch &batch) noexcept;

struct ModbusRtuDependencies final
{
	SerialReadHandler serialRead{nullptr};
	SerialWriteHandler serialWrite{nullptr};
	void *serialContext{nullptr};
	SnapshotProvider snapshotProvider{nullptr};
	void *snapshotContext{nullptr};
	WriteBatchHandler writeBatchHandler{nullptr};
	void *writeBatchContext{nullptr};
	app::DiagnosticsService *diagnostics{nullptr};
};

struct ModbusRtuConfiguration final
{
	std::uint8_t unitId{10};
	std::uint32_t baudRate{115200};
	domain::SerialParity parity{domain::SerialParity::None};
	std::uint8_t dataBits{8};
	std::uint8_t stopBits{1};
};

enum class ModbusInitializeResult : std::uint8_t
{
	Initialized,
	InvalidDependencies,
	InvalidConfiguration,
	LibraryFailure
};

enum class ModbusPollResult : std::uint8_t
{
	RequestHandled,
	Idle,
	ProtocolError,
	TransportError,
	NotInitialized
};

class ModbusRtuAdapter final
{
public:
	explicit ModbusRtuAdapter(ModbusRtuDependencies dependencies) noexcept;

	[[nodiscard]] ModbusInitializeResult initialize(const ModbusRtuConfiguration &configuration) noexcept;
	[[nodiscard]] ModbusPollResult poll() noexcept;
	[[nodiscard]] ports::ModbusRtuControlPort controlPort() noexcept;
	[[nodiscard]] bool isInitialized() const noexcept;

private:
	[[nodiscard]] static std::int32_t serialRead(std::uint8_t *buffer,
											  std::uint16_t count,
											  std::int32_t byteTimeoutMs,
											  void *context) noexcept;
	[[nodiscard]] static std::int32_t serialWrite(const std::uint8_t *buffer,
											   std::uint16_t count,
											   std::int32_t byteTimeoutMs,
											   void *context) noexcept;
	[[nodiscard]] static nmbs_error readCoils(std::uint16_t address,
											std::uint16_t quantity,
											nmbs_bitfield output,
											std::uint8_t unitId,
											void *context) noexcept;
	[[nodiscard]] static nmbs_error readDiscreteInputs(std::uint16_t address,
													  std::uint16_t quantity,
													  nmbs_bitfield output,
													  std::uint8_t unitId,
													  void *context) noexcept;
	[[nodiscard]] static nmbs_error readHoldingRegisters(std::uint16_t address,
													 std::uint16_t quantity,
													 std::uint16_t *output,
													 std::uint8_t unitId,
													 void *context) noexcept;
	[[nodiscard]] static nmbs_error readInputRegisters(std::uint16_t address,
												   std::uint16_t quantity,
												   std::uint16_t *output,
												   std::uint8_t unitId,
												   void *context) noexcept;
	[[nodiscard]] static nmbs_error writeSingleCoil(std::uint16_t address,
												   bool value,
												   std::uint8_t unitId,
												   void *context) noexcept;
	[[nodiscard]] static nmbs_error writeMultipleCoils(std::uint16_t address,
													  std::uint16_t quantity,
													  const nmbs_bitfield values,
													  std::uint8_t unitId,
													  void *context) noexcept;
	[[nodiscard]] static nmbs_error writeSingleRegister(std::uint16_t address,
													   std::uint16_t value,
													   std::uint8_t unitId,
													   void *context) noexcept;
	[[nodiscard]] static nmbs_error writeMultipleRegisters(std::uint16_t address,
														std::uint16_t quantity,
														const std::uint16_t *values,
														std::uint8_t unitId,
														void *context) noexcept;
	[[nodiscard]] static nmbs_error readDeviceIdentification(std::uint8_t objectId,
														 char buffer[NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH]) noexcept;
	[[nodiscard]] static nmbs_error readDeviceIdentificationMap(nmbs_bitfield_256 map) noexcept;
	[[nodiscard]] static bool setRole(void *context, ports::ModbusRtuRole role) noexcept;
	[[nodiscard]] static ports::ModbusRtuRole role(const void *context) noexcept;
	[[nodiscard]] static ports::ModbusClientResult readClientHoldingRegisters(void *context,
		std::uint8_t destination,
		std::uint16_t address,
		std::uint16_t quantity,
		std::uint16_t *output,
		std::size_t outputCapacity) noexcept;
	[[nodiscard]] static ports::ModbusClientResult writeClientRegister(void *context,
		std::uint8_t destination,
		std::uint16_t address,
		std::uint16_t value) noexcept;

	[[nodiscard]] static bool validConfiguration(const ModbusRtuConfiguration &configuration) noexcept;
	[[nodiscard]] bool createRole(ports::ModbusRtuRole role) noexcept;
	[[nodiscard]] ports::ModbusClientResult clientResult(nmbs_error error) noexcept;
	[[nodiscard]] static std::int32_t byteTimeoutMs(const ModbusRtuConfiguration &configuration) noexcept;
	[[nodiscard]] static std::int32_t clientResponseTimeoutMs(const ModbusRtuConfiguration &configuration) noexcept;
	[[nodiscard]] static nmbs_error toException(RegisterMapResult result) noexcept;
	[[nodiscard]] nmbs_error provideSnapshot(RegisterMapSnapshot &snapshot) noexcept;
	[[nodiscard]] nmbs_error submit(const HoldingWriteBatch &batch) noexcept;
	[[nodiscard]] std::uint32_t nextCorrelationId() noexcept;

	ModbusRtuDependencies dependencies_;
	ModbusRegisterMap registerMap_{};
	nmbs_t protocol_{};
	ModbusRtuConfiguration configuration_{};
	std::uint32_t correlationId_{0};
	ports::ModbusRtuRole role_{ports::ModbusRtuRole::Server};
	bool requestHandled_{false};
	bool initialized_{false};
};
}