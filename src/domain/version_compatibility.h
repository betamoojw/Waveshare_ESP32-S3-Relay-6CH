#pragma once

#include <cstdint>
#include <string_view>

namespace switch_actuator::domain::compatibility
{
struct VersionContract final
{
	std::uint16_t major;
	std::string_view label;
};

#ifdef FIRMWARE_VERSION
inline constexpr std::string_view firmware{FIRMWARE_VERSION};
#else
inline constexpr std::string_view firmware{"FW-1.4.0+development"};
#endif

inline constexpr VersionContract configuration{4, "CFG-4"};
inline constexpr VersionContract api{1, "API-v1"};
inline constexpr VersionContract modbus{1, "MODBUS-v1"};
inline constexpr VersionContract knxApplication{1, "KNX-APP-v1"};
inline constexpr VersionContract filesystem{1, "FS-v1"};

[[nodiscard]] constexpr bool hasPrefix(const std::string_view value, const std::string_view prefix) noexcept
{
	return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

[[nodiscard]] constexpr std::uint16_t modbusFirmwareVersion(const std::string_view value) noexcept
{
	if (!hasPrefix(value, "FW-") || value.size() < 6U) return 0;
	std::size_t index{3};
	std::uint16_t major{0};
	while (index < value.size() && value[index] >= '0' && value[index] <= '9')
	{
		major = static_cast<std::uint16_t>(major * 10U + static_cast<std::uint16_t>(value[index] - '0'));
		++index;
	}
	if (index >= value.size() || value[index++] != '.') return 0;
	std::uint16_t minor{0};
	const auto minorStart = index;
	while (index < value.size() && value[index] >= '0' && value[index] <= '9')
	{
		minor = static_cast<std::uint16_t>(minor * 10U + static_cast<std::uint16_t>(value[index] - '0'));
		++index;
	}
	return index == minorStart || major > 655U || minor > 99U ? 0 :
		static_cast<std::uint16_t>(major * 100U + minor);
}

inline constexpr std::uint16_t firmwareModbusRegister{modbusFirmwareVersion(firmware)};

static_assert(hasPrefix(firmware, "FW-"), "Firmware version must use the FW-* compatibility format");
static_assert(firmwareModbusRegister != 0, "Firmware version must contain numeric major and minor components");
}