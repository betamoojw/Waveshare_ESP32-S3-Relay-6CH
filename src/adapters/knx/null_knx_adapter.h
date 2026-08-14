#pragma once

#include "knx_adapter.h"
#include "../../app/diagnostics_service.h"

namespace switch_actuator::adapters::knx
{
class NullKnxAdapter final
{
public:
	explicit NullKnxAdapter(app::DiagnosticsService *diagnostics) noexcept;

	[[nodiscard]] KnxInitializeResult initialize(bool enabledByConfiguration, std::uint32_t nowMs) noexcept;
	[[nodiscard]] KnxPollResult poll() const noexcept;
	[[nodiscard]] bool isInitialized() const noexcept;
	[[nodiscard]] constexpr bool isAvailable() const noexcept
	{
		return false;
	}
	[[nodiscard]] constexpr bool isBusOnline() const noexcept
	{
		return false;
	}

private:
	app::DiagnosticsService *diagnostics_;
	bool requested_{false};
	bool initialized_{false};
};
}