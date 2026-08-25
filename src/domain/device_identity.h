#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace switch_actuator::domain
{
inline constexpr std::size_t productIdCapacity{24};
inline constexpr std::size_t productNameCapacity{48};
inline constexpr std::size_t hardwareModelCapacity{48};
inline constexpr std::size_t hardwareRevisionIdentityCapacity{16};
inline constexpr std::size_t firmwareVersionCapacity{32};
inline constexpr std::size_t serialNumberCapacity{32};
inline constexpr std::size_t deviceIdentityUuidSize{16};
inline constexpr std::size_t macAddressSize{6};
inline constexpr std::size_t manufacturingDateCapacity{11};

struct ProductId final
{
	std::array<char, productIdCapacity> value{};
};

struct ProductName final
{
	std::array<char, productNameCapacity> value{};
};

struct HardwareModel final
{
	std::array<char, hardwareModelCapacity> value{};
};

struct HardwareRevision final
{
	std::array<char, hardwareRevisionIdentityCapacity> value{};
};

struct FirmwareVersion final
{
	std::array<char, firmwareVersionCapacity> value{};
};

struct SerialNumber final
{
	std::array<char, serialNumberCapacity> value{};
};

struct DeviceUuid final
{
	std::array<std::uint8_t, deviceIdentityUuidSize> bytes{};
};

struct MacAddress final
{
	std::array<std::uint8_t, macAddressSize> bytes{};
};

struct ManufacturingDate final
{
	std::array<char, manufacturingDateCapacity> iso8601{};
};

struct DeviceIdentity final
{
	ProductId productId{};
	ProductName productName{};
	HardwareModel hardwareModel{};
	HardwareRevision hardwareRevision{};
	FirmwareVersion firmwareVersion{};
	SerialNumber serialNumber{};
	DeviceUuid deviceUuid{};
	MacAddress macAddress{};
	ManufacturingDate manufacturingDate{};
	std::uint32_t manufacturingBatch{0};
};

struct DeviceIdentitySource final
{
	std::string_view productId;
	std::string_view productName;
	std::string_view hardwareModel;
	std::string_view hardwareRevision;
	std::string_view firmwareVersion;
	std::string_view serialNumber;
	std::array<std::uint8_t, deviceIdentityUuidSize> deviceUuid{};
	std::array<std::uint8_t, macAddressSize> macAddress{};
	std::string_view manufacturingDate;
	std::uint32_t manufacturingBatch{0};
};

[[nodiscard]] bool isValid(const DeviceIdentity &identity) noexcept;
[[nodiscard]] bool isValid(const ManufacturingDate &date) noexcept;
[[nodiscard]] bool isManufacturingIdentityProvisioned(const DeviceIdentity &identity) noexcept;
[[nodiscard]] std::optional<DeviceIdentity> makeDeviceIdentity(const DeviceIdentitySource &source) noexcept;
}