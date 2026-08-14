#include "waveshare_esp32s3_relay6ch.h"

#include <cstddef>

namespace switch_actuator::adapters::bsp
{
namespace
{
[[nodiscard]] constexpr bool relayPinsAreUnique() noexcept
{
	for (std::size_t current = 0; current < waveshareEsp32S3Relay6Ch.relayPins.size(); ++current)
	{
		for (std::size_t candidate = current + 1; candidate < waveshareEsp32S3Relay6Ch.relayPins.size(); ++candidate)
		{
			if (waveshareEsp32S3Relay6Ch.relayPins[current] == waveshareEsp32S3Relay6Ch.relayPins[candidate])
			{
				return false;
			}
		}
	}

	return true;
}

[[nodiscard]] constexpr bool bootPinIsNotAnOutput() noexcept
{
	for (const auto relayPin : waveshareEsp32S3Relay6Ch.relayPins)
	{
		if (relayPin == waveshareEsp32S3Relay6Ch.bootButtonPin)
		{
			return false;
		}
	}

	return waveshareEsp32S3Relay6Ch.bootButtonPin != waveshareEsp32S3Relay6Ch.buzzerPin &&
		   waveshareEsp32S3Relay6Ch.bootButtonPin != waveshareEsp32S3Relay6Ch.rgbLedPin &&
		   waveshareEsp32S3Relay6Ch.bootButtonPin != waveshareEsp32S3Relay6Ch.modbusTxPin;
}
}

static_assert(relayPinsAreUnique(), "Relay GPIO assignments must be unique");
static_assert(bootPinIsNotAnOutput(), "GPIO0 must not be assigned to an output");
static_assert(waveshareEsp32S3Relay6Ch.modbusTxPin != waveshareEsp32S3Relay6Ch.modbusRxPin,
			  "Modbus TX and RX pins must be different");
}