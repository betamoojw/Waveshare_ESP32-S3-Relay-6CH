#pragma once

#include <cstdint>

namespace switch_actuator::hal
{
enum class WatchdogInitializeResult : std::uint8_t
{
	Initialized,
	AlreadyInitialized,
	RegistrationFailure
};

enum class WatchdogFeedResult : std::uint8_t
{
	Fed,
	NotInitialized,
	FeedFailure
};

using WatchdogInitializeHandler = WatchdogInitializeResult (*)(void *context) noexcept;
using WatchdogFeedHandler = WatchdogFeedResult (*)(void *context) noexcept;
using WatchdogStateHandler = bool (*)(void *context) noexcept;

class IWatchdog final
{
public:
	constexpr IWatchdog() noexcept = default;

	constexpr IWatchdog(const WatchdogInitializeHandler initialize,
		const WatchdogFeedHandler feed,
		const WatchdogStateHandler initialized,
		const WatchdogStateHandler healthy,
		void *const context = nullptr) noexcept
		: initialize_{initialize}, feed_{feed}, initialized_{initialized}, healthy_{healthy}, context_{context}
	{
	}

	[[nodiscard]] WatchdogInitializeResult initialize() const noexcept
	{
		return initialize_ != nullptr ? initialize_(context_) : WatchdogInitializeResult::RegistrationFailure;
	}

	[[nodiscard]] WatchdogFeedResult feed() const noexcept
	{
		return feed_ != nullptr ? feed_(context_) : WatchdogFeedResult::NotInitialized;
	}

	[[nodiscard]] bool isInitialized() const noexcept
	{
		return initialized_ != nullptr && initialized_(context_);
	}

	[[nodiscard]] bool isHealthy() const noexcept
	{
		return healthy_ != nullptr && healthy_(context_);
	}

	[[nodiscard]] constexpr bool isValid() const noexcept
	{
		return initialize_ != nullptr && feed_ != nullptr && initialized_ != nullptr && healthy_ != nullptr;
	}

private:
	WatchdogInitializeHandler initialize_{nullptr};
	WatchdogFeedHandler feed_{nullptr};
	WatchdogStateHandler initialized_{nullptr};
	WatchdogStateHandler healthy_{nullptr};
	void *context_{nullptr};
};
}