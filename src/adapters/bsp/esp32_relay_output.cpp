#include "esp32_relay_output.h"

#include <driver/gpio.h>

namespace switch_actuator::adapters::bsp
{
namespace
{
[[nodiscard]] constexpr gpio_num_t toGpioNumber(const std::uint8_t pin) noexcept
{
	return static_cast<gpio_num_t>(pin);
}
}

Esp32RelayOutput::Esp32RelayOutput(const hal::BoardDescriptor &descriptor) noexcept
	: descriptor_{descriptor}
{
}

Esp32RelayOutput::~Esp32RelayOutput()
{
	if (initialized_)
	{
		static_cast<void>(allOff());
	}
}

RelayOutputResult Esp32RelayOutput::initialize() noexcept
{
	initialized_ = false;
	channelStates_.fill(false);
	if (!hal::isValid(descriptor_))
	{
		return RelayOutputResult::HardwareFailure;
	}

	const auto inactiveLevel = static_cast<std::uint32_t>(descriptor_.relayInactiveLevel());
	for (std::size_t channel = 0; channel < descriptor_.relayCount; ++channel)
	{
		const auto pin = descriptor_.relayPin(channel);
		const auto gpio = toGpioNumber(pin);
		if (!GPIO_IS_VALID_OUTPUT_GPIO(gpio) || gpio_set_level(gpio, inactiveLevel) != ESP_OK ||
			gpio_set_direction(gpio, GPIO_MODE_OUTPUT) != ESP_OK || gpio_set_level(gpio, inactiveLevel) != ESP_OK)
		{
			for (std::size_t safeChannel = 0; safeChannel < descriptor_.relayCount; ++safeChannel)
			{
				const auto safePin = descriptor_.relayPin(safeChannel);
				const auto safeGpio = toGpioNumber(safePin);
				if (GPIO_IS_VALID_OUTPUT_GPIO(safeGpio))
				{
					static_cast<void>(gpio_set_level(safeGpio, inactiveLevel));
				}
			}
			return RelayOutputResult::HardwareFailure;
		}
	}

	initialized_ = true;
	return RelayOutputResult::Applied;
}

hal::RelayHal Esp32RelayOutput::hal() noexcept
{
	return {applyCallback, this};
}

ports::RelayOutputPort Esp32RelayOutput::port() noexcept
{
	return hal();
}

hal::RelayHalResult Esp32RelayOutput::applyCallback(void *const context,
																	 const domain::RelayChannelId channel,
																	 const domain::RelayState state) noexcept
{
	if (context == nullptr)
	{
		return hal::RelayHalResult::HardwareFailure;
	}

	auto &output = *static_cast<Esp32RelayOutput *>(context);
	const auto result = output.setChannel(channel.value, state == domain::RelayState::On);
	return result == RelayOutputResult::Applied ? hal::RelayHalResult::Applied : hal::RelayHalResult::HardwareFailure;
}

RelayOutputResult Esp32RelayOutput::setChannel(const std::size_t channel, const bool enabled) noexcept
{
	if (channel >= descriptor_.relayCount)
	{
		return RelayOutputResult::InvalidChannel;
	}
	if (!initialized_)
	{
		return RelayOutputResult::NotInitialized;
	}

	return writeChannel(channel, enabled);
}

RelayOutputResult Esp32RelayOutput::allOff() noexcept
{
	if (!initialized_)
	{
		return RelayOutputResult::NotInitialized;
	}

	for (std::size_t channel = 0; channel < descriptor_.relayCount; ++channel)
	{
		if (writeChannel(channel, false) != RelayOutputResult::Applied)
		{
			return RelayOutputResult::HardwareFailure;
		}
	}

	return RelayOutputResult::Applied;
}

bool Esp32RelayOutput::channelState(const std::size_t channel) const noexcept
{
	return channel < channelStates_.size() && channelStates_[channel];
}

bool Esp32RelayOutput::isInitialized() const noexcept
{
	return initialized_;
}

RelayOutputResult Esp32RelayOutput::writeChannel(const std::size_t channel, const bool enabled) noexcept
{
	const auto level = static_cast<std::uint32_t>(enabled ? descriptor_.relayActiveLevel() : descriptor_.relayInactiveLevel());
	if (gpio_set_level(toGpioNumber(descriptor_.relayPin(channel)), level) != ESP_OK)
	{
		return RelayOutputResult::HardwareFailure;
	}

	channelStates_[channel] = enabled;
	return RelayOutputResult::Applied;
}
}