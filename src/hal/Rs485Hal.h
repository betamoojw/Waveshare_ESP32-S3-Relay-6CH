#pragma once

#include <cstdint>

namespace switch_actuator::hal
{
using Rs485ReadHandler = std::int32_t (*)(void *context, std::uint8_t *buffer,
	std::uint16_t count, std::int32_t byteTimeoutMs) noexcept;
using Rs485WriteHandler = std::int32_t (*)(void *context, const std::uint8_t *buffer,
	std::uint16_t count, std::int32_t byteTimeoutMs) noexcept;

struct IUart final
{
	Rs485ReadHandler read{nullptr};
	Rs485WriteHandler write{nullptr};
	void *context{nullptr};

	[[nodiscard]] constexpr bool isValid() const noexcept { return read != nullptr && write != nullptr; }
};

using Rs485Hal = IUart;
}