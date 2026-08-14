#pragma once

#include "command_arbiter.h"
#include "../domain/relay_types.h"
#include "../ports/event_sink.h"
#include "../ports/relay_output_port.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace switch_actuator::app
{
enum class RelayServiceInitializeResult : std::uint8_t
{
	Initialized,
	InvalidPort,
	OutputFailure
};

enum class RelayCommandStatus : std::uint8_t
{
	Accepted,
	Rejected
};

enum class RelayCommandReason : std::uint8_t
{
	None,
	NotInitialized,
	EmptyBatch,
	InvalidChannel,
	InvalidAction,
	InvalidSource,
	DuplicateChannel,
	SafetyLockout,
	OutputFailure,
	EventRejected
};

struct RelayCommandResult final
{
	RelayCommandStatus status;
	RelayCommandReason reason;
	std::uint32_t correlationId;
	domain::RelayState finalState;
};

struct RelayCommandBatchResult final
{
	RelayCommandStatus status;
	RelayCommandReason reason;
	std::size_t commandCount;
	std::size_t changedCount;
};

class RelayCommandService final
{
public:
	using RelayEventSink = ports::EventSink<domain::RelayStateChanged>;

	RelayCommandService(ports::RelayOutputPort outputPort, RelayEventSink eventSink) noexcept;

	[[nodiscard]] RelayServiceInitializeResult initialize(std::uint32_t nowMs) noexcept;
	[[nodiscard]] RelayCommandResult execute(const domain::RelayCommand &command) noexcept;
	[[nodiscard]] RelayCommandBatchResult executeBatch(const domain::RelayCommand *commands, std::size_t commandCount) noexcept;
	[[nodiscard]] RelayCommandResult setSafetyLockout(domain::RelayChannelId channel,
														 bool active,
														 std::uint32_t correlationId,
														 std::uint32_t nowMs) noexcept;
	[[nodiscard]] const domain::RelaySnapshot *snapshot(domain::RelayChannelId channel) const noexcept;
	[[nodiscard]] const std::array<domain::RelaySnapshot, domain::relayChannelCount> &snapshots() const noexcept;
	[[nodiscard]] bool isInitialized() const noexcept;

private:
	[[nodiscard]] static bool isValid(domain::RelayAction action) noexcept;
	[[nodiscard]] static bool isValid(domain::CommandSource source) noexcept;
	[[nodiscard]] RelayCommandReason validate(const domain::RelayCommand &command) const noexcept;
	[[nodiscard]] static domain::RelayState requestedState(domain::RelayState current, domain::RelayAction action) noexcept;
	static void incrementSequence(std::uint32_t &sequence) noexcept;
	[[nodiscard]] RelayCommandResult reject(const domain::RelayCommand &command, RelayCommandReason reason) const noexcept;
	[[nodiscard]] RelayCommandReason publishStateChanged(const domain::RelayCommand &command,
															   const domain::RelaySnapshot &snapshot) const noexcept;
	[[nodiscard]] bool forceSafeOff(domain::RelayChannelId channel,
								  domain::RelaySnapshot &snapshot,
								  std::uint32_t correlationId,
								  std::uint32_t nowMs) noexcept;

	ports::RelayOutputPort outputPort_;
	RelayEventSink eventSink_;
	CommandArbiter commandArbiter_;
	std::array<domain::RelaySnapshot, domain::relayChannelCount> snapshots_{};
	bool initialized_{false};
};
}