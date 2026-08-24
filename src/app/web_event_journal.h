#pragma once

#include "relay_command_service.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace switch_actuator::app
{
enum class WebEventType : std::uint8_t
{
	RelayCommandCompleted,
	RelayStateChanged,
	NetworkChanged,
	WifiScanStarted,
	WifiScanCompleted,
	ConfigurationChanged,
	DiagnosticsChanged
};

struct WebEvent final
{
	std::uint32_t sequence{0};
	WebEventType type{WebEventType::DiagnosticsChanged};
	std::uint32_t correlationId{0};
	domain::RelayChannelId channel{0};
	domain::RelayState appliedState{domain::RelayState::Off};
	RelayCommandStatus commandStatus{RelayCommandStatus::Rejected};
	RelayCommandReason commandReason{RelayCommandReason::None};
	std::uint32_t resourceSequence{0};
	std::uint32_t occurredAtMs{0};
};

enum class WebEventReadResult : std::uint8_t
{
	Available,
	NotYetAvailable,
	Gap
};

class WebEventJournal final
{
public:
	static constexpr std::size_t capacity{64};

	void publish(WebEvent event) noexcept;
	[[nodiscard]] WebEventReadResult read(std::uint32_t sequence, WebEvent &event) const noexcept;
	[[nodiscard]] std::uint32_t latestSequence() const noexcept;
	[[nodiscard]] std::uint32_t oldestSequence() const noexcept;
	void clear() noexcept;

private:
	[[nodiscard]] static std::uint32_t nextSequence(std::uint32_t current) noexcept;

	std::array<WebEvent, capacity> events_{};
	std::uint32_t latestSequence_{0};
	std::size_t count_{0};
};
}