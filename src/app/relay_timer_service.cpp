#include "relay_timer_service.h"

#include <limits>

namespace switch_actuator::app
{
RelayTimerService::RelayTimerService(SwitchingPolicyService &switchingPolicy) noexcept
	: switchingPolicy_{switchingPolicy}
{
}

RelayTimerScheduleResult RelayTimerService::schedule(const domain::RelayChannelId channel,
																									 const domain::RelayAction action,
																									 const domain::CommandSource source,
																									 const std::uint32_t correlationId,
																									 const std::uint32_t nowMs,
																									 const std::uint32_t delayMs) noexcept
{
	if (channel.value >= pending_.size())
	{
		return RelayTimerScheduleResult::InvalidChannel;
	}
	if (delayMs > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
	{
		return RelayTimerScheduleResult::InvalidDelay;
	}
	auto &pending = pending_[channel.value];
	const auto result = pending.active ? RelayTimerScheduleResult::Replaced : RelayTimerScheduleResult::Scheduled;
	pending = {{channel, action, source, correlationId, nowMs + delayMs}, nowMs + delayMs, true};
	return result;
}

bool RelayTimerService::cancel(const domain::RelayChannelId channel) noexcept
{
	if (channel.value >= pending_.size() || !pending_[channel.value].active)
	{
		return false;
	}
	pending_[channel.value] = {};
	return true;
}

RelayTimerUpdateResult RelayTimerService::update(const std::uint32_t nowMs) noexcept
{
	RelayTimerUpdateResult result{};
	for (auto &pending : pending_)
	{
		if (!pending.active || !isDue(nowMs, pending.dueAtMs))
		{
			continue;
		}
		const auto policyResult = switchingPolicy_.requestChannel(pending.command.channel,
			pending.command.action,
			pending.command.source,
			pending.command.correlationId,
			nowMs);
		if (policyResult == SwitchingPolicyResult::QueueFull)
		{
			continue;
		}
		pending = {};
		if (policyResult == SwitchingPolicyResult::Accepted)
		{
			++result.submitted;
		}
		else
		{
			++result.rejected;
		}
	}
	result.pending = pendingCount();
	return result;
}

std::size_t RelayTimerService::pendingCount() const noexcept
{
	std::size_t count{0};
	for (const auto &pending : pending_)
	{
		count += pending.active ? 1U : 0U;
	}
	return count;
}

void RelayTimerService::cancelAll() noexcept
{
	pending_.fill({});
}

bool RelayTimerService::isDue(const std::uint32_t nowMs, const std::uint32_t dueAtMs) noexcept
{
	return nowMs - dueAtMs <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
}
}