#pragma once

#include "../../hal/BuzzerHal.h"
#include "../../hal/RgbLedHal.h"

#include <cstdint>

namespace switch_actuator::adapters::indicators
{
enum class IndicatorResult : std::uint8_t
{
	Applied,
	NotInitialized,
	InvalidValue,
	HardwareFailure
};

enum class CommandFeedback : std::uint8_t
{
	Accepted,
	Rejected
};

enum class IndicatorMode : std::uint8_t
{
	Normal,
	CommandFeedback,
	DegradedBus,
	Commissioning,
	CriticalFault,
	Maintenance
};

struct MaintenanceIndicatorState final
{
	std::uint8_t red{0};
	std::uint8_t green{0};
	std::uint8_t blue{0};
	std::uint8_t brightness{0};
	std::uint8_t tone{0};
	std::uint8_t buzzerDutyPercent{0};
	bool active{false};
};

class StatusIndicator final
{
public:
	StatusIndicator(hal::RgbLedHal rgbLedHal, hal::BuzzerHal buzzerHal) noexcept;
	~StatusIndicator();

	StatusIndicator(const StatusIndicator &) = delete;
	StatusIndicator &operator=(const StatusIndicator &) = delete;
	StatusIndicator(StatusIndicator &&) = delete;
	StatusIndicator &operator=(StatusIndicator &&) = delete;

	[[nodiscard]] IndicatorResult initialize() noexcept;
	void setCriticalFault(bool active) noexcept;
	void setCommissioning(bool active) noexcept;
	void setBusDegraded(bool active) noexcept;
	void notifyCommand(CommandFeedback feedback, std::uint32_t nowMs) noexcept;
	[[nodiscard]] IndicatorResult setMaintenanceColor(std::uint8_t red,
															std::uint8_t green,
															std::uint8_t blue,
															std::uint8_t brightness,
															std::uint8_t maximumBrightness,
															std::uint32_t nowMs) noexcept;
	[[nodiscard]] IndicatorResult playMaintenanceTone(std::uint8_t tone,
															 std::uint8_t maximumDutyPercent,
															 std::uint32_t nowMs) noexcept;
	void clearMaintenanceOverride() noexcept;
	[[nodiscard]] IndicatorResult update(std::uint32_t nowMs) noexcept;
	[[nodiscard]] IndicatorMode activeMode(std::uint32_t nowMs) const noexcept;
	[[nodiscard]] bool isInitialized() const noexcept;
	[[nodiscard]] MaintenanceIndicatorState maintenanceState(std::uint32_t nowMs) const noexcept;

private:
	struct OutputState final
	{
		std::uint8_t red;
		std::uint8_t green;
		std::uint8_t blue;
		std::uint16_t toneHz;
		std::uint8_t buzzerDutyPercent;
	};

	static constexpr OutputState offOutput{0, 0, 0, 0, 0};
	static constexpr OutputState normalOutput{0, 32, 0, 0, 0};
	static constexpr OutputState criticalFaultOutput{96, 0, 0, 2000, 10};
	static constexpr OutputState commissioningOutput{0, 0, 64, 0, 0};
	static constexpr OutputState degradedBusOutput{96, 32, 0, 0, 0};
	static constexpr OutputState commandAcceptedOutput{0, 96, 0, 2400, 10};
	static constexpr OutputState commandRejectedOutput{96, 0, 0, 1200, 10};

	[[nodiscard]] OutputState outputFor(IndicatorMode mode, std::uint32_t nowMs) const noexcept;
	[[nodiscard]] IndicatorResult apply(const OutputState &output) noexcept;
	void silenceAndTurnOff() noexcept;

	static constexpr std::uint32_t commandFeedbackDurationMs{300};
	static constexpr std::uint32_t maximumBuzzerOnDurationMs{100};
	static constexpr std::uint32_t criticalAlertPeriodMs{1000};
	static constexpr std::uint32_t maintenanceDurationMs{5000};
	static constexpr std::uint32_t maintenanceToneDurationMs{100};

	hal::RgbLedHal rgbLedHal_;
	hal::BuzzerHal buzzerHal_;
	OutputState appliedOutput_{};
	CommandFeedback commandFeedback_{CommandFeedback::Accepted};
	std::uint32_t commandFeedbackStartedAtMs_{0};
	MaintenanceIndicatorState maintenance_{};
	std::uint32_t maintenanceStartedAtMs_{0};
	std::uint32_t maintenanceToneStartedAtMs_{0};
	bool criticalFault_{false};
	bool commissioning_{false};
	bool busDegraded_{false};
	bool commandFeedbackPending_{false};
	bool initialized_{false};
};
}