#pragma once

#include "lifecycle_supervisor.h"
#include "../domain/fault.h"
#include "../domain/relay_types.h"
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

struct DiagnosticsSnapshot final
{
	std::array<char, diagnosticIdentityCapacity> firmwareVersion{};
	std::array<char, diagnosticIdentityCapacity> buildId{};
	std::array<char, diagnosticIdentityCapacity> boardModel{};
	std::array<char, diagnosticIdentityCapacity> hardwareRevision{};
	std::uint32_t uptimeMs{0};
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

	[[nodiscard]] DiagnosticsResult setIdentity(std::string_view firmwareVersion,
											 std::string_view buildId,
											 std::string_view boardModel,
											 std::string_view hardwareRevision) noexcept;
	void updateRuntime(std::uint32_t uptimeMs, std::uint32_t heapLowWaterMarkBytes, bool taskWatchdogHealthy) noexcept;
	void updateConfiguration(bool valid,
							 std::uint32_t generation,
							 ports::SettingsLoadResult loadResult,
							 ports::SettingsSaveResult saveResult) noexcept;
	void recordCommandAccepted() noexcept;
	void recordCommandRejected() noexcept;
	void recordCommandQueueFull(std::uint32_t nowMs) noexcept;
	void recordModbus(ModbusDiagnosticEvent event) noexcept;
	void updateKnx(bool available, bool busOnline) noexcept;
	void recordKnxTelegram(bool valid) noexcept;
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

	DiagnosticsSnapshot snapshot_{};
};
}