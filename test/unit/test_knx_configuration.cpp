#include "../../src/domain/configuration.h"
#include "../../src/app/switching_policy_service.h"
#include "../../src/app/relay_command_service.h"
#include "../../src/app/relay_timer_service.h"
#include "../../src/app/scene_service.h"

#include <unity.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdint>

namespace
{
using switch_actuator::domain::Configuration;
using switch_actuator::domain::ConfigurationValidationError;
using switch_actuator::app::RelayCommandBatch;
using switch_actuator::app::RelayCommandQueue;
using switch_actuator::app::CommandArbiter;
using switch_actuator::app::RelayCommandService;
using switch_actuator::app::RelayServiceInitializeResult;
using switch_actuator::app::RelayTimerScheduleResult;
using switch_actuator::app::RelayTimerService;
using switch_actuator::app::SceneConfigureResult;
using switch_actuator::app::SceneDefinition;
using switch_actuator::app::SceneOperationResult;
using switch_actuator::app::SceneService;
using switch_actuator::app::SwitchingPolicyResult;
using switch_actuator::app::SwitchingPolicyService;

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

struct RelayFixture final
{
	std::array<switch_actuator::domain::RelayState, switch_actuator::domain::relayChannelCount> states{};
};

switch_actuator::ports::RelayOutputResult applyRelay(void *const context,
	const switch_actuator::domain::RelayChannelId channel,
	const switch_actuator::domain::RelayState state) noexcept
{
	static_cast<RelayFixture *>(context)->states[channel.value] = state;
	return switch_actuator::ports::RelayOutputResult::Applied;
}

bool acceptEvent(void *, const switch_actuator::domain::RelayStateChanged &) noexcept
{
	return true;
}

bool persistScene(const SceneDefinition &, void *const context) noexcept
{
	return *static_cast<bool *>(context);
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

void testEnqueuesTypedChannelCommand()
{
	RelayCommandQueue queue{};
	CommandArbiter arbiter{};
	SwitchingPolicyService service{queue, arbiter};
	const auto result = service.requestChannel({2},
		switch_actuator::domain::RelayAction::SetOn,
		switch_actuator::domain::CommandSource::Knx,
		42,
		1000);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(SwitchingPolicyResult::Accepted),
		static_cast<std::uint8_t>(result));

	RelayCommandBatch batch{};
	TEST_ASSERT_TRUE(queue.dequeue(batch));
	TEST_ASSERT_EQUAL_UINT32(1, batch.count);
	TEST_ASSERT_EQUAL_UINT8(2, batch.commands[0].channel.value);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::domain::RelayAction::SetOn),
		static_cast<std::uint8_t>(batch.commands[0].action));
	TEST_ASSERT_EQUAL_UINT32(42, batch.commands[0].correlationId);
	TEST_ASSERT_EQUAL_UINT32(1000, batch.commands[0].receivedAtMs);
}

void testEnqueuesAtomicParticipantGroup()
{
	RelayCommandQueue queue{};
	CommandArbiter arbiter{};
	SwitchingPolicyService service{queue, arbiter};
	std::array<bool, switch_actuator::domain::relayChannelCount> participants{};
	participants[0] = true;
	participants[3] = true;
	participants[5] = true;
	const auto result = service.requestGroup(participants,
		switch_actuator::domain::RelayAction::SetOff,
		switch_actuator::domain::CommandSource::Knx,
		77,
		2000);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(SwitchingPolicyResult::Accepted),
		static_cast<std::uint8_t>(result));

	RelayCommandBatch batch{};
	TEST_ASSERT_TRUE(queue.dequeue(batch));
	TEST_ASSERT_EQUAL_UINT32(3, batch.count);
	TEST_ASSERT_EQUAL_UINT8(0, batch.commands[0].channel.value);
	TEST_ASSERT_EQUAL_UINT8(3, batch.commands[1].channel.value);
	TEST_ASSERT_EQUAL_UINT8(5, batch.commands[2].channel.value);
	TEST_ASSERT_TRUE(queue.empty());
}

void testGroupWithoutParticipantsDoesNotQueue()
{
	RelayCommandQueue queue{};
	CommandArbiter arbiter{};
	SwitchingPolicyService service{queue, arbiter};
	const std::array<bool, switch_actuator::domain::relayChannelCount> participants{};
	const auto result = service.requestGroup(participants,
		switch_actuator::domain::RelayAction::SetOff,
		switch_actuator::domain::CommandSource::Knx,
		1,
		1);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(SwitchingPolicyResult::NoParticipants),
		static_cast<std::uint8_t>(result));
	TEST_ASSERT_TRUE(queue.empty());
}

void testRejectsInvalidChannelBeforeQueueing()
{
	RelayCommandQueue queue{};
	CommandArbiter arbiter{};
	SwitchingPolicyService service{queue, arbiter};
	const auto result = service.requestChannel({switch_actuator::domain::relayChannelCount},
		switch_actuator::domain::RelayAction::SetOn,
		switch_actuator::domain::CommandSource::Knx,
		1,
		1);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(SwitchingPolicyResult::InvalidChannel),
		static_cast<std::uint8_t>(result));
	TEST_ASSERT_TRUE(queue.empty());
}

