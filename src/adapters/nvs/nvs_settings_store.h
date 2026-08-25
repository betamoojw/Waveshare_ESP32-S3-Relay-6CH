#pragma once

#include "../../domain/diagnostic_counters.h"
#include "../../domain/relay_policy.h"
#include "../../ports/settings_store.h"

#include <Preferences.h>

#include <cstdint>

namespace switch_actuator::adapters::nvs
{
enum class NvsInitializeResult : std::uint8_t
{
	Initialized,
	OpenFailure
};

struct DiagnosticCountersBootResult final
{
	domain::PersistentDiagnosticCounters counters{};
	bool persisted{false};
};

class NvsSettingsStore final
{
public:
	NvsSettingsStore() noexcept = default;
	~NvsSettingsStore();

	NvsSettingsStore(const NvsSettingsStore &) = delete;
	NvsSettingsStore &operator=(const NvsSettingsStore &) = delete;
	NvsSettingsStore(NvsSettingsStore &&) = delete;
	NvsSettingsStore &operator=(NvsSettingsStore &&) = delete;

	[[nodiscard]] NvsInitializeResult initialize() noexcept;
	[[nodiscard]] DiagnosticCountersBootResult beginDiagnosticCounters(domain::ResetCategory resetReason) noexcept;
	[[nodiscard]] bool saveDiagnosticCounters(const domain::PersistentDiagnosticCounters &counters) noexcept;
	[[nodiscard]] ports::SettingsStore port() noexcept;
	[[nodiscard]] bool isInitialized() const noexcept;

private:
	[[nodiscard]] static ports::SettingsLoadResult loadCallback(void *context, domain::Configuration &configuration) noexcept;
	[[nodiscard]] static ports::SettingsSaveResult saveCallback(void *context, const domain::Configuration &configuration) noexcept;
	[[nodiscard]] static ports::SettingsEraseResult eraseCallback(void *context) noexcept;
	[[nodiscard]] ports::SettingsLoadResult load(domain::Configuration &configuration) noexcept;
	[[nodiscard]] ports::SettingsSaveResult save(const domain::Configuration &configuration) noexcept;
	[[nodiscard]] ports::SettingsEraseResult erase() noexcept;

	Preferences preferences_{};
	bool initialized_{false};
};
}