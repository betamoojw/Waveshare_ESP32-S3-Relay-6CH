#pragma once

#include "../../ports/configuration_source.h"

#include <string_view>

namespace switch_actuator::adapters::configuration
{
class JsonConfigurationSource final
{
public:
	explicit constexpr JsonConfigurationSource(const std::string_view json) noexcept
		: json_{json}
	{
	}

	[[nodiscard]] ports::ConfigurationSource port() noexcept;

private:
	[[nodiscard]] static ports::ConfigurationSourceResult loadCallback(void *context,
		domain::Configuration &configuration) noexcept;
	[[nodiscard]] ports::ConfigurationSourceResult load(domain::Configuration &configuration) const noexcept;

	std::string_view json_;
};

[[nodiscard]] std::string_view embeddedDefaultConfigurationJson() noexcept;
}