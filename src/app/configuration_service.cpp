#include "configuration_service.h"

#include <limits>

namespace switch_actuator::app
{
namespace
{
[[nodiscard]] bool sameKnxChannelConfiguration(const domain::KnxChannelConfiguration &left,
												 const domain::KnxChannelConfiguration &right) noexcept
{
	return left.switchGroupAddress == right.switchGroupAddress && left.statusGroupAddress == right.statusGroupAddress &&
		left.faultGroupAddress == right.faultGroupAddress &&
		left.commandPolarityInverted == right.commandPolarityInverted &&
		left.statusPolarityInverted == right.statusPolarityInverted &&
		left.sendStatusAfterStartup == right.sendStatusAfterStartup &&
		left.participatesInCentralSwitch == right.participatesInCentralSwitch &&
		left.participatesInCentralOff == right.participatesInCentralOff;
}

[[nodiscard]] bool sameKnxConfiguration(const domain::KnxConfiguration &left,
										 const domain::KnxConfiguration &right) noexcept
{
	if (left.enabled != right.enabled || left.individualAddress != right.individualAddress ||
		left.startupTransmitDelayMs != right.startupTransmitDelayMs ||
		left.minimumTelegramIntervalMs != right.minimumTelegramIntervalMs ||
		left.cyclicStatusIntervalMs != right.cyclicStatusIntervalMs ||
		left.heartbeatIntervalMs != right.heartbeatIntervalMs || left.readSwitchObject != right.readSwitchObject ||
		left.heartbeatGroupAddress != right.heartbeatGroupAddress ||
		left.centralSwitchGroupAddress != right.centralSwitchGroupAddress ||
		left.centralOffGroupAddress != right.centralOffGroupAddress ||
		left.deviceFaultGroupAddress != right.deviceFaultGroupAddress)
	{
		return false;
	}
	for (std::size_t channel = 0; channel < left.channels.size(); ++channel)
	{
		if (!sameKnxChannelConfiguration(left.channels[channel], right.channels[channel]))
		{
			return false;
		}
	}
	return true;
}
}

ConfigurationService::ConfigurationService(const ports::IStorage storage) noexcept
	: storage_{storage}
{
}

void ConfigurationService::setDefaultSource(const ports::ConfigurationSource defaultSource) noexcept
{
	defaultSource_ = defaultSource;
}

ConfigurationInitializeResult ConfigurationService::initialize() noexcept
{
	staged_.reset();
	active_ = domain::makeSafeConfiguration();
	activeConfigurationValid_ = false;
	lastValidationError_ = domain::ConfigurationValidationError::None;
	if (storage_.isValid())
	{
		domain::Configuration loaded{};
		lastLoadResult_ = storage_.load(loaded);
		if (lastLoadResult_ == ports::SettingsLoadResult::Loaded)
		{
			lastValidationError_ = domain::validateConfiguration(loaded);
			if (lastValidationError_ == domain::ConfigurationValidationError::None)
			{
				active_ = loaded;
				activeConfigurationValid_ = true;
				return ConfigurationInitializeResult::Loaded;
			}
			lastLoadResult_ = ports::SettingsLoadResult::Corrupt;
		}
	}
	else
	{
		lastLoadResult_ = ports::SettingsLoadResult::IoFailure;
	}

	domain::Configuration defaults{};
	if (defaultSource_.isValid() && defaultSource_.load(defaults) == ports::ConfigurationSourceResult::Loaded)
	{
		lastValidationError_ = domain::validateConfiguration(defaults);
		if (lastValidationError_ == domain::ConfigurationValidationError::None)
		{
			active_ = defaults;
			activeConfigurationValid_ = true;
			return ConfigurationInitializeResult::DefaultsLoaded;
		}
	}

	return storage_.isValid() ? ConfigurationInitializeResult::SafeDefaultsApplied
											 : ConfigurationInitializeResult::InvalidStore;
}

ConfigurationStageResult ConfigurationService::stage(const domain::Configuration &configuration) noexcept
{
	lastValidationError_ = domain::validateConfiguration(configuration);
	if (lastValidationError_ != domain::ConfigurationValidationError::None)
	{
		return ConfigurationStageResult::InvalidConfiguration;
	}

	staged_ = configuration;
	staged_->generation = nextGeneration(active_.generation);
	return ConfigurationStageResult::Staged;
}

ConfigurationCommitResult ConfigurationService::commit() noexcept
{
	if (!staged_.has_value())
	{
		return ConfigurationCommitResult::NothingStaged;
	}

	const auto restartRequired = transportRestartRequired(active_, *staged_);
	lastSaveResult_ = storage_.save(*staged_);
	if (lastSaveResult_ != ports::SettingsSaveResult::Saved)
	{
		return ConfigurationCommitResult::PersistenceFailure;
	}

	active_ = *staged_;
	staged_.reset();
	activeConfigurationValid_ = true;
	return restartRequired ? ConfigurationCommitResult::CommittedRestartRequired : ConfigurationCommitResult::Committed;
}

ConfigurationFactoryResetResult ConfigurationService::factoryReset() noexcept
{
	switch (eraseUserConfiguration())
	{
	case ConfigurationUserResetResult::Erased:
		return ConfigurationFactoryResetResult::Erased;
	case ConfigurationUserResetResult::InvalidIdentity:
		return ConfigurationFactoryResetResult::InvalidIdentity;
	case ConfigurationUserResetResult::PersistenceFailure:
		return ConfigurationFactoryResetResult::PersistenceFailure;
	}
	return ConfigurationFactoryResetResult::PersistenceFailure;
}

ConfigurationUserResetResult ConfigurationService::eraseUserConfiguration() noexcept
{
	auto replacement = domain::makeSafeConfiguration();
	replacement.productId = active_.productId;
	replacement.boardModel = active_.boardModel;
	replacement.hardwareRevision = active_.hardwareRevision;
	replacement.deviceSerial = active_.deviceSerial;
	replacement.deviceUuid = active_.deviceUuid;
	replacement.manufacturingDate = active_.manufacturingDate;
	replacement.manufacturingBatch = active_.manufacturingBatch;
	if (stage(replacement) != ConfigurationStageResult::Staged)
	{
		return ConfigurationUserResetResult::InvalidIdentity;
	}
	const auto result = commit();
	return result == ConfigurationCommitResult::Committed || result == ConfigurationCommitResult::CommittedRestartRequired ?
		ConfigurationUserResetResult::Erased : ConfigurationUserResetResult::PersistenceFailure;
}

void ConfigurationService::discardStaged() noexcept
{
	staged_.reset();
}

const domain::Configuration &ConfigurationService::active() const noexcept
{
	return active_;
}

const domain::Configuration *ConfigurationService::staged() const noexcept
{
	return staged_.has_value() ? &*staged_ : nullptr;
}

domain::ConfigurationValidationError ConfigurationService::lastValidationError() const noexcept
{
	return lastValidationError_;
}

ports::SettingsLoadResult ConfigurationService::lastLoadResult() const noexcept
{
	return lastLoadResult_;
}

ports::SettingsSaveResult ConfigurationService::lastSaveResult() const noexcept
{
	return lastSaveResult_;
}

bool ConfigurationService::hasValidActiveConfiguration() const noexcept
{
	return activeConfigurationValid_;
}

bool ConfigurationService::hasStagedConfiguration() const noexcept
{
	return staged_.has_value();
}

bool ConfigurationService::transportRestartRequired(const domain::Configuration &current,
																	 const domain::Configuration &replacement) noexcept
{
	return current.modbus.unitId != replacement.modbus.unitId || current.modbus.baudRate != replacement.modbus.baudRate ||
		   current.modbus.parity != replacement.modbus.parity || current.modbus.dataBits != replacement.modbus.dataBits ||
		   current.modbus.stopBits != replacement.modbus.stopBits || !sameKnxConfiguration(current.knx, replacement.knx) ||
		   current.web.enabled != replacement.web.enabled;
}

std::uint32_t ConfigurationService::nextGeneration(const std::uint32_t current) noexcept
{
	return current == std::numeric_limits<std::uint32_t>::max() ? current : current + 1;
}
}