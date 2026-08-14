#pragma once

#include "command_arbiter.h"
#include "relay_command_queue.h"
#include "../domain/relay_types.h"

#include <array>
#include <cstdint>

namespace switch_actuator::app
{
enum class SwitchingPolicyResult : std::uint8_t
{
	Accepted,
	NoParticipants,
	InvalidChannel,
	InvalidAction,
	InvalidSource,
	SafetyLockout,
	QueueFull
};

class SwitchingPolicyService final
{
public:
	SwitchingPolicyService(RelayCommandQueue &commandQueue, const CommandArbiter &commandArbiter) noexcept;

	[[nodiscard]] SwitchingPolicyResult requestChannel(domain::RelayChannelId channel,
													 domain::RelayAction action,
													 domain::CommandSource source,
													 std::uint32_t correlationId,
													 std::uint32_t receivedAtMs) noexcept;
	[[nodiscard]] SwitchingPolicyResult requestGroup(
		const std::array<bool, domain::relayChannelCount> &participants,
		domain::RelayAction action,
		domain::CommandSource source,
		std::uint32_t correlationId,
		std::uint32_t receivedAtMs) noexcept;
	[[nodiscard]] SwitchingPolicyResult requestStates(
		const std::array<bool, domain::relayChannelCount> &participants,
		const std::array<domain::RelayState, domain::relayChannelCount> &targetStates,
		domain::CommandSource source,
		std::uint32_t correlationId,
		std::uint32_t receivedAtMs) noexcept;
	[[nodiscard]] SwitchingPolicyResult requestBatch(const RelayCommandBatch &batch) noexcept;

private:
	[[nodiscard]] static bool isValid(domain::RelayAction action) noexcept;
	[[nodiscard]] static bool isValid(domain::CommandSource source) noexcept;
	[[nodiscard]] SwitchingPolicyResult validate(const domain::RelayCommand &command) const noexcept;
	[[nodiscard]] SwitchingPolicyResult enqueue(const RelayCommandBatch &batch) noexcept;

	RelayCommandQueue &commandQueue_;
	const CommandArbiter &commandArbiter_;
};
}