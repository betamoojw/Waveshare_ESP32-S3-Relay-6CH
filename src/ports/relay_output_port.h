#pragma once

#include "../domain/relay_types.h"

namespace switch_actuator::ports
{
enum class RelayOutputResult : std::uint8_t
{
	Applied,
	HardwareFailure
};

using RelayOutputHandler = RelayOutputResult (*)(void *context,
												 domain::RelayChannelId channel,
												 domain::RelayState state) noexcept;

class RelayOutputPort final
{
public:
	constexpr RelayOutputPort() noexcept = default;

	constexpr RelayOutputPort(RelayOutputHandler handler, void *context = nullptr) noexcept
		: handler_{handler}, context_{context}
	{
	}

	[[nodiscard]] RelayOutputResult apply(const domain::RelayChannelId channel, const domain::RelayState state) const noexcept
	{
		return handler_ != nullptr ? handler_(context_, channel, state) : RelayOutputResult::HardwareFailure;
	}

	[[nodiscard]] constexpr bool isValid() const noexcept
	{
		return handler_ != nullptr;
	}

private:
	RelayOutputHandler handler_{nullptr};
	void *context_{nullptr};
};
}