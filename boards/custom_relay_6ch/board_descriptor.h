#pragma once

#include "../../src/hal/BoardDescriptor.h"

#include <array>

#if !defined(CUSTOM_RELAY_PIN_1) || !defined(CUSTOM_RELAY_PIN_2) || !defined(CUSTOM_RELAY_PIN_3) || \
	!defined(CUSTOM_RELAY_PIN_4) || !defined(CUSTOM_RELAY_PIN_5) || !defined(CUSTOM_RELAY_PIN_6) || \
	!defined(CUSTOM_BUTTON_PIN) || !defined(CUSTOM_RGB_PIN) || !defined(CUSTOM_BUZZER_PIN) || \
	!defined(CUSTOM_RS485_TX_PIN) || !defined(CUSTOM_RS485_RX_PIN)
#error "The custom 6-channel board requires explicit CUSTOM_* GPIO build definitions"
#endif

namespace switch_actuator::boards::custom_relay_6ch
{
inline constexpr std::array<std::uint8_t, 6> relayPins{
	CUSTOM_RELAY_PIN_1, CUSTOM_RELAY_PIN_2, CUSTOM_RELAY_PIN_3,
	CUSTOM_RELAY_PIN_4, CUSTOM_RELAY_PIN_5, CUSTOM_RELAY_PIN_6};

inline constexpr hal::BoardDescriptor descriptor{
	"CUSTOM-RELAY-6CH",
	"Custom ESP32 Relay 6CH",
	"HW-A01",
	static_cast<std::uint8_t>(relayPins.size()),
	relayPins.data(),
	hal::RelayPolarity::ActiveHigh,
	{CUSTOM_BUTTON_PIN, hal::ButtonPullMode::PullUp, true},
	{CUSTOM_RGB_PIN, CUSTOM_BUZZER_PIN},
	{CUSTOM_RS485_TX_PIN, CUSTOM_RS485_RX_PIN, true},
	{true, false, hal::EthernetImplementation::None},
};

static_assert(hal::isValid(descriptor), "Invalid custom 6-channel board descriptor");
}