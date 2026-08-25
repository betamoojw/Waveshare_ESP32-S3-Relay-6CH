#pragma once

#include <cstdint>

namespace switch_actuator::hal
{
using BuzzerInitializeHandler = bool (*)(void *context) noexcept;
using BuzzerWriteHandler = bool (*)(void *context, std::uint16_t frequencyHz, std::uint8_t dutyPercent) noexcept;

class IBuzzer final
{
public:
	constexpr IBuzzer() noexcept = default;
	constexpr IBuzzer(const BuzzerInitializeHandler initialize,
		const BuzzerWriteHandler write,
		void *const context = nullptr) noexcept
		: initialize_{initialize}, write_{write}, context_{context}
	{
	}
	[[nodiscard]] bool initialize() const noexcept { return initialize_ != nullptr && initialize_(context_); }
	[[nodiscard]] bool write(const std::uint16_t frequencyHz, const std::uint8_t dutyPercent) const noexcept
	{
		return write_ != nullptr && write_(context_, frequencyHz, dutyPercent);
	}
	[[nodiscard]] constexpr bool isValid() const noexcept { return initialize_ != nullptr && write_ != nullptr; }
private:
	BuzzerInitializeHandler initialize_{nullptr};
	BuzzerWriteHandler write_{nullptr};
	void *context_{nullptr};
};

using BuzzerHal = IBuzzer;
}