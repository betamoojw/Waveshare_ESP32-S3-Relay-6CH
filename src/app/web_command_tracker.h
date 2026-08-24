#pragma once

#include "relay_command_service.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace switch_actuator::app
{
enum class WebTrackedCommandStatus : std::uint8_t
{
	Queued,
	Applied,
	Idempotent,
	Rejected
};

struct WebTrackedCommand final
{
	bool active{false};
	std::uint32_t sessionId{0};
	std::uint32_t keyHash{0};
	std::uint32_t requestHash{0};
	std::uint32_t correlationId{0};
	domain::RelayChannelId channel{0};
	domain::RelayAction action{domain::RelayAction::SetOff};
	WebTrackedCommandStatus status{WebTrackedCommandStatus::Queued};
	RelayCommandReason reason{RelayCommandReason::None};
	domain::RelayState appliedState{domain::RelayState::Off};
	std::uint32_t expectedResourceSequence{0};
	std::uint32_t resourceSequence{0};
	std::uint32_t createdAtMs{0};
	std::uint32_t completedAtMs{0};
};

enum class WebCommandBeginResult : std::uint8_t
{
	Accepted,
	Duplicate,
	IdempotencyMismatch,
	CapacityFull,
	InvalidKey
};

class WebCommandTracker final
{
public:
	static constexpr std::size_t capacity{32};
	static constexpr std::uint32_t retentionMs{60'000};

	[[nodiscard]] WebCommandBeginResult begin(std::uint32_t sessionId,
		std::string_view idempotencyKey,
		domain::RelayChannelId channel,
		domain::RelayAction action,
		std::uint32_t expectedResourceSequence,
		std::uint32_t correlationId,
		std::uint32_t nowMs,
		WebTrackedCommand &command) noexcept;
	[[nodiscard]] bool complete(std::uint32_t correlationId,
		RelayCommandStatus status,
		RelayCommandReason reason,
		const domain::RelaySnapshot &snapshot,
		std::uint32_t nowMs,
		WebTrackedCommand &command) noexcept;
	[[nodiscard]] bool reject(std::uint32_t correlationId,
		RelayCommandReason reason,
		std::uint32_t nowMs,
		WebTrackedCommand &command) noexcept;
	[[nodiscard]] bool findByCorrelation(std::uint32_t sessionId,
		std::uint32_t correlationId,
		WebTrackedCommand &command) const noexcept;
	void expire(std::uint32_t nowMs) noexcept;
	void clear() noexcept;

private:
	[[nodiscard]] static std::uint32_t hash(std::string_view value) noexcept;
	[[nodiscard]] static std::uint32_t requestHash(domain::RelayChannelId channel,
		domain::RelayAction action,
		std::uint32_t expectedResourceSequence) noexcept;

	std::array<WebTrackedCommand, capacity> commands_{};
};
}