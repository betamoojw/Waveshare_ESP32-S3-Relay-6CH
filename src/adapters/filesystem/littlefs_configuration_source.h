#pragma once

#include "../configuration/json_configuration_source.h"
#include "../../ports/configuration_file_port.h"

#include <ArduinoJson.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace switch_actuator::adapters::filesystem
{
enum class LittleFsInitializeResult : std::uint8_t
{
	Initialized,
	MountFailure
};

class LittleFsConfigurationSource final
{
public:
	explicit LittleFsConfigurationSource(std::string_view embeddedFallback) noexcept;

	[[nodiscard]] LittleFsInitializeResult initialize() noexcept;
	[[nodiscard]] ports::ConfigurationSource port() noexcept;
	[[nodiscard]] ports::ConfigurationFilePort filePort() noexcept;

private:
	static constexpr std::size_t maximumConfigurationBytes{8192};

	[[nodiscard]] static ports::ConfigurationSourceResult loadCallback(void *context,
		domain::Configuration &configuration) noexcept;
	[[nodiscard]] static ports::ConfigurationSourceResult loadFileCallback(void *context,
		domain::Configuration &configuration) noexcept;
	[[nodiscard]] static ports::ConfigurationFileStoreResult storeCallback(void *context,
		const domain::Configuration &configuration) noexcept;
	[[nodiscard]] ports::ConfigurationSourceResult load(domain::Configuration &configuration) noexcept;
	[[nodiscard]] ports::ConfigurationSourceResult loadPrimary(domain::Configuration &configuration) noexcept;
	[[nodiscard]] ports::ConfigurationFileStoreResult store(const domain::Configuration &configuration) noexcept;
	[[nodiscard]] ports::ConfigurationSourceResult loadBundle(std::string_view directory,
		domain::Configuration &configuration) noexcept;
	[[nodiscard]] bool backupBundle() noexcept;
	[[nodiscard]] bool restoreBundle() noexcept;
	[[nodiscard]] bool copyFileAtomically(std::string_view sourcePath, std::string_view destinationPath) noexcept;
	[[nodiscard]] bool writeDocumentAtomically(std::string_view path, const JsonDocument &document) noexcept;

	configuration::JsonConfigurationSource embeddedFallback_;
	std::array<char, maximumConfigurationBytes> buffer_{};
	bool mounted_{false};
};
}