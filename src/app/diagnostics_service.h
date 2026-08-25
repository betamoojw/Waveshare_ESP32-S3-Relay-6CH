#pragma once

#include "lifecycle_supervisor.h"
#include "../domain/diagnostic_counters.h"
#include "../domain/device_identity.h"
#include "../domain/fault.h"
#include "../domain/relay_types.h"
#include "../domain/relay_policy.h"
#include "../ports/network_status_port.h"
#include "../ports/event_sink.h"
#include "../ports/settings_store.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace switch_actuator::app
{
inline constexpr std::size_t diagnosticIdentityCapacity{32};

struct CommandCounters final
{
	std::uint32_t accepted{0};
	std::uint32_t rejected{0};
	std::uint32_t queueFull{0};
};

struct ModbusCounters final
{
	bool available{false};
	std::uint32_t validRequests{0};
	std::uint32_t crcErrors{0};
	std::uint32_t malformedFrames{0};
	std::uint32_t illegalFunction{0};
	std::uint32_t illegalAddress{0};
	std::uint32_t illegalValue{0};
	std::uint32_t timeouts{0};
};

struct KnxCounters final
{
	std::uint32_t validTelegrams{0};
	std::uint32_t telegramErrors{0};
	bool available{false};
	bool busOnline{false};
};

struct RuntimeDiagnostics final
{
	std::uint32_t freeHeapBytes{0};
	std::uint32_t minimumFreeHeapBytes{0};
	std::uint32_t largestFreeHeapBlockBytes{0};
	std::uint32_t psramTotalBytes{0};
	std::uint32_t psramFreeBytes{0};
	std::uint32_t psramMinimumFreeBytes{0};
	std::uint32_t cpuFrequencyMhz{0};
	std::uint8_t cpuCoreCount{0};
	bool taskWatchdogHealthy{false};
};

struct NetworkDiagnostics final
{
	ports::NetworkLifecycleState state{ports::NetworkLifecycleState::Disabled};
	ports::NetworkTransport activeTransport{ports::NetworkTransport::None};
	bool connected{false};
	bool wifiAvailable{false};
	bool ethernetAvailable{false};
	bool recoveryApActive{false};
	std::int32_t wifiRssiDbm{0};
	std::array<std::uint8_t, 4> ipv4Address{};
};

struct StorageDiagnostics final
{
	bool filesystemAvailable{false};
	bool settingsAvailable{false};
	bool settingsHealthy{false};
};

struct DiagnosticsSnapshot final
{
	domain::DeviceIdentity identity{};
	std::array<char, diagnosticIdentityCapacity> firmwareVersion{};
	std::array<char, diagnosticIdentityCapacity> buildId{};
	std::array<char, diagnosticIdentityCapacity> boardModel{};
	std::array<char, diagnosticIdentityCapacity> hardwareRevision{};
	std::uint32_t uptimeMs{0};
	std::uint32_t bootCount{0};
	domain::PersistentDiagnosticCounters persistentCounters{};
	domain::ResetCategory resetReason{domain::ResetCategory::Unknown};
	RuntimeDiagnostics runtime{};
	NetworkDiagnostics network{};
	StorageDiagnostics storage{};
	LifecycleState lifecycleState{LifecycleState::Booting};
	LifecycleReason lifecycleReason{LifecycleReason::Startup};
	bool configurationValid{false};
	std::uint32_t configurationGeneration{0};
	ports::SettingsLoadResult lastSettingsLoadResult{ports::SettingsLoadResult::NotFound};
	ports::SettingsSaveResult lastSettingsSaveResult{ports::SettingsSaveResult::Saved};
	std::array<domain::RelaySnapshot, domain::relayChannelCount> relays{};
	CommandCounters commands{};
	ModbusCounters modbus{};
	KnxCounters knx{};
	std::array<domain::FaultRecord, domain::faultCodeCount> faults{};
	std::uint32_t activeFaultCount{0};
	std::uint32_t heapLowWaterMarkBytes{0};
	bool taskWatchdogHealthy{false};
};

enum class ModbusDiagnosticEvent : std::uint8_t
{
	ValidRequest,
	CrcError,
	MalformedFrame,
	IllegalFunction,
	IllegalAddress,
	IllegalValue,
	Timeout
};

enum class DiagnosticsResult : std::uint8_t
{
	Applied,
	InvalidIdentity,
	InvalidFault,
	InvalidChannel
};

class DiagnosticsService final
{
public:
	DiagnosticsService() noexcept;

	[[nodiscard]] DiagnosticsResult setIdentity(const domain::DeviceIdentity &identity,
											 std::string_view buildId) noexcept;
	void updateRuntime(std::uint32_t uptimeMs, std::uint32_t heapLowWaterMarkBytes, bool taskWatchdogHealthy) noexcept;
	void updateRuntime(std::uint32_t uptimeMs, const RuntimeDiagnostics &runtime) noexcept;
	void updateBoot(std::uint32_t bootCount, domain::ResetCategory resetReason) noexcept;
	void setPersistentCounters(const domain::PersistentDiagnosticCounters &counters) noexcept;
	void markPersistentCountersSaved() noexcept;
	[[nodiscard]] bool persistentCountersDirty() const noexcept;
	void updateNetwork(const ports::NetworkStatusSnapshot &network) noexcept;
	void updateStorage(bool filesystemAvailable, bool settingsAvailable, bool settingsHealthy) noexcept;
	void updateConfiguration(bool valid,
							 std::uint32_t generation,
							 ports::SettingsLoadResult loadResult,
							 ports::SettingsSaveResult saveResult) noexcept;
	void recordCommandAccepted() noexcept;
	void recordCommandRejected() noexcept;
	void recordCommandQueueFull(std::uint32_t nowMs) noexcept;
	void recordModbus(ModbusDiagnosticEvent event) noexcept;
	void updateModbus(bool available) noexcept;
	void updateKnx(bool available, bool busOnline) noexcept;
	void recordKnxTelegram(bool valid) noexcept;
	void recordOtaFailure() noexcept;
	void recordStorageFailure() noexcept;
	[[nodiscard]] DiagnosticsResult recordFault(domain::FaultCode code, domain::FaultSeverity severity, std::uint32_t nowMs) noexcept;
	[[nodiscard]] DiagnosticsResult clearFault(domain::FaultCode code) noexcept;

	[[nodiscard]] ports::EventSink<LifecycleChanged> lifecycleEventSink() noexcept;
	[[nodiscard]] ports::EventSink<domain::RelayStateChanged> relayEventSink() noexcept;
	[[nodiscard]] const DiagnosticsSnapshot &snapshot() const noexcept;

private:
	[[nodiscard]] static bool lifecycleCallback(void *context, const LifecycleChanged &event) noexcept;
	[[nodiscard]] static bool relayCallback(void *context, const domain::RelayStateChanged &event) noexcept;
	[[nodiscard]] static bool isValidIdentity(std::string_view identity) noexcept;
	[[nodiscard]] static bool copyIdentity(std::string_view source,
										 std::array<char, diagnosticIdentityCapacity> &destination) noexcept;
	static void incrementSaturating(std::uint32_t &counter) noexcept;
	void incrementPersistent(std::uint32_t &counter) noexcept;
	void recordPersistentFault(domain::FaultCode code) noexcept;

	DiagnosticsSnapshot snapshot_{};
	bool persistentCountersDirty_{false};
	bool networkStateObserved_{false};
};
}