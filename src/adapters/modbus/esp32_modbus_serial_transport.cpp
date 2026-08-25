#include "esp32_modbus_serial_transport.h"

#include <Arduino.h>
#include <driver/gpio.h>

#include <algorithm>
#include <cstddef>

namespace switch_actuator::adapters::modbus
{
Esp32ModbusSerialTransport::Esp32ModbusSerialTransport(HardwareSerial &serial,
													   const hal::BoardDescriptor &descriptor) noexcept
	: serial_{serial}, descriptor_{descriptor}
{
}

Esp32ModbusSerialTransport::~Esp32ModbusSerialTransport()
{
	shutdown();
}

SerialTransportInitializeResult Esp32ModbusSerialTransport::initialize(const ModbusRtuConfiguration &configuration) noexcept
{
	if (initialized_)
	{
		serial_.end();
	}
	initialized_ = false;
	const auto config = serialConfig(configuration);
	if (!config.has_value() || configuration.baudRate == 0)
	{
		return SerialTransportInitializeResult::InvalidConfiguration;
	}
	if (!descriptor_.rs485.available)
	{
		return SerialTransportInitializeResult::InvalidPins;
	}

	const auto rxPin = static_cast<gpio_num_t>(descriptor_.rs485.rxPin);
	const auto txPin = static_cast<gpio_num_t>(descriptor_.rs485.txPin);
	if (!GPIO_IS_VALID_GPIO(rxPin) || !GPIO_IS_VALID_OUTPUT_GPIO(txPin))
	{
		return SerialTransportInitializeResult::InvalidPins;
	}

	serial_.begin(configuration.baudRate,
		*config,
		static_cast<std::int8_t>(descriptor_.rs485.rxPin),
		static_cast<std::int8_t>(descriptor_.rs485.txPin));
	if (!serial_)
	{
		serial_.end();
		return SerialTransportInitializeResult::HardwareFailure;
	}

	initialized_ = true;
	return SerialTransportInitializeResult::Initialized;
}

bool Esp32ModbusSerialTransport::isInitialized() const noexcept
{
	return initialized_;
}

hal::IUart Esp32ModbusSerialTransport::hal() noexcept
{
	return {read, write, this};
}

std::int32_t Esp32ModbusSerialTransport::read(void *const context,
											 std::uint8_t *const buffer,
											 const std::uint16_t count,
											 const std::int32_t byteTimeoutMs) noexcept
{
	return context != nullptr ? static_cast<Esp32ModbusSerialTransport *>(context)->readBytes(buffer, count, byteTimeoutMs) : -1;
}

std::int32_t Esp32ModbusSerialTransport::write(void *const context,
											  const std::uint8_t *const buffer,
											  const std::uint16_t count,
											  const std::int32_t byteTimeoutMs) noexcept
{
	return context != nullptr ? static_cast<Esp32ModbusSerialTransport *>(context)->writeBytes(buffer, count, byteTimeoutMs) : -1;
}

std::optional<std::uint32_t> Esp32ModbusSerialTransport::serialConfig(const ModbusRtuConfiguration &configuration) noexcept
{
	if (configuration.dataBits != 8 ||
		(configuration.stopBits != 1 && configuration.stopBits != 2))
	{
		return std::nullopt;
	}

	if (configuration.dataBits == 7)
	{
		if (configuration.parity == domain::SerialParity::None)
		{
			return configuration.stopBits == 1 ? SERIAL_7N1 : SERIAL_7N2;
		}
		if (configuration.parity == domain::SerialParity::Even)
		{
			return configuration.stopBits == 1 ? SERIAL_7E1 : SERIAL_7E2;
		}
		if (configuration.parity == domain::SerialParity::Odd)
		{
			return configuration.stopBits == 1 ? SERIAL_7O1 : SERIAL_7O2;
		}
		return std::nullopt;
	}

	if (configuration.parity == domain::SerialParity::None)
	{
		return configuration.stopBits == 1 ? SERIAL_8N1 : SERIAL_8N2;
	}
	if (configuration.parity == domain::SerialParity::Even)
	{
		return configuration.stopBits == 1 ? SERIAL_8E1 : SERIAL_8E2;
	}
	if (configuration.parity == domain::SerialParity::Odd)
	{
		return configuration.stopBits == 1 ? SERIAL_8O1 : SERIAL_8O2;
	}
	return std::nullopt;
}

std::int32_t Esp32ModbusSerialTransport::readBytes(std::uint8_t *const buffer,
													  const std::uint16_t count,
													  const std::int32_t byteTimeoutMs) noexcept
{
	if (!initialized_ || (buffer == nullptr && count != 0) || byteTimeoutMs < 0)
	{
		return -1;
	}
	if (count == 0)
	{
		return 0;
	}

	serial_.setTimeout(static_cast<unsigned long>(byteTimeoutMs));
	return static_cast<std::int32_t>(serial_.readBytes(buffer, count));
}

std::int32_t Esp32ModbusSerialTransport::writeBytes(const std::uint8_t *const buffer,
													   const std::uint16_t count,
													   const std::int32_t byteTimeoutMs) noexcept
{
	if (!initialized_ || (buffer == nullptr && count != 0) || byteTimeoutMs < 0)
	{
		return -1;
	}

	std::size_t written{0};
	const auto startedAtMs = millis();
	while (written < count)
	{
		const auto available = serial_.availableForWrite();
		if (available > 0)
		{
			const auto chunkSize = std::min<std::size_t>(static_cast<std::size_t>(available), count - written);
			const auto chunkWritten = serial_.write(buffer + written, chunkSize);
			written += chunkWritten;
			if (chunkWritten == 0)
			{
				break;
			}
		}
		if (written == count || static_cast<std::uint32_t>(millis() - startedAtMs) >= static_cast<std::uint32_t>(byteTimeoutMs))
		{
			break;
		}
		yield();
	}
	return static_cast<std::int32_t>(written);
}

void Esp32ModbusSerialTransport::shutdown() noexcept
{
	if (!initialized_)
	{
		return;
	}
	serial_.end();
	initialized_ = false;
}
}