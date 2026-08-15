#pragma once

#include "configuration_source.h"

#include <cstdint>

namespace switch_actuator::ports
{
enum class ConfigurationFileStoreResult : std::uint8_t
{
	Stored,
	InvalidConfiguration,
	Unavailable,
	IoFailure
};

using ConfigurationFileStoreHandler = ConfigurationFileStoreResult (*)(void *context,
	const domain::Configuration &configuration) noexcept;

class ConfigurationFilePort final
{
public:
	constexpr ConfigurationFilePort() noexcept = default;

	constexpr ConfigurationFilePort(const ConfigurationSource source,
		const ConfigurationFileStoreHandler storeHandler,
		void *const context) noexcept
		: source_{source}, storeHandler_{storeHandler}, context_{context}
	{
	}

	[[nodiscard]] ConfigurationSourceResult load(domain::Configuration &configuration) const noexcept
	{
		return source_.load(configuration);
	}

	[[nodiscard]] ConfigurationFileStoreResult store(const domain::Configuration &configuration) const noexcept
	{
		return storeHandler_ != nullptr ? storeHandler_(context_, configuration)
									 : ConfigurationFileStoreResult::Unavailable;
	}

	[[nodiscard]] constexpr bool isValid() const noexcept
	{
		return source_.isValid() && storeHandler_ != nullptr;
	}

private:
	ConfigurationSource source_{};
	ConfigurationFileStoreHandler storeHandler_{nullptr};
	void *context_{nullptr};
};
}