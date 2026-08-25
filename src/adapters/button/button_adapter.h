#pragma once

#include "../../hal/ButtonHal.h"

#include <cstdint>

namespace switch_actuator::adapters::button
{
enum class ButtonEventType : std::uint8_t
{
	IdentifyRequested,
	CommissioningRequested,
	FactoryResetArmed,
	FactoryResetRequested
};

struct ButtonEvent final
{
	ButtonEventType type;
	std::uint32_t occurredAtMs;
	std::uint32_t heldForMs;
};

using ButtonEventHandler = bool (*)(const ButtonEvent &event, void *context) noexcept;

enum class ButtonInitializeResult : std::uint8_t
{
	Initialized,
	InvalidHandler,
	InvalidHal,
	HardwareFailure
};

enum class ButtonUpdateResult : std::uint8_t
{
	Idle,
	EventEmitted,
	EventRejected,
	NotInitialized
};

class ButtonAdapter final
{
public:
	ButtonAdapter(hal::IButton buttonHal, ButtonEventHandler eventHandler, void *eventContext = nullptr) noexcept;

	[[nodiscard]] ButtonInitializeResult initialize(std::uint32_t nowMs) noexcept;
	[[nodiscard]] ButtonUpdateResult update(std::uint32_t nowMs) noexcept;
	[[nodiscard]] bool isInitialized() const noexcept;
	[[nodiscard]] bool isPressed() const noexcept;

private:
	[[nodiscard]] bool readPressed() const noexcept;
	[[nodiscard]] ButtonUpdateResult emit(ButtonEventType type, std::uint32_t nowMs, std::uint32_t heldForMs) const noexcept;
	[[nodiscard]] ButtonUpdateResult handleRelease(std::uint32_t nowMs) noexcept;

	static constexpr std::uint32_t bootQualificationDurationMs{1000};
	static constexpr std::uint32_t debounceDurationMs{30};
	static constexpr std::uint32_t commissioningHoldDurationMs{3000};
	static constexpr std::uint32_t factoryResetHoldDurationMs{10000};

	hal::IButton buttonHal_;
	ButtonEventHandler eventHandler_;
	void *eventContext_;
	std::uint32_t initializedAtMs_{0};
	std::uint32_t rawStateChangedAtMs_{0};
	std::uint32_t pressedAtMs_{0};
	bool rawPressed_{false};
	bool stablePressed_{false};
	bool bootQualified_{false};
	bool suppressUntilRelease_{false};
	bool factoryResetArmed_{false};
	bool initialized_{false};
};
}