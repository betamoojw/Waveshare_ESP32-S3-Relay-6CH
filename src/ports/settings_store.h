#pragma once

#include "../domain/configuration.h"

#include <cstdint>

namespace switch_actuator::ports
{
enum class SettingsLoadResult : std::uint8_t
{
	Loaded,
	NotFound,
	Corrupt,
	IoFailure
};

enum class SettingsSaveResult : std::uint8_t
{
	Saved,
	IoFailure,
	VerificationFailure
};

enum class SettingsEraseResult : std::uint8_t
{
	Erased,
	IoFailure,
	VerificationFailure
};

using SettingsLoadHandler = SettingsLoadResult (*)(void *context, domain::Configuration &configuration) noexcept;
using SettingsSaveHandler = SettingsSaveResult (*)(void *context, const domain::Configuration &configuration) noexcept;
using SettingsEraseHandler = SettingsEraseResult (*)(void *context) noexcept;

class SettingsStore final
{
public:
	constexpr SettingsStore() noexcept = default;

	constexpr SettingsStore(SettingsLoadHandler loadHandler,
							SettingsSaveHandler saveHandler,
							SettingsEraseHandler eraseHandler,
							void *context = nullptr) noexcept
		: loadHandler_{loadHandler}, saveHandler_{saveHandler}, eraseHandler_{eraseHandler}, context_{context}
	{
	}

	[[nodiscard]] SettingsLoadResult load(domain::Configuration &configuration) const noexcept
	{
		return loadHandler_ != nullptr ? loadHandler_(context_, configuration) : SettingsLoadResult::IoFailure;
	}

	[[nodiscard]] SettingsSaveResult save(const domain::Configuration &configuration) const noexcept
	{
		return saveHandler_ != nullptr ? saveHandler_(context_, configuration) : SettingsSaveResult::IoFailure;
	}

	[[nodiscard]] SettingsEraseResult erase() const noexcept
	{
		return eraseHandler_ != nullptr ? eraseHandler_(context_) : SettingsEraseResult::IoFailure;
	}

	[[nodiscard]] constexpr bool isValid() const noexcept
	{
		return loadHandler_ != nullptr && saveHandler_ != nullptr && eraseHandler_ != nullptr;
	}

private:
	SettingsLoadHandler loadHandler_{nullptr};
	SettingsSaveHandler saveHandler_{nullptr};
	SettingsEraseHandler eraseHandler_{nullptr};
	void *context_{nullptr};
};
}