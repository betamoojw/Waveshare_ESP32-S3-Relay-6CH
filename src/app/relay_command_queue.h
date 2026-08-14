#pragma once

#include "relay_command_service.h"
#include "../domain/relay_types.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace switch_actuator::app
{
struct RelayCommandBatch final
{
	std::array<domain::RelayCommand, domain::relayChannelCount> commands{};
	std::size_t count{0};
};

enum class RelayCommandEnqueueResult : std::uint8_t
{
	Accepted,
	EmptyBatch,
	TooManyCommands,
	InvalidSafetyBatch,
	QueueFull
};

class RelayCommandQueue final
{
public:
	static constexpr std::size_t capacity{16};
	static constexpr std::size_t safetyReserve{2};

	[[nodiscard]] RelayCommandEnqueueResult enqueue(const RelayCommandBatch &batch) noexcept;
	[[nodiscard]] bool dequeue(RelayCommandBatch &batch) noexcept;
	[[nodiscard]] bool processNext(RelayCommandService &service, RelayCommandBatchResult &result) noexcept;
	[[nodiscard]] std::size_t size() const noexcept;
	[[nodiscard]] bool empty() const noexcept;
	[[nodiscard]] bool full() const noexcept;
	void clear() noexcept;

private:
	[[nodiscard]] static bool containsSafetyCommand(const RelayCommandBatch &batch) noexcept;
	[[nodiscard]] static bool isSafetyOffBatch(const RelayCommandBatch &batch) noexcept;

	std::array<RelayCommandBatch, capacity> batches_{};
	std::size_t head_{0};
	std::size_t tail_{0};
	std::size_t size_{0};
};
}