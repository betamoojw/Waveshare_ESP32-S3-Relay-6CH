#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace switch_actuator::hal
{
inline constexpr std::size_t maximumRelayCount{12};
inline constexpr std::uint8_t invalidGpio{0xFF};

enum class RelayPolarity : std::uint8_t { ActiveLow, ActiveHigh };
enum class ButtonPullMode : std::uint8_t { None, PullUp, PullDown };
enum class EthernetImplementation : std::uint8_t { None, InternalMacPhy, SpiController };

struct ButtonConfig final
{
	std::uint8_t pin{invalidGpio};
	ButtonPullMode pullMode{ButtonPullMode::None};
	bool activeLow{true};
};

struct IndicatorConfig final
{
	std::uint8_t rgbLedPin{invalidGpio};
	std::uint8_t buzzerPin{invalidGpio};
};

struct Rs485Config final
{
	std::uint8_t txPin{invalidGpio};
	std::uint8_t rxPin{invalidGpio};
	bool available{false};
};

struct NetworkCapabilities final
{
	bool wifi{false};
	bool ethernet{false};
	EthernetImplementation ethernetImplementation{EthernetImplementation::None};
};

struct BoardDescriptor final
{
	std::string_view productId;
	std::string_view model;
	std::string_view hardwareRevision;
	std::uint8_t relayCount{0};
	const std::uint8_t *relayPins{nullptr};
	RelayPolarity relayPolarity;
	ButtonConfig button{};
	IndicatorConfig indicators{};
	Rs485Config rs485{};
	NetworkCapabilities network{};

	[[nodiscard]] constexpr bool relayActiveLevel() const noexcept
	{
		return relayPolarity == RelayPolarity::ActiveHigh;
	}

	[[nodiscard]] constexpr bool relayInactiveLevel() const noexcept
	{
		return !relayActiveLevel();
	}

	[[nodiscard]] constexpr std::uint8_t relayPin(const std::size_t channel) const noexcept
	{
		return relayPins != nullptr && channel < relayCount ? relayPins[channel] : invalidGpio;
	}
};

[[nodiscard]] constexpr bool isValid(const BoardDescriptor &descriptor) noexcept
{
	if (descriptor.productId.empty() || descriptor.model.empty() || descriptor.hardwareRevision.empty() ||
		descriptor.hardwareRevision.size() < 4U || descriptor.hardwareRevision.substr(0, 3) != "HW-" ||
		descriptor.relayPins == nullptr || descriptor.relayCount == 0 || descriptor.relayCount > maximumRelayCount ||
		descriptor.button.pin == invalidGpio || descriptor.indicators.rgbLedPin == invalidGpio ||
		descriptor.indicators.buzzerPin == invalidGpio ||
		descriptor.button.pin == descriptor.indicators.rgbLedPin ||
		descriptor.button.pin == descriptor.indicators.buzzerPin ||
		descriptor.indicators.rgbLedPin == descriptor.indicators.buzzerPin ||
		descriptor.network.ethernet !=
			(descriptor.network.ethernetImplementation != EthernetImplementation::None))
	{
		return false;
	}
	for (std::size_t current = 0; current < descriptor.relayCount; ++current)
	{
		if (descriptor.relayPins[current] == invalidGpio || descriptor.relayPins[current] == descriptor.button.pin ||
			descriptor.relayPins[current] == descriptor.indicators.rgbLedPin ||
			descriptor.relayPins[current] == descriptor.indicators.buzzerPin ||
			(descriptor.rs485.available &&
			 (descriptor.relayPins[current] == descriptor.rs485.txPin || descriptor.relayPins[current] == descriptor.rs485.rxPin)))
		{
			return false;
		}
		for (std::size_t candidate = current + 1; candidate < descriptor.relayCount; ++candidate)
		{
			if (descriptor.relayPins[current] == descriptor.relayPins[candidate])
			{
				return false;
			}
		}
	}
	return !descriptor.rs485.available ||
		(descriptor.rs485.txPin != invalidGpio && descriptor.rs485.rxPin != invalidGpio &&
			descriptor.rs485.txPin != descriptor.rs485.rxPin &&
			descriptor.rs485.txPin != descriptor.button.pin && descriptor.rs485.rxPin != descriptor.button.pin &&
			descriptor.rs485.txPin != descriptor.indicators.rgbLedPin &&
			descriptor.rs485.rxPin != descriptor.indicators.rgbLedPin &&
			descriptor.rs485.txPin != descriptor.indicators.buzzerPin &&
			descriptor.rs485.rxPin != descriptor.indicators.buzzerPin);
}

[[nodiscard]] constexpr bool supportsRelayCount(const BoardDescriptor &descriptor,
												 const std::size_t compiledRelayCount) noexcept
{
	return isValid(descriptor) && descriptor.relayCount == compiledRelayCount;
}
}