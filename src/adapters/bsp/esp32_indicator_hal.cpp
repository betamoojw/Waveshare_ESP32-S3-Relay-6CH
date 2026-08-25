#include "esp32_indicator_hal.h"

#include <Arduino.h>

namespace switch_actuator::adapters::bsp
{
Esp32RgbLedHal::Esp32RgbLedHal(const hal::BoardDescriptor &descriptor) noexcept
	: descriptor_{descriptor}
{
}

hal::IIndicator Esp32RgbLedHal::hal() noexcept
{
	return {writeCallback, this};
}

bool Esp32RgbLedHal::writeCallback(void *const context,
	const std::uint8_t red,
	const std::uint8_t green,
	const std::uint8_t blue) noexcept
{
	if (context == nullptr)
	{
		return false;
	}
	const auto pin = static_cast<Esp32RgbLedHal *>(context)->descriptor_.indicators.rgbLedPin;
	rgbLedWrite(pin, red, green, blue);
	return true;
}

Esp32BuzzerHal::Esp32BuzzerHal(const hal::BoardDescriptor &descriptor) noexcept
	: descriptor_{descriptor}
{
}

Esp32BuzzerHal::~Esp32BuzzerHal()
{
	if (initialized_)
	{
		static_cast<void>(ledcWriteTone(descriptor_.indicators.buzzerPin, 0));
		static_cast<void>(ledcDetach(descriptor_.indicators.buzzerPin));
	}
}

hal::IBuzzer Esp32BuzzerHal::hal() noexcept
{
	return {initializeCallback, writeCallback, this};
}

bool Esp32BuzzerHal::initializeCallback(void *const context) noexcept
{
	if (context == nullptr)
	{
		return false;
	}
	auto &driver = *static_cast<Esp32BuzzerHal *>(context);
	driver.initialized_ = ledcAttach(driver.descriptor_.indicators.buzzerPin, baseFrequencyHz, resolutionBits);
	return driver.initialized_;
}

bool Esp32BuzzerHal::writeCallback(void *const context,
	const std::uint16_t frequencyHz,
	const std::uint8_t dutyPercent) noexcept
{
	if (context == nullptr)
	{
		return false;
	}
	const auto &driver = *static_cast<Esp32BuzzerHal *>(context);
	if (!driver.initialized_)
	{
		return false;
	}
	const auto appliedFrequency = ledcWriteTone(driver.descriptor_.indicators.buzzerPin, frequencyHz);
	if (frequencyHz != 0 && appliedFrequency == 0)
	{
		return false;
	}
	if (frequencyHz == 0)
	{
		return true;
	}
	const auto duty = static_cast<std::uint32_t>((255U * dutyPercent) / 100U);
	return ledcWrite(driver.descriptor_.indicators.buzzerPin, duty);
}
}