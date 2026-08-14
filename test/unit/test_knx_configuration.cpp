#include "../../src/domain/configuration.h"

#include <unity.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdint>

namespace
{
using switch_actuator::domain::Configuration;
using switch_actuator::domain::ConfigurationValidationError;

template <std::size_t Size>
void setText(std::array<char, Size> &destination, const char *const text)
{
	destination.fill('\0');
	std::copy(text, text + std::strlen(text), destination.begin());
}

Configuration validConfiguration()
{
	Configuration configuration{};
	setText(configuration.boardModel, "Host-Test-Board");
	setText(configuration.hardwareRevision, "1.0");
	setText(configuration.deviceSerial, "TEST-0001");
	configuration.deviceUuid[0] = 1;
	return configuration;
}

void testValidKnxDefaults()
{
	const auto configuration = validConfiguration();
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(ConfigurationValidationError::None),
		static_cast<std::uint8_t>(switch_actuator::domain::validateConfiguration(configuration)));
}

void testRejectsInvalidPublicationIntervals()
{
	auto configuration = validConfiguration();
	configuration.knx.minimumTelegramIntervalMs = 19;
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(ConfigurationValidationError::InvalidKnxConfiguration),
		static_cast<std::uint8_t>(switch_actuator::domain::validateConfiguration(configuration)));

	configuration = validConfiguration();
	configuration.knx.cyclicStatusIntervalMs = 9999;
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(ConfigurationValidationError::InvalidKnxConfiguration),
		static_cast<std::uint8_t>(switch_actuator::domain::validateConfiguration(configuration)));
}

void testRejectsHeartbeatWithoutAddress()
{
	auto configuration = validConfiguration();
	configuration.knx.heartbeatIntervalMs = 10'000;
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(ConfigurationValidationError::InvalidKnxConfiguration),
		static_cast<std::uint8_t>(switch_actuator::domain::validateConfiguration(configuration)));
}

void testRejectsAmbiguousCommandAddresses()
{
	auto configuration = validConfiguration();
	configuration.knx.channels[0].switchGroupAddress = 0x0801;
	configuration.knx.centralSwitchGroupAddress = 0x0801;
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(ConfigurationValidationError::InvalidKnxConfiguration),
		static_cast<std::uint8_t>(switch_actuator::domain::validateConfiguration(configuration)));
}

void testRejectsOutputAddressCollidingWithCommand()
{
	auto configuration = validConfiguration();
	configuration.knx.channels[0].switchGroupAddress = 0x0801;
	configuration.knx.channels[1].statusGroupAddress = 0x0801;
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(ConfigurationValidationError::InvalidKnxConfiguration),
		static_cast<std::uint8_t>(switch_actuator::domain::validateConfiguration(configuration)));
}
}

int main()
{
	UNITY_BEGIN();
	RUN_TEST(testValidKnxDefaults);
	RUN_TEST(testRejectsInvalidPublicationIntervals);
	RUN_TEST(testRejectsHeartbeatWithoutAddress);
	RUN_TEST(testRejectsAmbiguousCommandAddresses);
	RUN_TEST(testRejectsOutputAddressCollidingWithCommand);
	return UNITY_END();
}