#include "web_event_journal.h"

#include <limits>

namespace switch_actuator::app
{
void WebEventJournal::publish(WebEvent event) noexcept
{
	latestSequence_ = nextSequence(latestSequence_);
	event.sequence = latestSequence_;
	events_[(latestSequence_ - 1U) % capacity] = event;
	if (count_ < capacity)
	{
		++count_;
	}
}

WebEventReadResult WebEventJournal::read(const std::uint32_t sequence, WebEvent &event) const noexcept
{
	if (sequence == 0 || sequence > latestSequence_)
	{
		return WebEventReadResult::NotYetAvailable;
	}
	if (sequence < oldestSequence())
	{
		return WebEventReadResult::Gap;
	}
	const auto &candidate = events_[(sequence - 1U) % capacity];
	if (candidate.sequence != sequence)
	{
		return WebEventReadResult::Gap;
	}
	event = candidate;
	return WebEventReadResult::Available;
}

std::uint32_t WebEventJournal::latestSequence() const noexcept
{
	return latestSequence_;
}

std::uint32_t WebEventJournal::oldestSequence() const noexcept
{
	return count_ == 0 ? 0 : latestSequence_ - static_cast<std::uint32_t>(count_) + 1U;
}

void WebEventJournal::clear() noexcept
{
	events_.fill({});
	latestSequence_ = 0;
	count_ = 0;
}

std::uint32_t WebEventJournal::nextSequence(const std::uint32_t current) noexcept
{
	return current == std::numeric_limits<std::uint32_t>::max() ? 1U : current + 1U;
}
}