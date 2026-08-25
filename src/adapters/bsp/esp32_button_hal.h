#pragma once

#include "../../hal/BoardDescriptor.h"
#include "../../hal/ButtonHal.h"

namespace switch_actuator::adapters::bsp
{
class Esp32ButtonHal final
{
public:
	explicit Esp32ButtonHal(const hal::BoardDescriptor &descriptor) noexcept;
	[[nodiscard]] hal::IButton hal() noexcept;

private:
	[[nodiscard]] static bool initializeCallback(void *context) noexcept;
	[[nodiscard]] static bool pressedCallback(void *context) noexcept;

	const hal::BoardDescriptor &descriptor_;
};
}