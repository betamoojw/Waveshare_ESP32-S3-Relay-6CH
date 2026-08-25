#include "esp32_button_hal.h"

#include <Arduino.h>

namespace switch_actuator::adapters::bsp
{
namespace
{
[[nodiscard]] std::uint8_t toArduinoPinMode(const hal::ButtonPullMode pullMode) noexcept
{
	switch (pullMode)
	{
	case hal::ButtonPullMode::PullUp:
		return INPUT_PULLUP;
	case hal::ButtonPullMode::PullDown:
		return INPUT_PULLDOWN;
	case hal::ButtonPullMode::None:
		return INPUT;
	}
	return INPUT;
}
}

Esp32ButtonHal::Esp32ButtonHal(const hal::BoardDescriptor &descriptor) noexcept
	: descriptor_{descriptor}
{
}

hal::IButton Esp32ButtonHal::hal() noexcept
{
	return {initializeCallback, pressedCallback, this};
}

bool Esp32ButtonHal::initializeCallback(void *const context) noexcept
{
	if (context == nullptr)
	{
		return false;
	}
	const auto &descriptor = static_cast<Esp32ButtonHal *>(context)->descriptor_;
	pinMode(descriptor.button.pin, toArduinoPinMode(descriptor.button.pullMode));
	return true;
}

bool Esp32ButtonHal::pressedCallback(void *const context) noexcept
{
	if (context == nullptr)
	{
		return false;
	}
	const auto &descriptor = static_cast<Esp32ButtonHal *>(context)->descriptor_;
	const auto levelIsHigh = digitalRead(descriptor.button.pin) == HIGH;
	return descriptor.button.activeLow ? !levelIsHigh : levelIsHigh;
}
}