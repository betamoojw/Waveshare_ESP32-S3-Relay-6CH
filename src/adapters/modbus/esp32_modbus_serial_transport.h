#pragma once

#include "modbus_rtu_adapter.h"
#include "../../hal/BoardDescriptor.h"
#include "../../hal/Rs485Hal.h"

#include <HardwareSerial.h>

#include <cstdint>
#include <optional>

namespace switch_actuator::adapters::modbus
{
enum class SerialTransportInitializeResult : std::uint8_t
{
	Initialized,
	InvalidConfiguration,
	InvalidPins,
	HardwareFailure
};

class Esp32ModbusSerialTransport final
{
public:
	Esp32ModbusSerialTransport(HardwareSerial &serial, const hal::BoardDescriptor &descriptor) noexcept;
	~Esp32ModbusSerialTransport();

	Esp32ModbusSerialTransport(const Esp32ModbusSerialTransport &) = delete;
	Esp32ModbusSerialTransport &operator=(const Esp32ModbusSerialTransport &) = delete;
	Esp32ModbusSerialTransport(Esp32ModbusSerialTransport &&) = delete;
	Esp32ModbusSerialTransport &operator=(Esp32ModbusSerialTransport &&) = delete;

	[[nodiscard]] SerialTransportInitializeResult initialize(const ModbusRtuConfiguration &configuration) noexcept;
	void shutdown() noexcept;
	[[nodiscard]] bool isInitialized() const noexcept;
	[[nodiscard]] hal::IUart hal() noexcept;

	[[nodiscard]] static std::int32_t read(void *context,
										std::uint8_t *buffer,
										std::uint16_t count,
										std::int32_t byteTimeoutMs) noexcept;
	[[nodiscard]] static std::int32_t write(void *context,
										 const std::uint8_t *buffer,
										 std::uint16_t count,
										 std::int32_t byteTimeoutMs) noexcept;

private:
	[[nodiscard]] static std::optional<std::uint32_t> serialConfig(const ModbusRtuConfiguration &configuration) noexcept;
	[[nodiscard]] std::int32_t readBytes(std::uint8_t *buffer,
									 std::uint16_t count,
									 std::int32_t byteTimeoutMs) noexcept;
	[[nodiscard]] std::int32_t writeBytes(const std::uint8_t *buffer,
									  std::uint16_t count,
									  std::int32_t byteTimeoutMs) noexcept;

	HardwareSerial &serial_;
	const hal::BoardDescriptor &descriptor_;
	bool initialized_{false};
};
}