#pragma once

#include "../../hal/WatchdogHal.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace switch_actuator::adapters::watchdog
{
using WatchdogInitializeResult = hal::WatchdogInitializeResult;
using WatchdogFeedResult = hal::WatchdogFeedResult;

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
	[[nodiscard]] hal::IWatchdog hal() noexcept;

private:
	[[nodiscard]] static WatchdogInitializeResult initializeHandler(void *context) noexcept;
	[[nodiscard]] static WatchdogFeedResult feedHandler(void *context) noexcept;
	[[nodiscard]] static bool initializedHandler(void *context) noexcept;
	[[nodiscard]] static bool healthyHandler(void *context) noexcept;

	bool initialized_{false};
	bool ownsRegistration_{false};
	bool fedSinceInitialization_{false};
	TaskHandle_t task_{nullptr};
};
}