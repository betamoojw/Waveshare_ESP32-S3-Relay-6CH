#pragma once

#include "../domain/configuration.h"
#include "../ports/configuration_source.h"
#include "../ports/settings_store.h"

#include <cstdint>
#include <optional>

namespace switch_actuator::app
{
enum class ConfigurationInitializeResult : std::uint8_t
{
	Loaded,
	DefaultsLoaded,
	SafeDefaultsApplied,
	InvalidStore
};

enum class ConfigurationStageResult : std::uint8_t
{
	Staged,
	InvalidConfiguration
};

enum class ConfigurationCommitResult : std::uint8_t
{
	Committed,
	CommittedRestartRequired,
	NothingStaged,
	PersistenceFailure
};

enum class ConfigurationFactoryResetResult : std::uint8_t
{
	Erased,
	InvalidIdentity,
	PersistenceFailure
};

enum class ConfigurationUserResetResult : std::uint8_t
{
	Erased,
	InvalidIdentity,
	PersistenceFailure
};

class ConfigurationService final
{
public:
	explicit ConfigurationService(ports::SettingsStore settingsStore) noexcept;

	void setDefaultSource(ports::ConfigurationSource defaultSource) noexcept;
	[[nodiscard]] ConfigurationInitializeResult initialize() noexcept;
	[[nodiscard]] ConfigurationStageResult stage(const domain::Configuration &configuration) noexcept;
	[[nodiscard]] ConfigurationCommitResult commit() noexcept;
	[[nodiscard]] ConfigurationFactoryResetResult factoryReset() noexcept;
	[[nodiscard]] ConfigurationUserResetResult eraseUserConfiguration() noexcept;
	void discardStaged() noexcept;

	[[nodiscard]] const domain::Configuration &active() const noexcept;
	[[nodiscard]] const domain::Configuration *staged() const noexcept;
	[[nodiscard]] domain::ConfigurationValidationError lastValidationError() const noexcept;
	[[nodiscard]] ports::SettingsLoadResult lastLoadResult() const noexcept;
	[[nodiscard]] ports::SettingsSaveResult lastSaveResult() const noexcept;
	[[nodiscard]] bool hasValidActiveConfiguration() const noexcept;
	[[nodiscard]] bool hasStagedConfiguration() const noexcept;

private:
	[[nodiscard]] static bool transportRestartRequired(const domain::Configuration &current,
															 const domain::Configuration &replacement) noexcept;
	[[nodiscard]] static std::uint32_t nextGeneration(std::uint32_t current) noexcept;

	ports::SettingsStore settingsStore_;
	ports::ConfigurationSource defaultSource_;
	domain::Configuration active_{domain::makeSafeConfiguration()};
	std::optional<domain::Configuration> staged_{};
	domain::ConfigurationValidationError lastValidationError_{domain::ConfigurationValidationError::None};
	ports::SettingsLoadResult lastLoadResult_{ports::SettingsLoadResult::NotFound};
	ports::SettingsSaveResult lastSaveResult_{ports::SettingsSaveResult::Saved};
	bool activeConfigurationValid_{false};
};
}