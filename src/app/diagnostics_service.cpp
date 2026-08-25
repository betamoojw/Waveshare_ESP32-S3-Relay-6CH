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

DiagnosticsResult DiagnosticsService::setIdentity(const domain::DeviceIdentity &identity,
													   const std::string_view buildId) noexcept
{
	const std::string_view firmwareVersion{identity.firmwareVersion.value.data()};
	const std::string_view hardwareModel{identity.hardwareModel.value.data()};
	const std::string_view hardwareRevision{identity.hardwareRevision.value.data()};
	if (!domain::isValid(identity) || !isValidIdentity(buildId) || !isValidIdentity(firmwareVersion) ||
		!isValidIdentity(hardwareModel) || !isValidIdentity(hardwareRevision))
	{
		return DiagnosticsResult::InvalidIdentity;
	}

	snapshot_.identity = identity;
	static_cast<void>(copyIdentity(firmwareVersion, snapshot_.firmwareVersion));
	static_cast<void>(copyIdentity(buildId, snapshot_.buildId));
	static_cast<void>(copyIdentity(hardwareModel, snapshot_.boardModel));
	static_cast<void>(copyIdentity(hardwareRevision, snapshot_.hardwareRevision));
	return DiagnosticsResult::Applied;
}

void DiagnosticsService::updateRuntime(const std::uint32_t uptimeMs,
										   const std::uint32_t heapLowWaterMarkBytes,
										   const bool taskWatchdogHealthy) noexcept
{
	RuntimeDiagnostics runtime{};
	runtime.minimumFreeHeapBytes = heapLowWaterMarkBytes;
	runtime.taskWatchdogHealthy = taskWatchdogHealthy;
	updateRuntime(uptimeMs, runtime);
}

void DiagnosticsService::updateRuntime(const std::uint32_t uptimeMs,
	const RuntimeDiagnostics &runtime) noexcept
{
	snapshot_.uptimeMs = uptimeMs;
	snapshot_.runtime = runtime;
	snapshot_.heapLowWaterMarkBytes = runtime.minimumFreeHeapBytes;
	snapshot_.taskWatchdogHealthy = runtime.taskWatchdogHealthy;
}

void DiagnosticsService::updateBoot(const std::uint32_t bootCount,
	const domain::ResetCategory resetReason) noexcept
{
	snapshot_.bootCount = bootCount;
	snapshot_.resetReason = resetReason;
}

void DiagnosticsService::setPersistentCounters(const domain::PersistentDiagnosticCounters &counters) noexcept
{
	snapshot_.persistentCounters = counters;
	snapshot_.bootCount = counters.bootCount;
	persistentCountersDirty_ = false;
}

void DiagnosticsService::markPersistentCountersSaved() noexcept
{
	persistentCountersDirty_ = false;
}

bool DiagnosticsService::persistentCountersDirty() const noexcept
{
	return persistentCountersDirty_;
}

void DiagnosticsService::updateNetwork(const ports::NetworkStatusSnapshot &network) noexcept
{
	const auto failedAfterOnline = networkStateObserved_ && snapshot_.network.connected && !network.infrastructureOnline;
	const auto enteredRecovery = networkStateObserved_ && !snapshot_.network.recoveryApActive && network.recoveryApActive;
	if (failedAfterOnline || enteredRecovery)
	{
		incrementPersistent(snapshot_.persistentCounters.networkFailureCount);
	}
	snapshot_.network.state = network.state;
	snapshot_.network.activeTransport = network.activeTransport;
	snapshot_.network.connected = network.infrastructureOnline;
	snapshot_.network.wifiAvailable = network.wifiAvailable;
	snapshot_.network.ethernetAvailable = network.ethernetAvailable;
	snapshot_.network.recoveryApActive = network.recoveryApActive;
	snapshot_.network.wifiRssiDbm = network.rssi;
	snapshot_.network.ipv4Address = network.ipv4Address;
	networkStateObserved_ = true;
}

void DiagnosticsService::updateStorage(const bool filesystemAvailable,
	const bool settingsAvailable,
	const bool settingsHealthy) noexcept
{
	snapshot_.storage = {filesystemAvailable, settingsAvailable, settingsHealthy};
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
		incrementPersistent(snapshot_.persistentCounters.modbusErrorCount);
		break;
	case ModbusDiagnosticEvent::MalformedFrame:
		incrementSaturating(snapshot_.modbus.malformedFrames);
		incrementPersistent(snapshot_.persistentCounters.modbusErrorCount);
		break;
	case ModbusDiagnosticEvent::IllegalFunction:
		incrementSaturating(snapshot_.modbus.illegalFunction);
		incrementPersistent(snapshot_.persistentCounters.modbusErrorCount);
		break;
	case ModbusDiagnosticEvent::IllegalAddress:
		incrementSaturating(snapshot_.modbus.illegalAddress);
		incrementPersistent(snapshot_.persistentCounters.modbusErrorCount);
		break;
	case ModbusDiagnosticEvent::IllegalValue:
		incrementSaturating(snapshot_.modbus.illegalValue);
		incrementPersistent(snapshot_.persistentCounters.modbusErrorCount);
		break;
	case ModbusDiagnosticEvent::Timeout:
		incrementSaturating(snapshot_.modbus.timeouts);
		incrementPersistent(snapshot_.persistentCounters.modbusErrorCount);
		break;
	default:
		break;
	}
}

void DiagnosticsService::updateModbus(const bool available) noexcept
{
	snapshot_.modbus.available = available;
}

void DiagnosticsService::updateKnx(const bool available, const bool busOnline) noexcept
{
	snapshot_.knx.available = available;
	snapshot_.knx.busOnline = available && busOnline;
}

void DiagnosticsService::recordKnxTelegram(const bool valid) noexcept
{
	incrementSaturating(valid ? snapshot_.knx.validTelegrams : snapshot_.knx.telegramErrors);
	if (!valid)
	{
		incrementPersistent(snapshot_.persistentCounters.knxErrorCount);
	}
}

void DiagnosticsService::recordOtaFailure() noexcept
{
	incrementPersistent(snapshot_.persistentCounters.otaFailureCount);
}

void DiagnosticsService::recordStorageFailure() noexcept
{
	incrementPersistent(snapshot_.persistentCounters.storageErrorCount);
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
		recordPersistentFault(code);
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

void DiagnosticsService::incrementPersistent(std::uint32_t &counter) noexcept
{
	const auto previous = counter;
	incrementSaturating(counter);
	persistentCountersDirty_ = persistentCountersDirty_ || counter != previous;
}

void DiagnosticsService::recordPersistentFault(const domain::FaultCode code) noexcept
{
	switch (code)
	{
	case domain::FaultCode::InvalidConfiguration:
		incrementPersistent(snapshot_.persistentCounters.configErrorCount);
		break;
	case domain::FaultCode::TaskWatchdogFailure:
		incrementPersistent(snapshot_.persistentCounters.watchdogCount);
		break;
	case domain::FaultCode::SettingsLoadFailure:
	case domain::FaultCode::SettingsSaveFailure:
	case domain::FaultCode::FileSystemFailure:
		incrementPersistent(snapshot_.persistentCounters.storageErrorCount);
		break;
	default:
		break;
	}
}
}