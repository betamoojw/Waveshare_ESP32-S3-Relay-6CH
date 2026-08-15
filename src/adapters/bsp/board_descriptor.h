#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace switch_actuator::adapters::bsp
{
enum class RelayPolarity : std::uint8_t
{
	ActiveLow,
	ActiveHigh
};

enum class ButtonPullMode : std::uint8_t
{
	None,
	PullUp,
	PullDown
};

enum class EthernetImplementation : std::uint8_t { None, InternalMacPhy, SpiController };

struct BoardDescriptor final
{
	static constexpr std::size_t relayChannelCount{6};

	std::string_view model;
	std::string_view hardwareRevision;
	std::array<std::uint8_t, relayChannelCount> relayPins;
	RelayPolarity relayPolarity;
	std::uint8_t bootButtonPin;
	ButtonPullMode bootButtonPullMode;
	bool bootButtonActiveLow;
	std::uint8_t modbusTxPin;
	std::uint8_t modbusRxPin;
	std::uint8_t buzzerPin;
	std::uint8_t rgbLedPin;
	bool wifiSupported;
	bool ethernetSupported;
	EthernetImplementation ethernetImplementation;

	[[nodiscard]] constexpr bool relayActiveLevel() const noexcept
	{
		return relayPolarity == RelayPolarity::ActiveHigh;
	}

	[[nodiscard]] constexpr bool relayInactiveLevel() const noexcept
	{
		return !relayActiveLevel();
	}
};
}