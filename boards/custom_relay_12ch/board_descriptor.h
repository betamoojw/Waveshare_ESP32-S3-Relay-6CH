#pragma once

#include "../../src/hal/BoardDescriptor.h"

#include <array>

#if !defined(CUSTOM_RELAY_PIN_1) || !defined(CUSTOM_RELAY_PIN_2) || !defined(CUSTOM_RELAY_PIN_3) || \
	!defined(CUSTOM_RELAY_PIN_4) || !defined(CUSTOM_RELAY_PIN_5) || !defined(CUSTOM_RELAY_PIN_6) || \
	!defined(CUSTOM_RELAY_PIN_7) || !defined(CUSTOM_RELAY_PIN_8) || !defined(CUSTOM_RELAY_PIN_9) || \
	!defined(CUSTOM_RELAY_PIN_10) || !defined(CUSTOM_RELAY_PIN_11) || !defined(CUSTOM_RELAY_PIN_12) || \
	!defined(CUSTOM_BUTTON_PIN) || !defined(CUSTOM_RGB_PIN) || !defined(CUSTOM_BUZZER_PIN) || \
	!defined(CUSTOM_RS485_TX_PIN) || !defined(CUSTOM_RS485_RX_PIN)
#error "The custom 12-channel board requires explicit CUSTOM_* GPIO build definitions"
#endif

namespace switch_actuator::boards::custom_relay_12ch
{
inline constexpr std::array<std::uint8_t, 12> relayPins{
	CUSTOM_RELAY_PIN_1, CUSTOM_RELAY_PIN_2, CUSTOM_RELAY_PIN_3, CUSTOM_RELAY_PIN_4,
	CUSTOM_RELAY_PIN_5, CUSTOM_RELAY_PIN_6, CUSTOM_RELAY_PIN_7, CUSTOM_RELAY_PIN_8,
	CUSTOM_RELAY_PIN_9, CUSTOM_RELAY_PIN_10, CUSTOM_RELAY_PIN_11, CUSTOM_RELAY_PIN_12};

inline constexpr hal::BoardDescriptor descriptor{
	"CUSTOM-RELAY-12CH",
	"Custom ESP32 Relay 12CH",
	"HW-A01",
	static_cast<std::uint8_t>(relayPins.size()),
	relayPins.data(),
	hal::RelayPolarity::ActiveHigh,
	{CUSTOM_BUTTON_PIN, hal::ButtonPullMode::PullUp, true},
	{CUSTOM_RGB_PIN, CUSTOM_BUZZER_PIN},
	{CUSTOM_RS485_TX_PIN, CUSTOM_RS485_RX_PIN, true},
	{true, false, hal::EthernetImplementation::None},
};

static_assert(hal::isValid(descriptor), "Invalid custom 12-channel board descriptor");
}