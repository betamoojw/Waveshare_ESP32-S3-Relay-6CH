#pragma once

#include "../domain/configuration.h"

#include <cstdint>

namespace switch_actuator::ports
{
enum class ConfigurationSourceResult : std::uint8_t
{
	Loaded,
	Invalid,
	Unavailable
};

using ConfigurationSourceLoadHandler = ConfigurationSourceResult (*)(void *context,
	 domain::Configuration &configuration) noexcept;

class ConfigurationSource final
{
public:
	constexpr ConfigurationSource() noexcept = default;

	constexpr ConfigurationSource(ConfigurationSourceLoadHandler loadHandler, void *context = nullptr) noexcept
		: loadHandler_{loadHandler}, context_{context}
	{
	}

	[[nodiscard]] ConfigurationSourceResult load(domain::Configuration &configuration) const noexcept
	{
		return loadHandler_ != nullptr ? loadHandler_(context_, configuration) : ConfigurationSourceResult::Unavailable;
	}

	[[nodiscard]] constexpr bool isValid() const noexcept
	{
		return loadHandler_ != nullptr;
	}

private:
	ConfigurationSourceLoadHandler loadHandler_{nullptr};
	void *context_{nullptr};
};
}