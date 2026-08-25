#pragma once

#include "../../hal/BoardDescriptor.h"
#include "../../hal/BuzzerHal.h"
#include "../../hal/RgbLedHal.h"

namespace switch_actuator::adapters::bsp
{
class Esp32RgbLedHal final
{
public:
	explicit Esp32RgbLedHal(const hal::BoardDescriptor &descriptor) noexcept;
	[[nodiscard]] hal::IIndicator hal() noexcept;

private:
	[[nodiscard]] static bool writeCallback(void *context,
		std::uint8_t red,
		std::uint8_t green,
		std::uint8_t blue) noexcept;
	const hal::BoardDescriptor &descriptor_;
};

class Esp32BuzzerHal final
{
public:
	explicit Esp32BuzzerHal(const hal::BoardDescriptor &descriptor) noexcept;
	~Esp32BuzzerHal();
	[[nodiscard]] hal::IBuzzer hal() noexcept;

private:
	[[nodiscard]] static bool initializeCallback(void *context) noexcept;
	[[nodiscard]] static bool writeCallback(void *context,
		std::uint16_t frequencyHz,
		std::uint8_t dutyPercent) noexcept;

	static constexpr std::uint32_t baseFrequencyHz{2000};
	static constexpr std::uint8_t resolutionBits{8};
	const hal::BoardDescriptor &descriptor_;
	bool initialized_{false};
};
}