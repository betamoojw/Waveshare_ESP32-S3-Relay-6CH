#pragma once

#include "switching_policy_service.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace switch_actuator::app
{
enum class RelayTimerScheduleResult : std::uint8_t
{
	Scheduled,
	Replaced,
	InvalidChannel,
	InvalidDelay
};

struct RelayTimerUpdateResult final
{
	std::size_t submitted{0};
	std::size_t rejected{0};
	std::size_t pending{0};
};

class RelayTimerService final
{
public:
	explicit RelayTimerService(SwitchingPolicyService &switchingPolicy) noexcept;

	[[nodiscard]] RelayTimerScheduleResult schedule(domain::RelayChannelId channel,
																 domain::RelayAction action,
																 domain::CommandSource source,
																 std::uint32_t correlationId,
																 std::uint32_t nowMs,
																 std::uint32_t delayMs) noexcept;
	[[nodiscard]] bool cancel(domain::RelayChannelId channel) noexcept;
	[[nodiscard]] RelayTimerUpdateResult update(std::uint32_t nowMs) noexcept;
	[[nodiscard]] std::size_t pendingCount() const noexcept;
	void cancelAll() noexcept;

private:
	struct PendingCommand final
	{
		domain::RelayCommand command{};
		std::uint32_t dueAtMs{0};
		bool active{false};
	};

	[[nodiscard]] static bool isDue(std::uint32_t nowMs, std::uint32_t dueAtMs) noexcept;

	SwitchingPolicyService &switchingPolicy_;
	std::array<PendingCommand, domain::relayChannelCount> pending_{};
};
}