#pragma once

#include "../domain/relay_types.h"

namespace switch_actuator::hal
{
enum class RelayHalResult : std::uint8_t { Applied, HardwareFailure };
using RelayApplyHandler = RelayHalResult (*)(void *context,
	domain::RelayChannelId channel,
	domain::RelayState state) noexcept;

class RelayHal final
{
public:
	constexpr RelayHal() noexcept = default;
	constexpr RelayHal(const RelayApplyHandler handler, void *const context = nullptr) noexcept
		: handler_{handler}, context_{context}
	{
	}

	[[nodiscard]] RelayHalResult apply(const domain::RelayChannelId channel,
		const domain::RelayState state) const noexcept
	{
		return handler_ != nullptr ? handler_(context_, channel, state) : RelayHalResult::HardwareFailure;
	}

	[[nodiscard]] constexpr bool isValid() const noexcept { return handler_ != nullptr; }

private:
	RelayApplyHandler handler_{nullptr};
	void *context_{nullptr};
};
}