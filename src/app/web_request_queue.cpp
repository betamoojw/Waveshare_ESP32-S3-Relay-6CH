#include "web_request_queue.h"

#include <algorithm>

namespace switch_actuator::app
{
bool WebRequestQueue::enqueue(const WebApplicationRequest &request) noexcept
{
	if (request.operationId == 0 || request.sessionId == 0 || size_ >= capacity ||
		std::any_of(results_.begin(), results_.end(), [&request](const auto &result) {
			return result.active && result.operationId == request.operationId;
		})) return false;
	const auto result = std::find_if(results_.begin(), results_.end(), [](const auto &candidate) {
		return !candidate.active;
	});
	if (result == results_.end()) return false;
	*result = {true, request.operationId, request.sessionId, WebOperationStatus::Pending, request.receivedAtMs, 0};
	requests_[tail_] = request;
	tail_ = (tail_ + 1U) % capacity;
	++size_;
	highWaterMark_ = std::max(highWaterMark_, size_);
	return true;
}

bool WebRequestQueue::dequeue(WebApplicationRequest &request) noexcept
{
	if (size_ == 0) return false;
	request = requests_[head_];
	requests_[head_] = {};
	head_ = (head_ + 1U) % capacity;
	--size_;
	return true;
}

bool WebRequestQueue::complete(const std::uint32_t operationId,
	const WebOperationStatus status,
	const std::uint32_t nowMs) noexcept
{
	const auto result = std::find_if(results_.begin(), results_.end(), [operationId](const auto &candidate) {
		return candidate.active && candidate.operationId == operationId;
	});
	if (result == results_.end()) return false;
	result->status = status;
	result->completedAtMs = nowMs;
	return true;
}

bool WebRequestQueue::findResult(const std::uint32_t sessionId,
	const std::uint32_t operationId,
	WebOperationResult &result) const noexcept
{
	const auto match = std::find_if(results_.begin(), results_.end(), [sessionId, operationId](const auto &candidate) {
		return candidate.active && candidate.sessionId == sessionId && candidate.operationId == operationId;
	});
	if (match == results_.end()) return false;
	result = *match;
	return true;
}

void WebRequestQueue::expire(const std::uint32_t nowMs) noexcept
{
	for (auto &result : results_)
	{
		if (result.active && result.completedAtMs != 0 && nowMs - result.completedAtMs >= resultRetentionMs) result = {};
	}
}

void WebRequestQueue::clear() noexcept
{
	requests_.fill({});
	results_.fill({});
	head_ = 0;
	tail_ = 0;
	size_ = 0;
	highWaterMark_ = 0;
}

std::size_t WebRequestQueue::size() const noexcept { return size_; }
std::size_t WebRequestQueue::highWaterMark() const noexcept { return highWaterMark_; }
}