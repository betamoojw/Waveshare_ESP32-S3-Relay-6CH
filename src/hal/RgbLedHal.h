#pragma once

#include <cstdint>

namespace switch_actuator::hal
{
using RgbLedWriteHandler = bool (*)(void *context, std::uint8_t red, std::uint8_t green, std::uint8_t blue) noexcept;

class IIndicator final
{
public:
	constexpr IIndicator() noexcept = default;
	constexpr IIndicator(const RgbLedWriteHandler write, void *const context = nullptr) noexcept
		: write_{write}, context_{context}
	{
	}
	[[nodiscard]] bool write(const std::uint8_t red, const std::uint8_t green, const std::uint8_t blue) const noexcept
	{
		return write_ != nullptr && write_(context_, red, green, blue);
	}
	[[nodiscard]] constexpr bool isValid() const noexcept { return write_ != nullptr; }
private:
	RgbLedWriteHandler write_{nullptr};
	void *context_{nullptr};
};

using RgbLedHal = IIndicator;
}