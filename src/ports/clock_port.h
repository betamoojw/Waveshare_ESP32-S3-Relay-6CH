#pragma once

#include <cstdint>

namespace switch_actuator::ports
{
using MonotonicMillisecondsHandler = std::uint32_t (*)(void *context) noexcept;

class ClockPort final
{
public:
	constexpr ClockPort() noexcept = default;

	constexpr ClockPort(MonotonicMillisecondsHandler handler, void *context = nullptr) noexcept
		: handler_{handler}, context_{context}
	{
	}

	[[nodiscard]] std::uint32_t nowMs() const noexcept
	{
		return handler_ != nullptr ? handler_(context_) : 0;
	}

	[[nodiscard]] constexpr bool isValid() const noexcept
	{
		return handler_ != nullptr;
	}

private:
	MonotonicMillisecondsHandler handler_{nullptr};
	void *context_{nullptr};
};
}