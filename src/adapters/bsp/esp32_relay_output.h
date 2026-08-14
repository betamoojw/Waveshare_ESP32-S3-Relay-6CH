#pragma once

#include "board_descriptor.h"
#include "../../ports/relay_output_port.h"

#include <array>
#include <cstddef>

namespace switch_actuator::adapters::bsp
{
enum class RelayOutputResult : std::uint8_t
{
	Applied,
	InvalidChannel,
	NotInitialized,
	HardwareFailure
};

class Esp32RelayOutput final
{
public:
	explicit Esp32RelayOutput(const BoardDescriptor &descriptor) noexcept;
	~Esp32RelayOutput();

	Esp32RelayOutput(const Esp32RelayOutput &) = delete;
	Esp32RelayOutput &operator=(const Esp32RelayOutput &) = delete;
	Esp32RelayOutput(Esp32RelayOutput &&) = delete;
	Esp32RelayOutput &operator=(Esp32RelayOutput &&) = delete;

	[[nodiscard]] RelayOutputResult initialize() noexcept;
	[[nodiscard]] ports::RelayOutputPort port() noexcept;
	[[nodiscard]] RelayOutputResult setChannel(std::size_t channel, bool enabled) noexcept;
	[[nodiscard]] RelayOutputResult allOff() noexcept;
	[[nodiscard]] bool channelState(std::size_t channel) const noexcept;
	[[nodiscard]] bool isInitialized() const noexcept;

private:
	[[nodiscard]] static ports::RelayOutputResult applyCallback(void *context,
															  domain::RelayChannelId channel,
															  domain::RelayState state) noexcept;
	[[nodiscard]] RelayOutputResult writeChannel(std::size_t channel, bool enabled) noexcept;

	const BoardDescriptor &descriptor_;
	std::array<bool, BoardDescriptor::relayChannelCount> channelStates_{};
	bool initialized_{false};
};
}