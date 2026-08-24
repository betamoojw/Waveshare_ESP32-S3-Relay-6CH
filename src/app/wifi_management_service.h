#pragma once

#include "configuration_service.h"

#include <array>
#include <cstdint>

namespace switch_actuator::app
{
enum class WifiSecretUpdate : std::uint8_t
{
	Preserve,
	Replace,
	Clear
};

enum class WifiManagementResult : std::uint8_t
{
	Applied,
	InvalidIndex,
	GenerationConflict,
	InvalidConfiguration,
	PersistenceFailure
};

struct WifiProfilePatch final
{
	std::uint8_t index{0};
	bool enabled{false};
	std::array<char, domain::wifiSsidCapacity> ssid{};
	WifiSecretUpdate passphraseUpdate{WifiSecretUpdate::Preserve};
	std::array<char, domain::wifiPassphraseCapacity> passphrase{};
	domain::Ipv4Configuration ipv4{};
	std::uint32_t expectedGeneration{0};
};

struct WifiProfileSummary final
{
	std::uint8_t index{0};
	bool enabled{false};
	std::array<char, domain::wifiSsidCapacity> ssid{};
	bool hasPassphrase{false};
	domain::Ipv4Configuration ipv4{};
};

struct WifiManagementSnapshot final
{
	std::uint32_t generation{0};
	std::array<WifiProfileSummary, domain::wifiProfileCount> profiles{};
	domain::RecoveryApConfiguration recoveryAp{};
};

class WifiManagementService final
{
public:
	explicit WifiManagementService(ConfigurationService &configurationService) noexcept;

	[[nodiscard]] WifiManagementSnapshot snapshot() const noexcept;
	[[nodiscard]] WifiManagementResult saveProfile(const WifiProfilePatch &patch) noexcept;
	[[nodiscard]] WifiManagementResult removeProfile(std::uint8_t index, std::uint32_t expectedGeneration) noexcept;
	[[nodiscard]] WifiManagementResult moveProfile(std::uint8_t fromIndex,
		std::uint8_t toIndex,
		std::uint32_t expectedGeneration) noexcept;
	[[nodiscard]] WifiManagementResult updateRecoveryAp(const domain::RecoveryApConfiguration &configuration,
		std::uint32_t expectedGeneration) noexcept;

private:
	[[nodiscard]] WifiManagementResult commit(domain::Configuration replacement,
		std::uint32_t expectedGeneration) noexcept;

	ConfigurationService &configurationService_;
};
}