#include "diagnostics_service.h"

#include <algorithm>
#include <limits>

namespace switch_actuator::app
{
DiagnosticsService::DiagnosticsService() noexcept
{
	for (std::size_t index = 0; index < snapshot_.faults.size(); ++index)
	{
		snapshot_.faults[index].code = static_cast<domain::FaultCode>(index);
	}
}

DiagnosticsResult DiagnosticsService::setIdentity(const std::string_view firmwareVersion,
														   const std::string_view buildId,
														   const std::string_view boardModel,
														   const std::string_view hardwareRevision) noexcept
{
	if (!isValidIdentity(firmwareVersion) || !isValidIdentity(buildId) || !isValidIdentity(boardModel) ||
		!isValidIdentity(hardwareRevision))
	{
		return DiagnosticsResult::InvalidIdentity;
	}

	static_cast<void>(copyIdentity(firmwareVersion, snapshot_.firmwareVersion));
	static_cast<void>(copyIdentity(buildId, snapshot_.buildId));
	static_cast<void>(copyIdentity(boardModel, snapshot_.boardModel));
	static_cast<void>(copyIdentity(hardwareRevision, snapshot_.hardwareRevision));
	return DiagnosticsResult::Applied;
}

void DiagnosticsService::updateRuntime(const std::uint32_t uptimeMs,
										   const std::uint32_t heapLowWaterMarkBytes,
										   const bool taskWatchdogHealthy) noexcept
{
	snapshot_.uptimeMs = uptimeMs;
	if (snapshot_.heapLowWaterMarkBytes == 0 || heapLowWaterMarkBytes < snapshot_.heapLowWaterMarkBytes)
	{
		snapshot_.heapLowWaterMarkBytes = heapLowWaterMarkBytes;
	}
	snapshot_.taskWatchdogHealthy = taskWatchdogHealthy;
}

void DiagnosticsService::updateConfiguration(const bool valid,
											 const std::uint32_t generation,
											 const ports::SettingsLoadResult loadResult,
											 const ports::SettingsSaveResult saveResult) noexcept
{
	snapshot_.configurationValid = valid;
	snapshot_.configurationGeneration = generation;
	snapshot_.lastSettingsLoadResult = loadResult;
	snapshot_.lastSettingsSaveResult = saveResult;
}

void DiagnosticsService::recordCommandAccepted() noexcept
{
	incrementSaturating(snapshot_.commands.accepted);
}

void DiagnosticsService::recordCommandRejected() noexcept
{
	incrementSaturating(snapshot_.commands.rejected);
}

void DiagnosticsService::recordCommandQueueFull(const std::uint32_t nowMs) noexcept
{
	incrementSaturating(snapshot_.commands.queueFull);
	static_cast<void>(recordFault(domain::FaultCode::CommandQueueOverflow, domain::FaultSeverity::Warning, nowMs));
}

void DiagnosticsService::recordModbus(const ModbusDiagnosticEvent event) noexcept
{
	switch (event)
	{
	case ModbusDiagnosticEvent::ValidRequest:
		incrementSaturating(snapshot_.modbus.validRequests);
		break;
	case ModbusDiagnosticEvent::CrcError:
		incrementSaturating(snapshot_.modbus.crcErrors);
		break;
	case ModbusDiagnosticEvent::MalformedFrame:
		incrementSaturating(snapshot_.modbus.malformedFrames);
		break;
	case ModbusDiagnosticEvent::IllegalFunction:
		incrementSaturating(snapshot_.modbus.illegalFunction);
		break;
	case ModbusDiagnosticEvent::IllegalAddress:
		incrementSaturating(snapshot_.modbus.illegalAddress);
		break;
	case ModbusDiagnosticEvent::IllegalValue:
		incrementSaturating(snapshot_.modbus.illegalValue);
		break;
	case ModbusDiagnosticEvent::Timeout:
		incrementSaturating(snapshot_.modbus.timeouts);
		break;
	default:
		break;
	}
}

void DiagnosticsService::updateKnx(const bool available, const bool busOnline) noexcept
{
	snapshot_.knx.available = available;
	snapshot_.knx.busOnline = available && busOnline;
}

void DiagnosticsService::recordKnxTelegram(const bool valid) noexcept
{
	incrementSaturating(valid ? snapshot_.knx.validTelegrams : snapshot_.knx.telegramErrors);
}

DiagnosticsResult DiagnosticsService::recordFault(const domain::FaultCode code,
														   const domain::FaultSeverity severity,
														   const std::uint32_t nowMs) noexcept
{
	if (!domain::isValid(code) || !domain::isValid(severity))
	{
		return DiagnosticsResult::InvalidFault;
	}

	auto &fault = snapshot_.faults[static_cast<std::size_t>(code)];
	const auto wasActive = fault.active;
	if (!wasActive)
	{
		fault.firstOccurredAtMs = nowMs;
		fault.active = true;
		incrementSaturating(snapshot_.activeFaultCount);
	}
	if (!wasActive || static_cast<std::uint8_t>(severity) > static_cast<std::uint8_t>(fault.severity))
	{
		fault.severity = severity;
	}
	fault.lastOccurredAtMs = nowMs;
	incrementSaturating(fault.occurrenceCount);
	return DiagnosticsResult::Applied;
}

DiagnosticsResult DiagnosticsService::clearFault(const domain::FaultCode code) noexcept
{
	if (!domain::isValid(code))
	{
		return DiagnosticsResult::InvalidFault;
	}

	auto &fault = snapshot_.faults[static_cast<std::size_t>(code)];
	if (fault.active)
	{
		fault.active = false;
		if (snapshot_.activeFaultCount > 0)
		{
			--snapshot_.activeFaultCount;
		}
	}
	return DiagnosticsResult::Applied;
}

ports::EventSink<LifecycleChanged> DiagnosticsService::lifecycleEventSink() noexcept
{
	return {lifecycleCallback, this};
}

ports::EventSink<domain::RelayStateChanged> DiagnosticsService::relayEventSink() noexcept
{
	return {relayCallback, this};
}

const DiagnosticsSnapshot &DiagnosticsService::snapshot() const noexcept
{
	return snapshot_;
}

bool DiagnosticsService::lifecycleCallback(void *const context, const LifecycleChanged &event) noexcept
{
	auto &service = *static_cast<DiagnosticsService *>(context);
	service.snapshot_.lifecycleState = event.state;
	service.snapshot_.lifecycleReason = event.reason;
	return true;
}

bool DiagnosticsService::relayCallback(void *const context, const domain::RelayStateChanged &event) noexcept
{
	auto &service = *static_cast<DiagnosticsService *>(context);
	if (event.channel.value >= service.snapshot_.relays.size())
	{
		return false;
	}

	auto &relay = service.snapshot_.relays[event.channel.value];
	relay.requestedState = event.appliedState;
	relay.appliedState = event.appliedState;
	relay.lastCommandSource = event.source;
	relay.transitionSequence = event.transitionSequence;
	relay.lastTransitionAtMs = event.occurredAtMs;
	return true;
}

bool DiagnosticsService::isValidIdentity(const std::string_view identity) noexcept
{
	return !identity.empty() && identity.size() < diagnosticIdentityCapacity;
}

bool DiagnosticsService::copyIdentity(const std::string_view source,
									  std::array<char, diagnosticIdentityCapacity> &destination) noexcept
{
	if (!isValidIdentity(source))
	{
		return false;
	}

	destination.fill('\0');
	std::copy(source.begin(), source.end(), destination.begin());
	return true;
}

void DiagnosticsService::incrementSaturating(std::uint32_t &counter) noexcept
{
	if (counter != std::numeric_limits<std::uint32_t>::max())
	{
		++counter;
	}
}
}