void testPolicyRejectsLockedChannelBeforeQueueing()
{
	RelayCommandQueue queue{};
	CommandArbiter arbiter{};
	TEST_ASSERT_TRUE(arbiter.setSafetyLockout({1}, true));
	SwitchingPolicyService service{queue, arbiter};
	const auto result = service.requestChannel({1},
		switch_actuator::domain::RelayAction::SetOn,
		switch_actuator::domain::CommandSource::Knx,
		1,
		1);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(SwitchingPolicyResult::SafetyLockout),
		static_cast<std::uint8_t>(result));
	TEST_ASSERT_TRUE(queue.empty());
}

void testSceneRecallQueuesMixedAtomicStates()
{
	RelayFixture fixture{};
	CommandArbiter arbiter{};
	RelayCommandService relayService{{applyRelay, &fixture}, {acceptEvent}, arbiter};
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(RelayServiceInitializeResult::Initialized),
		static_cast<std::uint8_t>(relayService.initialize(0)));
	RelayCommandQueue queue{};
	SwitchingPolicyService policy{queue, arbiter};
	SceneService scenes{policy, relayService};
	SceneDefinition scene{};
	scene.number = 1;
	scene.participants[0] = true;
	scene.participants[2] = true;
	scene.targetStates[0] = switch_actuator::domain::RelayState::On;
	scene.targetStates[2] = switch_actuator::domain::RelayState::Off;
	bool persistenceSucceeds{true};
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(SceneConfigureResult::Configured),
		static_cast<std::uint8_t>(scenes.configure(&scene, 1, {persistScene, &persistenceSucceeds})));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(SceneOperationResult::Accepted),
		static_cast<std::uint8_t>(scenes.recall(1, switch_actuator::domain::CommandSource::Knx, 7, 100)));

	RelayCommandBatch batch{};
	TEST_ASSERT_TRUE(queue.dequeue(batch));
	TEST_ASSERT_EQUAL_UINT32(2, batch.count);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::domain::RelayAction::SetOn),
		static_cast<std::uint8_t>(batch.commands[0].action));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::domain::RelayAction::SetOff),
		static_cast<std::uint8_t>(batch.commands[1].action));
}

void testSceneLearningIsFailureAtomic()
{
	RelayFixture fixture{};
	CommandArbiter arbiter{};
	RelayCommandService relayService{{applyRelay, &fixture}, {acceptEvent}, arbiter};
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(RelayServiceInitializeResult::Initialized),
		static_cast<std::uint8_t>(relayService.initialize(0)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::app::RelayCommandStatus::Accepted),
		static_cast<std::uint8_t>(relayService.execute({{0},
			switch_actuator::domain::RelayAction::SetOn,
			switch_actuator::domain::CommandSource::Cli,
			1,
			1}).status));
	RelayCommandQueue queue{};
	SwitchingPolicyService policy{queue, arbiter};
	SceneService scenes{policy, relayService};
	SceneDefinition scene{};
	scene.number = 1;
	scene.participants[0] = true;
	bool persistenceSucceeds{false};
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(SceneConfigureResult::Configured),
		static_cast<std::uint8_t>(scenes.configure(&scene, 1, {persistScene, &persistenceSucceeds})));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(SceneOperationResult::PersistenceFailure),
		static_cast<std::uint8_t>(scenes.learn(1)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(SceneOperationResult::Accepted),
		static_cast<std::uint8_t>(scenes.recall(1, switch_actuator::domain::CommandSource::Cli, 2, 2)));
	RelayCommandBatch batch{};
	TEST_ASSERT_TRUE(queue.dequeue(batch));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::domain::RelayAction::SetOff),
		static_cast<std::uint8_t>(batch.commands[0].action));
}

void testTimerReplacementAndWrapAroundExpiry()
{
	RelayCommandQueue queue{};
	CommandArbiter arbiter{};
	SwitchingPolicyService policy{queue, arbiter};
	RelayTimerService timers{policy};
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(RelayTimerScheduleResult::Scheduled),
		static_cast<std::uint8_t>(timers.schedule({3},
			switch_actuator::domain::RelayAction::SetOn,
			switch_actuator::domain::CommandSource::Knx,
			10,
			0xFFFF'FFF0U,
			32)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(RelayTimerScheduleResult::Replaced),
		static_cast<std::uint8_t>(timers.schedule({3},
			switch_actuator::domain::RelayAction::SetOff,
			switch_actuator::domain::CommandSource::Knx,
			11,
			0xFFFF'FFF0U,
			32)));
	TEST_ASSERT_EQUAL_UINT32(0, timers.update(15).submitted);
	TEST_ASSERT_EQUAL_UINT32(1, timers.update(16).submitted);
	RelayCommandBatch batch{};
	TEST_ASSERT_TRUE(queue.dequeue(batch));
	TEST_ASSERT_EQUAL_UINT32(11, batch.commands[0].correlationId);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::domain::RelayAction::SetOff),
		static_cast<std::uint8_t>(batch.commands[0].action));
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
	RUN_TEST(testEnqueuesTypedChannelCommand);
	RUN_TEST(testEnqueuesAtomicParticipantGroup);
	RUN_TEST(testGroupWithoutParticipantsDoesNotQueue);
	RUN_TEST(testRejectsInvalidChannelBeforeQueueing);
	RUN_TEST(testPolicyRejectsLockedChannelBeforeQueueing);
	RUN_TEST(testSceneRecallQueuesMixedAtomicStates);
	RUN_TEST(testSceneLearningIsFailureAtomic);
	RUN_TEST(testTimerReplacementAndWrapAroundExpiry);
	return UNITY_END();
}