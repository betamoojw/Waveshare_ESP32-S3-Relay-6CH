#include "knx_adapter.h"

#include <WiFi.h>

#include <limits>

namespace switch_actuator::adapters::knx
{
namespace
{
constexpr std::size_t invalidChannel{domain::relayChannelCount};
constexpr std::uint32_t correlationIdPrefix{0x4B00'0000U};
}

KnxAdapter::KnxAdapter(const KnxAdapterDependencies dependencies) noexcept
	: dependencies_{dependencies}
{
}

KnxInitializeResult KnxAdapter::initialize(const domain::KnxConfiguration &configuration,
												const std::uint32_t nowMs) noexcept
{
	initialized_ = false;
	transportStarted_ = false;
	telegramHandled_ = false;
	enabled_ = configuration.enabled;
	configuration_ = configuration;
	statusPublished_.fill(false);
	faultPublished_.fill(false);
	publishedFaultStates_.fill(false);
	publishedSequences_.fill(0);
	lastStatusPublishedAtMs_.fill(0);
	hasPublishedTelegram_ = false;
	deviceFaultPublished_ = false;
	publishedDeviceFaultState_ = false;
	if (dependencies_.switchingPolicy == nullptr || dependencies_.relayService == nullptr ||
		dependencies_.diagnostics == nullptr || !dependencies_.clock.isValid())
	{
		return KnxInitializeResult::InvalidDependencies;
	}

	if (!enabled_)
	{
		dependencies_.diagnostics->updateKnx(true, false);
		static_cast<void>(dependencies_.diagnostics->clearFault(domain::FaultCode::KnxUnavailable));
		static_cast<void>(dependencies_.diagnostics->clearFault(domain::FaultCode::KnxBusOff));
		initialized_ = true;
		return KnxInitializeResult::Disabled;
	}

	const auto callbackId = ::knx.callback_register("Relay switch", handleTelegram, this);
	if (callbackId == std::numeric_limits<callback_id_t>::max())
	{
		dependencies_.diagnostics->updateKnx(false, false);
		static_cast<void>(dependencies_.diagnostics->recordFault(
			domain::FaultCode::KnxUnavailable, domain::FaultSeverity::Warning, nowMs));
		return KnxInitializeResult::Unavailable;
	}
	for (const auto &channel : configuration_.channels)
	{
		if (channel.switchGroupAddress != 0)
		{
			::knx.callback_assign(callbackId, addressFromRaw(channel.switchGroupAddress));
		}
	}
	if (configuration_.centralSwitchGroupAddress != 0)
	{
		::knx.callback_assign(callbackId, addressFromRaw(configuration_.centralSwitchGroupAddress));
	}
	if (configuration_.centralOffGroupAddress != 0)
	{
		::knx.callback_assign(callbackId, addressFromRaw(configuration_.centralOffGroupAddress));
	}
	::knx.physical_address_set(addressFromRaw(configuration_.individualAddress));
	WiFi.mode(WIFI_STA);
	WiFi.setAutoReconnect(true);
	static_cast<void>(WiFi.begin());
	initialized_ = true;
	if (!startTransport(nowMs))
	{
		return KnxInitializeResult::Unavailable;
	}
	return KnxInitializeResult::Initialized;
}

KnxPollResult KnxAdapter::poll() noexcept
{
	if (!initialized_)
	{
		return KnxPollResult::NotInitialized;
	}
	if (!enabled_)
	{
		return KnxPollResult::Idle;
	}

	const auto nowMs = dependencies_.clock.nowMs();
	if (WiFi.status() != WL_CONNECTED)
	{
		dependencies_.diagnostics->updateKnx(true, false);
		if (transportStarted_)
		{
			static_cast<void>(dependencies_.diagnostics->recordFault(
				domain::FaultCode::KnxBusOff, domain::FaultSeverity::Warning, nowMs));
			transportStarted_ = false;
		}
		return KnxPollResult::Unavailable;
	}
	if (!transportStarted_ && !startTransport(nowMs))
	{
		return KnxPollResult::Unavailable;
	}

	telegramHandled_ = false;
	::knx.loop();
	publishPending(nowMs);
	return telegramHandled_ ? KnxPollResult::TelegramHandled : KnxPollResult::Idle;
}

bool KnxAdapter::isInitialized() const noexcept
{
	return initialized_;
}

bool KnxAdapter::isBusOnline() const noexcept
{
	return enabled_ && transportStarted_ && WiFi.status() == WL_CONNECTED;
}

void KnxAdapter::handleTelegram(const message_t &message, void *const context) noexcept
{
	if (context != nullptr)
	{
		static_cast<KnxAdapter *>(context)->onTelegram(message);
	}
}

void KnxAdapter::onTelegram(const message_t &message) noexcept
{
	telegramHandled_ = true;
	const auto channel = channelFor(message.received_on);
	const auto centralSwitch = matches(message.received_on, configuration_.centralSwitchGroupAddress);
	const auto centralOff = matches(message.received_on, configuration_.centralOffGroupAddress);
	if ((channel == invalidChannel && !centralSwitch && !centralOff) || message.data == nullptr || message.data_len == 0)
	{
		dependencies_.diagnostics->recordKnxTelegram(false);
		return;
	}

	if (message.ct == KNX_CT_READ)
	{
		if (channel == invalidChannel || !configuration_.readSwitchObject)
		{
			dependencies_.diagnostics->recordKnxTelegram(false);
			return;
		}
		const auto *const snapshot = dependencies_.relayService->snapshot(
			domain::RelayChannelId{static_cast<std::uint8_t>(channel)});
		if (snapshot == nullptr)
		{
			dependencies_.diagnostics->recordKnxTelegram(false);
			return;
		}
		const auto appliedOn = snapshot->appliedState == domain::RelayState::On;
		::knx.answer_1bit(message.received_on,
			appliedOn != configuration_.channels[channel].commandPolarityInverted ? 1 : 0);
		dependencies_.diagnostics->recordKnxTelegram(true);
		return;
	}
	if (message.ct != KNX_CT_WRITE)
	{
		dependencies_.diagnostics->recordKnxTelegram(false);
		return;
	}

	const auto value = ::knx.data_to_bool(message.data);
	const auto accepted = channel != invalidChannel ? enqueueChannelCommand(channel, value) :
		enqueueCentralCommand(value, centralOff);
	dependencies_.diagnostics->recordKnxTelegram(accepted);
	if (!accepted)
	{
		dependencies_.diagnostics->recordCommandQueueFull(dependencies_.clock.nowMs());
	}
}

bool KnxAdapter::startTransport(const std::uint32_t nowMs) noexcept
{
	if (WiFi.status() != WL_CONNECTED)
	{
		dependencies_.diagnostics->updateKnx(true, false);
		static_cast<void>(dependencies_.diagnostics->recordFault(
			domain::FaultCode::KnxBusOff, domain::FaultSeverity::Warning, nowMs));
		return false;
	}

	::knx.start(static_cast<WebServer *>(nullptr));
	transportStarted_ = true;
	transportStartedAtMs_ = nowMs;
	lastHeartbeatPublishedAtMs_ = nowMs;
	const auto &snapshots = dependencies_.relayService->snapshots();
	for (std::size_t channel = 0; channel < snapshots.size(); ++channel)
	{
		publishedSequences_[channel] = snapshots[channel].transitionSequence;
		statusPublished_[channel] = !configuration_.channels[channel].sendStatusAfterStartup ||
			configuration_.channels[channel].statusGroupAddress == 0;
		faultPublished_[channel] = configuration_.channels[channel].faultGroupAddress == 0;
		lastStatusPublishedAtMs_[channel] = nowMs;
	}
	dependencies_.diagnostics->updateKnx(true, true);
	static_cast<void>(dependencies_.diagnostics->clearFault(domain::FaultCode::KnxUnavailable));
	static_cast<void>(dependencies_.diagnostics->clearFault(domain::FaultCode::KnxBusOff));
	return true;
}

bool KnxAdapter::enqueueChannelCommand(const std::size_t channel, const bool value) noexcept
{
	const auto requestedOn = value != configuration_.channels[channel].commandPolarityInverted;
	return dependencies_.switchingPolicy->requestChannel(
		domain::RelayChannelId{static_cast<std::uint8_t>(channel)},
		requestedOn ? domain::RelayAction::SetOn : domain::RelayAction::SetOff,
		domain::CommandSource::Knx,
		nextCorrelationId(),
		dependencies_.clock.nowMs()) == app::SwitchingPolicyResult::Accepted;
}

bool KnxAdapter::enqueueCentralCommand(const bool value, const bool centralOff) noexcept
{
	if (centralOff && !value)
	{
		return true;
	}
	std::array<bool, domain::relayChannelCount> participants{};
	for (std::size_t channel = 0; channel < configuration_.channels.size(); ++channel)
	{
		participants[channel] = centralOff ? configuration_.channels[channel].participatesInCentralOff :
			configuration_.channels[channel].participatesInCentralSwitch;
	}
	const auto result = dependencies_.switchingPolicy->requestGroup(participants,
		centralOff || !value ? domain::RelayAction::SetOff : domain::RelayAction::SetOn,
		domain::CommandSource::Knx,
		nextCorrelationId(),
		dependencies_.clock.nowMs());
	return result == app::SwitchingPolicyResult::Accepted || result == app::SwitchingPolicyResult::NoParticipants;
}

void KnxAdapter::publishPending(const std::uint32_t nowMs) noexcept
{
	if (!elapsed(nowMs, transportStartedAtMs_, configuration_.startupTransmitDelayMs) || !canPublish(nowMs))
	{
		return;
	}
	static_cast<void>(publishChangedState(nowMs) || publishChangedFault(nowMs) || publishDeviceFault(nowMs) ||
		publishHeartbeat(nowMs));
}

bool KnxAdapter::publishChangedState(const std::uint32_t nowMs) noexcept
{
	const auto &snapshots = dependencies_.relayService->snapshots();
	for (std::size_t channel = 0; channel < snapshots.size(); ++channel)
	{
		const auto &channelConfiguration = configuration_.channels[channel];
		if (channelConfiguration.statusGroupAddress == 0)
		{
			continue;
		}
		const auto changed = publishedSequences_[channel] != snapshots[channel].transitionSequence;
		const auto cyclic = configuration_.cyclicStatusIntervalMs != 0 &&
			elapsed(nowMs, lastStatusPublishedAtMs_[channel], configuration_.cyclicStatusIntervalMs);
		if (statusPublished_[channel] && !changed && !cyclic)
		{
			continue;
		}
		const auto appliedOn = snapshots[channel].appliedState == domain::RelayState::On;
		::knx.write_1bit(addressFromRaw(channelConfiguration.statusGroupAddress),
			appliedOn != channelConfiguration.statusPolarityInverted ? 1 : 0);
		publishedSequences_[channel] = snapshots[channel].transitionSequence;
		lastStatusPublishedAtMs_[channel] = nowMs;
		statusPublished_[channel] = true;
		markPublished(nowMs);
		return true;
	}
	return false;
}

bool KnxAdapter::publishChangedFault(const std::uint32_t nowMs) noexcept
{
	const auto &snapshots = dependencies_.relayService->snapshots();
	for (std::size_t channel = 0; channel < snapshots.size(); ++channel)
	{
		const auto address = configuration_.channels[channel].faultGroupAddress;
		if (address == 0)
		{
			continue;
		}
		const auto faulted = snapshots[channel].fault != domain::RelayFault::None || snapshots[channel].lockedOut;
		if (faultPublished_[channel] && publishedFaultStates_[channel] == faulted)
		{
			continue;
		}
		::knx.write_1bit(addressFromRaw(address), faulted ? 1 : 0);
		publishedFaultStates_[channel] = faulted;
		faultPublished_[channel] = true;
		markPublished(nowMs);
		return true;
	}
	return false;
}

bool KnxAdapter::publishDeviceFault(const std::uint32_t nowMs) noexcept
{
	if (configuration_.deviceFaultGroupAddress == 0)
	{
		return false;
	}
	const auto faulted = dependencies_.diagnostics->snapshot().activeFaultCount != 0;
	if (deviceFaultPublished_ && publishedDeviceFaultState_ == faulted)
	{
		return false;
	}
	::knx.write_1bit(addressFromRaw(configuration_.deviceFaultGroupAddress), faulted ? 1 : 0);
	publishedDeviceFaultState_ = faulted;
	deviceFaultPublished_ = true;
	markPublished(nowMs);
	return true;
}

bool KnxAdapter::publishHeartbeat(const std::uint32_t nowMs) noexcept
{
	if (configuration_.heartbeatGroupAddress == 0 || configuration_.heartbeatIntervalMs == 0 ||
		!elapsed(nowMs, lastHeartbeatPublishedAtMs_, configuration_.heartbeatIntervalMs))
	{
		return false;
	}
	::knx.write_1bit(addressFromRaw(configuration_.heartbeatGroupAddress), 1);
	lastHeartbeatPublishedAtMs_ = nowMs;
	markPublished(nowMs);
	return true;
}

bool KnxAdapter::canPublish(const std::uint32_t nowMs) const noexcept
{
	return !hasPublishedTelegram_ || elapsed(nowMs, lastTelegramPublishedAtMs_, configuration_.minimumTelegramIntervalMs);
}

void KnxAdapter::markPublished(const std::uint32_t nowMs) noexcept
{
	hasPublishedTelegram_ = true;
	lastTelegramPublishedAtMs_ = nowMs;
}

std::size_t KnxAdapter::channelFor(const address_t address) const noexcept
{
	for (std::size_t channel = 0; channel < configuration_.channels.size(); ++channel)
	{
		if (matches(address, configuration_.channels[channel].switchGroupAddress))
		{
			return channel;
		}
	}
	return invalidChannel;
}

address_t KnxAdapter::addressFromRaw(const std::uint16_t address) noexcept
{
	address_t result{};
	result.bytes.high = static_cast<std::uint8_t>(address >> 8U);
	result.bytes.low = static_cast<std::uint8_t>(address & 0xFFU);
	return result;
}

std::uint32_t KnxAdapter::nextCorrelationId() noexcept
{
	correlationId_ = (correlationId_ + 1U) & 0x00FF'FFFFU;
	if (correlationId_ == 0)
	{
		correlationId_ = 1;
	}
	return correlationIdPrefix | correlationId_;
}

bool KnxAdapter::matches(const address_t address, const std::uint16_t configuredAddress) noexcept
{
	return configuredAddress != 0 && addressFromRaw(configuredAddress).value == address.value;
}

bool KnxAdapter::elapsed(const std::uint32_t nowMs, const std::uint32_t sinceMs, const std::uint32_t intervalMs) noexcept
{
	return static_cast<std::uint32_t>(nowMs - sinceMs) >= intervalMs;
}
}