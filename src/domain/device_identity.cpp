#include "device_identity.h"

#include <algorithm>

namespace switch_actuator::domain
{
namespace
{
template <std::size_t Capacity>
[[nodiscard]] bool copyText(const std::string_view source, std::array<char, Capacity> &destination) noexcept
{
	if (source.empty() || source.size() >= destination.size())
	{
		return false;
	}
	destination.fill('\0');
	std::copy(source.begin(), source.end(), destination.begin());
	return true;
}

template <std::size_t Capacity>
[[nodiscard]] bool hasBoundedText(const std::array<char, Capacity> &text) noexcept
{
	return text.front() != '\0' && std::find(text.begin(), text.end(), '\0') != text.end();
}

[[nodiscard]] bool isLeapYear(const std::uint16_t year) noexcept
{
	return year % 4U == 0U && (year % 100U != 0U || year % 400U == 0U);
}

[[nodiscard]] bool isValidManufacturingDate(const ManufacturingDate &date) noexcept
{
	const auto &text = date.iso8601;
	if (text[4] != '-' || text[7] != '-' || text[10] != '\0')
	{
		return false;
	}
	constexpr std::array<std::size_t, 8> digitPositions{0, 1, 2, 3, 5, 6, 8, 9};
	if (std::any_of(digitPositions.begin(), digitPositions.end(), [&text](const auto index) {
			return text[index] < '0' || text[index] > '9';
		}))
	{
		return false;
	}
	const auto decimal = [&text](const std::size_t index) {
		return static_cast<std::uint8_t>((text[index] - '0') * 10 + text[index + 1] - '0');
	};
	const auto year = static_cast<std::uint16_t>((text[0] - '0') * 1000 + (text[1] - '0') * 100 +
		(text[2] - '0') * 10 + text[3] - '0');
	const auto month = decimal(5);
	const auto day = decimal(8);
	if (year == 0 || month == 0 || month > 12)
	{
		return false;
	}
	constexpr std::array<std::uint8_t, 12> daysPerMonth{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	const auto maximumDay = static_cast<std::uint8_t>(daysPerMonth[month - 1] + (month == 2 && isLeapYear(year) ? 1 : 0));
	return day != 0 && day <= maximumDay;
}
}

bool isValid(const ManufacturingDate &date) noexcept
{
	return isValidManufacturingDate(date);
}

bool isValid(const DeviceIdentity &identity) noexcept
{
	const auto manufacturingUnprovisioned = identity.manufacturingDate.iso8601.front() == '\0' &&
		identity.manufacturingBatch == 0;
	return hasBoundedText(identity.productId.value) &&
		hasBoundedText(identity.productName.value) &&
		hasBoundedText(identity.hardwareModel.value) &&
		hasBoundedText(identity.hardwareRevision.value) &&
		hasBoundedText(identity.firmwareVersion.value) &&
		hasBoundedText(identity.serialNumber.value) &&
		std::any_of(identity.deviceUuid.bytes.begin(), identity.deviceUuid.bytes.end(), [](const auto byte) { return byte != 0; }) &&
		std::any_of(identity.macAddress.bytes.begin(), identity.macAddress.bytes.end(), [](const auto byte) { return byte != 0; }) &&
		(manufacturingUnprovisioned || (isValid(identity.manufacturingDate) && identity.manufacturingBatch != 0));
}

bool isManufacturingIdentityProvisioned(const DeviceIdentity &identity) noexcept
{
	return isValid(identity) && identity.manufacturingBatch != 0;
}

std::optional<DeviceIdentity> makeDeviceIdentity(const DeviceIdentitySource &source) noexcept
{
	DeviceIdentity identity{};
	if (!copyText(source.productId, identity.productId.value) ||
		!copyText(source.productName, identity.productName.value) ||
		!copyText(source.hardwareModel, identity.hardwareModel.value) ||
		!copyText(source.hardwareRevision, identity.hardwareRevision.value) ||
		!copyText(source.firmwareVersion, identity.firmwareVersion.value) ||
		!copyText(source.serialNumber, identity.serialNumber.value) ||
		(!source.manufacturingDate.empty() && !copyText(source.manufacturingDate, identity.manufacturingDate.iso8601)))
	{
		return std::nullopt;
	}
	identity.deviceUuid.bytes = source.deviceUuid;
	identity.macAddress.bytes = source.macAddress;
	identity.manufacturingBatch = source.manufacturingBatch;
	return isValid(identity) ? std::optional<DeviceIdentity>{identity} : std::nullopt;
}
}