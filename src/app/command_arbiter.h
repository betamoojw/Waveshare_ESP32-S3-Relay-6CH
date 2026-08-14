#pragma once

#include "../domain/relay_types.h"

#include <array>
#include <cstdint>

namespace switch_actuator::app
{
enum class ArbitrationDecision : std::uint8_t
{
	Allowed,
	InvalidChannel,
	SafetyLockout
};

class CommandArbiter final
{
public:
	[[nodiscard]] ArbitrationDecision evaluate(const domain::RelayCommand &command) const noexcept;
	[[nodiscard]] bool setSafetyLockout(domain::RelayChannelId channel, bool active) noexcept;
	[[nodiscard]] bool isSafetyLocked(domain::RelayChannelId channel) const noexcept;
	void reset() noexcept;

private:
	std::array<bool, domain::relayChannelCount> safetyLockouts_{};
};
}