#pragma once

#include "../../app/diagnostics_service.h"
#include "../../app/relay_command_service.h"
#include "../../app/switching_policy_service.h"
#include "../../domain/configuration.h"
#include "../../ports/clock_port.h"
#include "../../ports/network_status_port.h"

#include <esp-knx-ip.h>

#include <array>
#include <cstdint>

namespace switch_actuator::adapters::knx
{
enum class KnxInitializeResult : std::uint8_t
{
	Initialized,
	Disabled,
	Unavailable,
	InvalidDependencies
};

enum class KnxPollResult : std::uint8_t
{
	Idle,
	TelegramHandled,
	Unavailable,
	NotInitialized
};

struct KnxAdapterDependencies final
{
	app::SwitchingPolicyService *switchingPolicy;
	app::RelayCommandService *relayService;
	app::DiagnosticsService *diagnostics;
	ports::ClockPort clock;
	ports::NetworkStatusPort networkStatus;
};

class KnxAdapter final
{
public:
	explicit KnxAdapter(KnxAdapterDependencies dependencies) noexcept;

	[[nodiscard]] KnxInitializeResult initialize(const domain::KnxConfiguration &configuration,
												 std::uint32_t nowMs) noexcept;
	[[nodiscard]] KnxPollResult poll() noexcept;
	[[nodiscard]] bool isInitialized() const noexcept;
	[[nodiscard]] constexpr bool isAvailable() const noexcept
	{
		return true;
	}
	[[nodiscard]] bool isBusOnline() const noexcept;

private:
	static void handleTelegram(const message_t &message, void *context) noexcept;
	void onTelegram(const message_t &message) noexcept;
	[[nodiscard]] bool startTransport(std::uint32_t nowMs) noexcept;
	[[nodiscard]] bool enqueueChannelCommand(std::size_t channel, bool value) noexcept;
	[[nodiscard]] bool enqueueCentralCommand(bool value, bool centralOff) noexcept;
	void publishPending(std::uint32_t nowMs) noexcept;
	[[nodiscard]] bool publishChangedState(std::uint32_t nowMs) noexcept;
	[[nodiscard]] bool publishChangedFault(std::uint32_t nowMs) noexcept;
	[[nodiscard]] bool publishDeviceFault(std::uint32_t nowMs) noexcept;
	[[nodiscard]] bool publishHeartbeat(std::uint32_t nowMs) noexcept;
	[[nodiscard]] bool canPublish(std::uint32_t nowMs) const noexcept;
	void markPublished(std::uint32_t nowMs) noexcept;
	[[nodiscard]] std::size_t channelFor(address_t address) const noexcept;
	[[nodiscard]] static bool matches(address_t address, std::uint16_t configuredAddress) noexcept;
	[[nodiscard]] static bool elapsed(std::uint32_t nowMs, std::uint32_t sinceMs, std::uint32_t intervalMs) noexcept;
	[[nodiscard]] static address_t addressFromRaw(std::uint16_t address) noexcept;
	[[nodiscard]] std::uint32_t nextCorrelationId() noexcept;

	KnxAdapterDependencies dependencies_;
	domain::KnxConfiguration configuration_{};
	std::array<std::uint32_t, domain::relayChannelCount> publishedSequences_{};
	std::array<std::uint32_t, domain::relayChannelCount> lastStatusPublishedAtMs_{};
	std::array<bool, domain::relayChannelCount> statusPublished_{};
	std::array<bool, domain::relayChannelCount> faultPublished_{};
	std::array<bool, domain::relayChannelCount> publishedFaultStates_{};
	std::uint32_t correlationId_{0};
	std::uint32_t transportStartedAtMs_{0};
	std::uint32_t lastTelegramPublishedAtMs_{0};
	std::uint32_t lastHeartbeatPublishedAtMs_{0};
	bool hasPublishedTelegram_{false};
	bool deviceFaultPublished_{false};
	bool publishedDeviceFaultState_{false};
	bool enabled_{false};
	bool initialized_{false};
	bool transportStarted_{false};
	bool telegramHandled_{false};
};
}