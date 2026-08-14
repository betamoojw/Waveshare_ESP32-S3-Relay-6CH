#include "esp32_task_watchdog.h"

#include <Arduino.h>
#include <esp_task_wdt.h>

namespace switch_actuator::adapters::watchdog
{
Esp32TaskWatchdog::~Esp32TaskWatchdog()
{
	if (initialized_ && ownsRegistration_)
	{
		static_cast<void>(esp_task_wdt_delete(task_));
	}
}

WatchdogInitializeResult Esp32TaskWatchdog::initialize() noexcept
{
	if (initialized_)
	{
		return WatchdogInitializeResult::AlreadyInitialized;
	}

	task_ = xTaskGetCurrentTaskHandle();
	if (task_ == nullptr)
	{
		return WatchdogInitializeResult::RegistrationFailure;
	}
	if (esp_task_wdt_status(task_) == ESP_OK)
	{
		initialized_ = true;
		ownsRegistration_ = false;
		fedSinceInitialization_ = false;
		return WatchdogInitializeResult::AlreadyInitialized;
	}

	enableLoopWDT();
	if (esp_task_wdt_status(task_) != ESP_OK)
	{
		task_ = nullptr;
		return WatchdogInitializeResult::RegistrationFailure;
	}
	initialized_ = true;
	ownsRegistration_ = true;
	fedSinceInitialization_ = false;
	return WatchdogInitializeResult::Initialized;
}

WatchdogFeedResult Esp32TaskWatchdog::feed() noexcept
{
	if (!initialized_)
	{
		return WatchdogFeedResult::NotInitialized;
	}

	if (esp_task_wdt_reset() != ESP_OK)
	{
		fedSinceInitialization_ = false;
		return WatchdogFeedResult::FeedFailure;
	}
	fedSinceInitialization_ = true;
	return WatchdogFeedResult::Fed;
}

bool Esp32TaskWatchdog::isInitialized() const noexcept
{
	return initialized_;
}

bool Esp32TaskWatchdog::isHealthy() const noexcept
{
	return initialized_ && task_ != nullptr && fedSinceInitialization_ && esp_task_wdt_status(task_) == ESP_OK;
}
}