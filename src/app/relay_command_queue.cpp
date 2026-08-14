#include "relay_command_queue.h"

namespace switch_actuator::app
{
RelayCommandEnqueueResult RelayCommandQueue::enqueue(const RelayCommandBatch &batch) noexcept
{
	if (batch.count == 0)
	{
		return RelayCommandEnqueueResult::EmptyBatch;
	}
	if (batch.count > batch.commands.size())
	{
		return RelayCommandEnqueueResult::TooManyCommands;
	}

	const auto safetyOff = isSafetyOffBatch(batch);
	if (containsSafetyCommand(batch) && !safetyOff)
	{
		return RelayCommandEnqueueResult::InvalidSafetyBatch;
	}
	const auto usableCapacity = safetyOff ? capacity : capacity - safetyReserve;
	if (size_ >= usableCapacity)
	{
		return RelayCommandEnqueueResult::QueueFull;
	}

	batches_[tail_] = batch;
	tail_ = (tail_ + 1U) % capacity;
	++size_;
	return RelayCommandEnqueueResult::Accepted;
}

bool RelayCommandQueue::dequeue(RelayCommandBatch &batch) noexcept
{
	if (empty())
	{
		return false;
	}

	batch = batches_[head_];
	head_ = (head_ + 1U) % capacity;
	--size_;
	return true;
}

bool RelayCommandQueue::processNext(RelayCommandService &service, RelayCommandBatchResult &result) noexcept
{
	RelayCommandBatch batch{};
	if (!dequeue(batch))
	{
		return false;
	}

	result = service.executeBatch(batch.commands.data(), batch.count);
	return true;
}

std::size_t RelayCommandQueue::size() const noexcept
{
	return size_;
}

bool RelayCommandQueue::empty() const noexcept
{
	return size_ == 0;
}

bool RelayCommandQueue::full() const noexcept
{
	return size_ == capacity;
}

void RelayCommandQueue::clear() noexcept
{
	head_ = 0;
	tail_ = 0;
	size_ = 0;
}

bool RelayCommandQueue::containsSafetyCommand(const RelayCommandBatch &batch) noexcept
{
	for (std::size_t index = 0; index < batch.count; ++index)
	{
		if (batch.commands[index].source == domain::CommandSource::Safety)
		{
			return true;
		}
	}
	return false;
}

bool RelayCommandQueue::isSafetyOffBatch(const RelayCommandBatch &batch) noexcept
{
	for (std::size_t index = 0; index < batch.count; ++index)
	{
		if (batch.commands[index].source != domain::CommandSource::Safety ||
			batch.commands[index].action != domain::RelayAction::SetOff)
		{
			return false;
		}
	}
	return true;
}
}