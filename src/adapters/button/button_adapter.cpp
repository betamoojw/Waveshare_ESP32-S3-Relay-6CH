#include "button_adapter.h"

namespace switch_actuator::adapters::button
{
ButtonAdapter::ButtonAdapter(const hal::ButtonHal buttonHal, const ButtonEventHandler eventHandler, void *const eventContext) noexcept
	: buttonHal_{buttonHal}, eventHandler_{eventHandler}, eventContext_{eventContext}
{
}

ButtonInitializeResult ButtonAdapter::initialize(const std::uint32_t nowMs) noexcept
{
	if (eventHandler_ == nullptr)
	{
		return ButtonInitializeResult::InvalidHandler;
	}
	if (!buttonHal_.isValid())
	{
		return ButtonInitializeResult::InvalidHal;
	}
	if (!buttonHal_.initialize())
	{
		return ButtonInitializeResult::HardwareFailure;
	}

	rawPressed_ = readPressed();
	stablePressed_ = rawPressed_;
	initializedAtMs_ = nowMs;
	rawStateChangedAtMs_ = nowMs;
	pressedAtMs_ = nowMs;
	bootQualified_ = false;
	suppressUntilRelease_ = rawPressed_;
	factoryResetArmed_ = false;
	initialized_ = true;
	return ButtonInitializeResult::Initialized;
}

ButtonUpdateResult ButtonAdapter::update(const std::uint32_t nowMs) noexcept
{
	if (!initialized_)
	{
		return ButtonUpdateResult::NotInitialized;
	}

	const auto pressed = readPressed();
	if (!bootQualified_)
	{
		rawPressed_ = pressed;
		stablePressed_ = pressed;
		rawStateChangedAtMs_ = nowMs;
		suppressUntilRelease_ = pressed;
		if (nowMs - initializedAtMs_ < bootQualificationDurationMs)
		{
			return ButtonUpdateResult::Idle;
		}

		bootQualified_ = true;
		pressedAtMs_ = nowMs;
		return ButtonUpdateResult::Idle;
	}

	if (pressed != rawPressed_)
	{
		rawPressed_ = pressed;
		rawStateChangedAtMs_ = nowMs;
	}

	if (rawPressed_ != stablePressed_ && nowMs - rawStateChangedAtMs_ >= debounceDurationMs)
	{
		stablePressed_ = rawPressed_;
		if (stablePressed_)
		{
			pressedAtMs_ = nowMs;
			factoryResetArmed_ = false;
			return ButtonUpdateResult::Idle;
		}

		return handleRelease(nowMs);
	}

	if (stablePressed_ && !suppressUntilRelease_ && !factoryResetArmed_ && nowMs - pressedAtMs_ >= factoryResetHoldDurationMs)
	{
		const auto result = emit(ButtonEventType::FactoryResetArmed, nowMs, nowMs - pressedAtMs_);
		factoryResetArmed_ = result == ButtonUpdateResult::EventEmitted;
		return result;
	}

	return ButtonUpdateResult::Idle;
}

bool ButtonAdapter::isInitialized() const noexcept
{
	return initialized_;
}

bool ButtonAdapter::isPressed() const noexcept
{
	return initialized_ && stablePressed_;
}

bool ButtonAdapter::readPressed() const noexcept
{
	return buttonHal_.isPressed();
}

ButtonUpdateResult ButtonAdapter::emit(const ButtonEventType type,
									   const std::uint32_t nowMs,
									   const std::uint32_t heldForMs) const noexcept
{
	return eventHandler_(ButtonEvent{type, nowMs, heldForMs}, eventContext_) ? ButtonUpdateResult::EventEmitted
																	   : ButtonUpdateResult::EventRejected;
}

ButtonUpdateResult ButtonAdapter::handleRelease(const std::uint32_t nowMs) noexcept
{
	const auto heldForMs = nowMs - pressedAtMs_;
	if (suppressUntilRelease_)
	{
		suppressUntilRelease_ = false;
		factoryResetArmed_ = false;
		return ButtonUpdateResult::Idle;
	}

	if (factoryResetArmed_)
	{
		factoryResetArmed_ = false;
		return emit(ButtonEventType::FactoryResetRequested, nowMs, heldForMs);
	}
	if (heldForMs >= commissioningHoldDurationMs)
	{
		return emit(ButtonEventType::CommissioningRequested, nowMs, heldForMs);
	}

	return emit(ButtonEventType::IdentifyRequested, nowMs, heldForMs);
}
}