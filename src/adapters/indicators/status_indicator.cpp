#include "status_indicator.h"

#include <array>

namespace switch_actuator::adapters::indicators
{
namespace
{
constexpr std::uint32_t commissioningPeriodMs{1000};
constexpr std::uint32_t degradedBusPeriodMs{1000};
constexpr std::uint32_t commandToneDurationMs{60};
constexpr std::array<std::uint16_t, 8> maintenanceToneFrequencies{0, 523, 659, 784, 1047, 1319, 1568, 2093};
}

StatusIndicator::StatusIndicator(const hal::IIndicator indicator, const hal::IBuzzer buzzer) noexcept
	: indicator_{indicator}, buzzer_{buzzer}
{
}

StatusIndicator::~StatusIndicator()
{
	if (initialized_)
	{
		silenceAndTurnOff();
	}
}

IndicatorResult StatusIndicator::initialize() noexcept
{
	initialized_ = false;
	if (!indicator_.isValid() || !buzzer_.isValid() || !indicator_.write(0, 0, 0) || !buzzer_.initialize())
	{
		return IndicatorResult::HardwareFailure;
	}

	if (!buzzer_.write(0, 0))
	{
		return IndicatorResult::HardwareFailure;
	}
	appliedOutput_ = offOutput;
	initialized_ = true;
	return IndicatorResult::Applied;
}

void StatusIndicator::setCriticalFault(const bool active) noexcept
{
	criticalFault_ = active;
}

void StatusIndicator::setCommissioning(const bool active) noexcept
{
	commissioning_ = active;
}

void StatusIndicator::setBusDegraded(const bool active) noexcept
{
	busDegraded_ = active;
}

void StatusIndicator::notifyCommand(const CommandFeedback feedback, const std::uint32_t nowMs) noexcept
{
	commandFeedback_ = feedback;
	commandFeedbackStartedAtMs_ = nowMs;
	commandFeedbackPending_ = true;
}

IndicatorResult StatusIndicator::setMaintenanceColor(const std::uint8_t red,
																		  const std::uint8_t green,
																		  const std::uint8_t blue,
																		  const std::uint8_t brightness,
																		  const std::uint8_t maximumBrightness,
																		  const std::uint32_t nowMs) noexcept
{
	if (!initialized_)
	{
		return IndicatorResult::NotInitialized;
	}
	maintenance_.red = red;
	maintenance_.green = green;
	maintenance_.blue = blue;
	maintenance_.brightness = brightness > maximumBrightness ? maximumBrightness : brightness;
	maintenance_.active = true;
	maintenanceStartedAtMs_ = nowMs;
	return IndicatorResult::Applied;
}

IndicatorResult StatusIndicator::playMaintenanceTone(const std::uint8_t tone,
																		 const std::uint8_t maximumDutyPercent,
																		 const std::uint32_t nowMs) noexcept
{
	if (!initialized_)
	{
		return IndicatorResult::NotInitialized;
	}
	if (tone >= maintenanceToneFrequencies.size())
	{
		return IndicatorResult::InvalidValue;
	}
	maintenance_.tone = tone;
	maintenance_.buzzerDutyPercent = maximumDutyPercent > 100 ? 100 : maximumDutyPercent;
	maintenance_.active = true;
	maintenanceStartedAtMs_ = nowMs;
	maintenanceToneStartedAtMs_ = nowMs;
	return IndicatorResult::Applied;
}

void StatusIndicator::clearMaintenanceOverride() noexcept
{
	maintenance_ = {};
}

IndicatorResult StatusIndicator::update(const std::uint32_t nowMs) noexcept
{
	if (!initialized_)
	{
		return IndicatorResult::NotInitialized;
	}

	if (commandFeedbackPending_ && nowMs - commandFeedbackStartedAtMs_ >= commandFeedbackDurationMs)
	{
		commandFeedbackPending_ = false;
	}
	if (maintenance_.active && nowMs - maintenanceStartedAtMs_ >= maintenanceDurationMs)
	{
		maintenance_ = {};
	}

	return apply(outputFor(activeMode(nowMs), nowMs));
}

IndicatorMode StatusIndicator::activeMode(const std::uint32_t nowMs) const noexcept
{
	if (criticalFault_)
	{
		return IndicatorMode::CriticalFault;
	}
	if (maintenance_.active && nowMs - maintenanceStartedAtMs_ < maintenanceDurationMs)
	{
		return IndicatorMode::Maintenance;
	}
	if (commissioning_)
	{
		return IndicatorMode::Commissioning;
	}
	if (busDegraded_)
	{
		return IndicatorMode::DegradedBus;
	}
	if (commandFeedbackPending_ && nowMs - commandFeedbackStartedAtMs_ < commandFeedbackDurationMs)
	{
		return IndicatorMode::CommandFeedback;
	}

	return IndicatorMode::Normal;
}

bool StatusIndicator::isInitialized() const noexcept
{
	return initialized_;
}

MaintenanceIndicatorState StatusIndicator::maintenanceState(const std::uint32_t nowMs) const noexcept
{
	auto state = maintenance_;
	state.active = maintenance_.active && nowMs - maintenanceStartedAtMs_ < maintenanceDurationMs;
	if (!state.active || nowMs - maintenanceToneStartedAtMs_ >= maintenanceToneDurationMs)
	{
		state.tone = 0;
	}
	return state;
}

StatusIndicator::OutputState StatusIndicator::outputFor(const IndicatorMode mode, const std::uint32_t nowMs) const noexcept
{
	switch (mode)
	{
	case IndicatorMode::Maintenance:
	{
		const auto brightness = static_cast<std::uint16_t>(maintenance_.brightness);
		OutputState output{};
		output.red = static_cast<std::uint8_t>((static_cast<std::uint16_t>(maintenance_.red) * brightness) / 255U);
		output.green = static_cast<std::uint8_t>((static_cast<std::uint16_t>(maintenance_.green) * brightness) / 255U);
		output.blue = static_cast<std::uint8_t>((static_cast<std::uint16_t>(maintenance_.blue) * brightness) / 255U);
		output.toneHz = nowMs - maintenanceToneStartedAtMs_ < maintenanceToneDurationMs
							? maintenanceToneFrequencies[maintenance_.tone]
							: std::uint16_t{0};
		output.buzzerDutyPercent = maintenance_.buzzerDutyPercent;
		return output;
	}
	case IndicatorMode::CriticalFault:
	{
		const auto phaseMs = nowMs % criticalAlertPeriodMs;
		if (phaseMs < maximumBuzzerOnDurationMs)
		{
			return criticalFaultOutput;
		}
		return phaseMs < criticalAlertPeriodMs / 2 ? OutputState{criticalFaultOutput.red, 0, 0, 0, 0} : offOutput;
	}
	case IndicatorMode::Commissioning:
		return nowMs % commissioningPeriodMs < commissioningPeriodMs / 2 ? commissioningOutput : offOutput;
	case IndicatorMode::DegradedBus:
		return nowMs % degradedBusPeriodMs < degradedBusPeriodMs / 4 ? degradedBusOutput : offOutput;
	case IndicatorMode::CommandFeedback:
	{
		auto output = commandFeedback_ == CommandFeedback::Accepted ? commandAcceptedOutput : commandRejectedOutput;
		if (nowMs - commandFeedbackStartedAtMs_ >= commandToneDurationMs)
		{
			output.toneHz = 0;
		}
		return output;
	}
	case IndicatorMode::Normal:
	default:
		return normalOutput;
	}
}

IndicatorResult StatusIndicator::apply(const OutputState &output) noexcept
{
	if (output.red != appliedOutput_.red || output.green != appliedOutput_.green || output.blue != appliedOutput_.blue)
	{
		if (!indicator_.write(output.red, output.green, output.blue))
		{
			return IndicatorResult::HardwareFailure;
		}
	}

	if (output.toneHz != appliedOutput_.toneHz || output.buzzerDutyPercent != appliedOutput_.buzzerDutyPercent)
	{
		if (!buzzer_.write(output.toneHz, output.buzzerDutyPercent))
		{
			return IndicatorResult::HardwareFailure;
		}
	}

	appliedOutput_ = output;
	return IndicatorResult::Applied;
}

void StatusIndicator::silenceAndTurnOff() noexcept
{
	static_cast<void>(buzzer_.write(0, 0));
	static_cast<void>(indicator_.write(0, 0, 0));
	appliedOutput_ = offOutput;
}
}