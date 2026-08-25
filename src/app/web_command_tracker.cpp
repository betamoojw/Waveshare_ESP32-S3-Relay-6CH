#include "web_command_tracker.h"

#include <algorithm>

namespace switch_actuator::app
{
WebCommandBeginResult WebCommandTracker::begin(const std::uint32_t sessionId,
	const std::string_view idempotencyKey,
	const domain::RelayChannelId channel,
	const domain::RelayAction action,
	const std::uint32_t expectedResourceSequence,
	const std::uint32_t correlationId,
	const std::uint32_t nowMs,
	WebTrackedCommand &command) noexcept
{
	const std::lock_guard<std::mutex> lock{mutex_};
	command = {};
	if (sessionId == 0 || correlationId == 0 || idempotencyKey.empty() || idempotencyKey.size() > 128U)
		return WebCommandBeginResult::InvalidKey;
	expireUnlocked(nowMs);
	const auto keyHash = hash(idempotencyKey);
	const auto normalizedRequestHash = requestHash(channel, action, expectedResourceSequence);
	const auto existing = std::find_if(commands_.begin(), commands_.end(), [sessionId, keyHash](const auto &candidate) {
		return candidate.active && candidate.sessionId == sessionId && candidate.keyHash == keyHash;
	});
	if (existing != commands_.end())
	{
		command = *existing;
		return existing->requestHash == normalizedRequestHash ? WebCommandBeginResult::Duplicate :
			WebCommandBeginResult::IdempotencyMismatch;
	}
	const auto available = std::find_if(commands_.begin(), commands_.end(), [](const auto &candidate) {
		return !candidate.active;
	});
	if (available == commands_.end()) return WebCommandBeginResult::CapacityFull;
	*available = {true, sessionId, keyHash, normalizedRequestHash, correlationId, channel, action,
		WebTrackedCommandStatus::Queued, RelayCommandReason::None, domain::RelayState::Off,
		expectedResourceSequence, 0, nowMs, 0};
	command = *available;
	return WebCommandBeginResult::Accepted;
}

bool WebCommandTracker::complete(const std::uint32_t correlationId,
	const RelayCommandStatus status,
	const RelayCommandReason reason,
	const domain::RelaySnapshot &snapshot,
	const std::uint32_t nowMs,
	WebTrackedCommand &command) noexcept
{
	const std::lock_guard<std::mutex> lock{mutex_};
	const auto tracked = std::find_if(commands_.begin(), commands_.end(), [correlationId](const auto &candidate) {
		return candidate.active && candidate.correlationId == correlationId;
	});
	if (tracked == commands_.end()) return false;
	tracked->status = status == RelayCommandStatus::Rejected ? WebTrackedCommandStatus::Rejected :
		tracked->expectedResourceSequence == snapshot.transitionSequence ? WebTrackedCommandStatus::Idempotent :
		WebTrackedCommandStatus::Applied;
	tracked->reason = reason;
	tracked->appliedState = snapshot.appliedState;
	tracked->resourceSequence = snapshot.transitionSequence;
	tracked->completedAtMs = nowMs;
	command = *tracked;
	return true;
}

bool WebCommandTracker::reject(const std::uint32_t correlationId,
	const RelayCommandReason reason,
	const std::uint32_t nowMs,
	WebTrackedCommand &command) noexcept
{
	const std::lock_guard<std::mutex> lock{mutex_};
	const auto tracked = std::find_if(commands_.begin(), commands_.end(), [correlationId](const auto &candidate) {
		return candidate.active && candidate.correlationId == correlationId;
	});
	if (tracked == commands_.end()) return false;
	tracked->status = WebTrackedCommandStatus::Rejected;
	tracked->reason = reason;
	tracked->completedAtMs = nowMs;
	command = *tracked;
	return true;
}

bool WebCommandTracker::findByCorrelation(const std::uint32_t sessionId,
	const std::uint32_t correlationId,
	WebTrackedCommand &command) const noexcept
{
	const std::lock_guard<std::mutex> lock{mutex_};
	const auto tracked = std::find_if(commands_.begin(), commands_.end(), [sessionId, correlationId](const auto &candidate) {
		return candidate.active && candidate.sessionId == sessionId && candidate.correlationId == correlationId;
	});
	if (tracked == commands_.end()) return false;
	command = *tracked;
	return true;
}

void WebCommandTracker::expire(const std::uint32_t nowMs) noexcept
{
	const std::lock_guard<std::mutex> lock{mutex_};
	expireUnlocked(nowMs);
}

void WebCommandTracker::expireUnlocked(const std::uint32_t nowMs) noexcept
{
	for (auto &command : commands_)
	{
		if (command.active && command.completedAtMs != 0 && nowMs - command.completedAtMs >= retentionMs) command = {};
	}
}

void WebCommandTracker::clear() noexcept
{
	const std::lock_guard<std::mutex> lock{mutex_};
	commands_.fill({});
}

std::uint32_t WebCommandTracker::hash(const std::string_view value) noexcept
{
	std::uint32_t result{2166136261U};
	for (const auto character : value) result = (result ^ static_cast<std::uint8_t>(character)) * 16777619U;
	return result;
}

std::uint32_t WebCommandTracker::requestHash(const domain::RelayChannelId channel,
	const domain::RelayAction action,
	const std::uint32_t expectedResourceSequence) noexcept
{
	return ((static_cast<std::uint32_t>(channel.value) << 8U) | static_cast<std::uint32_t>(action)) ^
		(expectedResourceSequence * 16777619U);
}
}