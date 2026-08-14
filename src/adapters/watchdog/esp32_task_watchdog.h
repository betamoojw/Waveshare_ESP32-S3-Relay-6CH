#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdint>

namespace switch_actuator::adapters::watchdog
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

class Esp32TaskWatchdog final
{
public:
	Esp32TaskWatchdog() noexcept = default;
	~Esp32TaskWatchdog();

	Esp32TaskWatchdog(const Esp32TaskWatchdog &) = delete;
	Esp32TaskWatchdog &operator=(const Esp32TaskWatchdog &) = delete;
	Esp32TaskWatchdog(Esp32TaskWatchdog &&) = delete;
	Esp32TaskWatchdog &operator=(Esp32TaskWatchdog &&) = delete;

	[[nodiscard]] WatchdogInitializeResult initialize() noexcept;
	[[nodiscard]] WatchdogFeedResult feed() noexcept;
	[[nodiscard]] bool isInitialized() const noexcept;
	[[nodiscard]] bool isHealthy() const noexcept;

private:
	bool initialized_{false};
	bool ownsRegistration_{false};
	bool fedSinceInitialization_{false};
	TaskHandle_t task_{nullptr};
};
}