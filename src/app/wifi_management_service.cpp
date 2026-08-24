#include "wifi_management_service.h"

#include <algorithm>

namespace switch_actuator::app
{
WifiManagementService::WifiManagementService(ConfigurationService &configurationService) noexcept
	: configurationService_{configurationService}
{
}

WifiManagementSnapshot WifiManagementService::snapshot() const noexcept
{
	WifiManagementSnapshot result{};
	const auto &active = configurationService_.active();
	result.generation = active.generation;
	result.recoveryAp = active.network.recoveryAp;
	for (std::size_t index = 0; index < active.network.wifiProfiles.size(); ++index)
	{
		const auto &source = active.network.wifiProfiles[index];
		auto &destination = result.profiles[index];
		destination.index = static_cast<std::uint8_t>(index);
		destination.enabled = source.enabled;
		destination.ssid = source.ssid;
		destination.hasPassphrase = source.passphrase.front() != '\0';
		destination.ipv4 = source.ipv4;
	}
	return result;
}

WifiManagementResult WifiManagementService::saveProfile(const WifiProfilePatch &patch) noexcept
{
	if (patch.index >= domain::wifiProfileCount)
	{
		return WifiManagementResult::InvalidIndex;
	}
	auto replacement = configurationService_.active();
	auto &profile = replacement.network.wifiProfiles[patch.index];
	profile.enabled = patch.enabled;
	profile.ssid = patch.ssid;
	profile.ipv4 = patch.ipv4;
	if (patch.passphraseUpdate == WifiSecretUpdate::Replace)
	{
		profile.passphrase = patch.passphrase;
	}
	else if (patch.passphraseUpdate == WifiSecretUpdate::Clear)
	{
		profile.passphrase.fill('\0');
	}
	return commit(replacement, patch.expectedGeneration);
}

WifiManagementResult WifiManagementService::removeProfile(const std::uint8_t index,
	const std::uint32_t expectedGeneration) noexcept
{
	if (index >= domain::wifiProfileCount)
	{
		return WifiManagementResult::InvalidIndex;
	}
	auto replacement = configurationService_.active();
	auto &profiles = replacement.network.wifiProfiles;
	std::move(profiles.begin() + index + 1, profiles.end(), profiles.begin() + index);
	profiles.back() = {};
	return commit(replacement, expectedGeneration);
}

WifiManagementResult WifiManagementService::moveProfile(const std::uint8_t fromIndex,
	const std::uint8_t toIndex,
	const std::uint32_t expectedGeneration) noexcept
{
	if (fromIndex >= domain::wifiProfileCount || toIndex >= domain::wifiProfileCount)
	{
		return WifiManagementResult::InvalidIndex;
	}
	auto replacement = configurationService_.active();
	auto &profiles = replacement.network.wifiProfiles;
	if (fromIndex < toIndex)
	{
		std::rotate(profiles.begin() + fromIndex, profiles.begin() + fromIndex + 1, profiles.begin() + toIndex + 1);
	}
	else if (fromIndex > toIndex)
	{
		std::rotate(profiles.begin() + toIndex, profiles.begin() + fromIndex, profiles.begin() + fromIndex + 1);
	}
	return commit(replacement, expectedGeneration);
}

WifiManagementResult WifiManagementService::updateRecoveryAp(
	const domain::RecoveryApConfiguration &configuration,
	const std::uint32_t expectedGeneration) noexcept
{
	auto replacement = configurationService_.active();
	replacement.network.recoveryAp = configuration;
	return commit(replacement, expectedGeneration);
}

WifiManagementResult WifiManagementService::commit(domain::Configuration replacement,
	const std::uint32_t expectedGeneration) noexcept
{
	if (configurationService_.active().generation != expectedGeneration)
	{
		return WifiManagementResult::GenerationConflict;
	}
	if (configurationService_.stage(replacement) != ConfigurationStageResult::Staged)
	{
		return WifiManagementResult::InvalidConfiguration;
	}
	const auto commitResult = configurationService_.commit();
	if (commitResult == ConfigurationCommitResult::PersistenceFailure)
	{
		configurationService_.discardStaged();
		return WifiManagementResult::PersistenceFailure;
	}
	return WifiManagementResult::Applied;
}
}