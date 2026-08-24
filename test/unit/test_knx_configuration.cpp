#include "../../src/domain/configuration.h"
#include "../../src/app/configuration_service.h"
#include "../../src/app/wifi_management_service.h"
#include "../../src/app/web_event_journal.h"
#include "../../src/app/web_security_service.h"
#include "../../src/app/web_command_tracker.h"
#include "../../src/app/web_request_queue.h"
#include "../../src/app/switching_policy_service.h"
#include "../../src/app/relay_command_service.h"
#include "../../src/app/relay_timer_service.h"
#include "../../src/app/scene_service.h"

#include <unity.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdint>
#include <cstdio>

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
using switch_actuator::app::WifiManagementResult;
using switch_actuator::app::WifiManagementService;
using switch_actuator::app::WifiProfilePatch;
using switch_actuator::app::WifiSecretUpdate;
using switch_actuator::app::WebEvent;
using switch_actuator::app::WebEventJournal;
using switch_actuator::app::WebEventReadResult;
using switch_actuator::app::WebEventType;
using switch_actuator::app::WebSecurityService;
using switch_actuator::app::WebSessionCreated;
using switch_actuator::app::WebSessionResult;
using switch_actuator::app::WebUserManagementResult;

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

struct SettingsFixture final
{
	Configuration stored{validConfiguration()};
	bool saveSucceeds{true};
};

switch_actuator::ports::SettingsLoadResult loadSettings(void *const context, Configuration &configuration) noexcept
{
	configuration = static_cast<SettingsFixture *>(context)->stored;
	return switch_actuator::ports::SettingsLoadResult::Loaded;
}

switch_actuator::ports::SettingsSaveResult saveSettings(void *const context,
	const Configuration &configuration) noexcept
{
	auto &fixture = *static_cast<SettingsFixture *>(context);
	if (!fixture.saveSucceeds)
	{
		return switch_actuator::ports::SettingsSaveResult::IoFailure;
	}
	fixture.stored = configuration;
	return switch_actuator::ports::SettingsSaveResult::Saved;
}

switch_actuator::ports::SettingsEraseResult eraseSettings(void *) noexcept
{
	return switch_actuator::ports::SettingsEraseResult::Erased;
}

switch_actuator::app::ConfigurationService configurationService(SettingsFixture &fixture)
{
	switch_actuator::app::ConfigurationService service{{loadSettings, saveSettings, eraseSettings, &fixture}};
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::app::ConfigurationInitializeResult::Loaded),
		static_cast<std::uint8_t>(service.initialize()));
	return service;
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

void testRejectsDuplicateWifiProfiles()
{
	auto configuration = validConfiguration();
	for (std::size_t index = 0; index < 2; ++index)
	{
		auto &profile = configuration.network.wifiProfiles[index];
		profile.enabled = true;
		setText(profile.ssid, "Workshop");
		setText(profile.passphrase, "commissioning-key");
	}
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(ConfigurationValidationError::InvalidNetworkConfiguration),
		static_cast<std::uint8_t>(switch_actuator::domain::validateConfiguration(configuration)));
}

void testRejectsIncompleteStaticWifiProfile()
{
	auto configuration = validConfiguration();
	auto &profile = configuration.network.wifiProfiles[0];
	profile.enabled = true;
	setText(profile.ssid, "Workshop");
	setText(profile.passphrase, "commissioning-key");
	profile.ipv4.mode = switch_actuator::domain::IpMode::Static;
	profile.ipv4.address = {192, 168, 1, 20};
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(ConfigurationValidationError::InvalidNetworkConfiguration),
		static_cast<std::uint8_t>(switch_actuator::domain::validateConfiguration(configuration)));
}

void testWifiProfileUpdatePreservesStoredSecret()
{
	SettingsFixture fixture{};
	auto &storedProfile = fixture.stored.network.wifiProfiles[0];
	storedProfile.enabled = true;
	setText(storedProfile.ssid, "Workshop");
	setText(storedProfile.passphrase, "commissioning-key");
	auto configuration = configurationService(fixture);
	WifiManagementService wifi{configuration};
	WifiProfilePatch patch{};
	patch.index = 0;
	patch.enabled = true;
	patch.expectedGeneration = configuration.active().generation;
	setText(patch.ssid, "Production");
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WifiManagementResult::Applied),
		static_cast<std::uint8_t>(wifi.saveProfile(patch)));
	TEST_ASSERT_EQUAL_STRING("commissioning-key", configuration.active().network.wifiProfiles[0].passphrase.data());
	TEST_ASSERT_TRUE(wifi.snapshot().profiles[0].hasPassphrase);
}

void testRejectsStaticWifiGatewayOutsideSubnet()
{
	auto configuration = validConfiguration();
	auto &profile = configuration.network.wifiProfiles[0];
	profile.enabled = true;
	setText(profile.ssid, "Workshop");
	setText(profile.passphrase, "commissioning-key");
	profile.ipv4.mode = switch_actuator::domain::IpMode::Static;
	profile.ipv4.address = {192, 168, 1, 20};
	profile.ipv4.subnetMask = {255, 255, 255, 0};
	profile.ipv4.gateway = {192, 168, 2, 1};
	profile.ipv4.dns = {192, 168, 1, 1};
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(ConfigurationValidationError::InvalidNetworkConfiguration),
		static_cast<std::uint8_t>(switch_actuator::domain::validateConfiguration(configuration)));
}

void testWifiManagementRejectsStaleGeneration()
{
	SettingsFixture fixture{};
	auto configuration = configurationService(fixture);
	WifiManagementService wifi{configuration};
	WifiProfilePatch patch{};
	patch.index = 0;
	patch.expectedGeneration = configuration.active().generation + 1;
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WifiManagementResult::GenerationConflict),
		static_cast<std::uint8_t>(wifi.saveProfile(patch)));
	TEST_ASSERT_EQUAL_UINT32(fixture.stored.generation, configuration.active().generation);
}

void testWifiProfileRemovalCompactsPriorityOrder()
{
	SettingsFixture fixture{};
	for (auto &profile : fixture.stored.network.wifiProfiles)
	{
		profile.enabled = true;
		setText(profile.passphrase, "commissioning-key");
	}
	setText(fixture.stored.network.wifiProfiles[0].ssid, "First");
	setText(fixture.stored.network.wifiProfiles[1].ssid, "Second");
	setText(fixture.stored.network.wifiProfiles[2].ssid, "Third");
	auto configuration = configurationService(fixture);
	WifiManagementService wifi{configuration};
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WifiManagementResult::Applied),
		static_cast<std::uint8_t>(wifi.removeProfile(1, configuration.active().generation)));
	TEST_ASSERT_EQUAL_STRING("Third", configuration.active().network.wifiProfiles[1].ssid.data());
	TEST_ASSERT_FALSE(configuration.active().network.wifiProfiles[2].enabled);
}

void testWifiPersistenceFailureLeavesActiveConfigurationUnchanged()
{
	SettingsFixture fixture{};
	auto configuration = configurationService(fixture);
	WifiManagementService wifi{configuration};
	fixture.saveSucceeds = false;
	WifiProfilePatch patch{};
	patch.index = 0;
	patch.enabled = true;
	patch.passphraseUpdate = WifiSecretUpdate::Replace;
	patch.expectedGeneration = configuration.active().generation;
	setText(patch.ssid, "Workshop");
	setText(patch.passphrase, "commissioning-key");
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WifiManagementResult::PersistenceFailure),
		static_cast<std::uint8_t>(wifi.saveProfile(patch)));
	TEST_ASSERT_FALSE(configuration.active().network.wifiProfiles[0].enabled);
	TEST_ASSERT_FALSE(configuration.hasStagedConfiguration());
}

void testRejectsModbusSevenDataBits()
{
	auto configuration = validConfiguration();
	configuration.modbus.dataBits = 7;
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(ConfigurationValidationError::InvalidSerialFormat),
		static_cast<std::uint8_t>(switch_actuator::domain::validateConfiguration(configuration)));
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

void testWebEventJournalReportsOverwrittenSequenceGap()
{
	WebEventJournal journal{};
	for (std::size_t index = 0; index < WebEventJournal::capacity + 1; ++index)
	{
		journal.publish({0,
			WebEventType::RelayCommandCompleted,
			static_cast<std::uint32_t>(index + 1),
			{0},
			switch_actuator::domain::RelayState::Off,
			switch_actuator::app::RelayCommandStatus::Accepted,
			switch_actuator::app::RelayCommandReason::None,
			static_cast<std::uint32_t>(index + 1),
			100});
	}
	WebEvent event{};
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebEventReadResult::Gap),
		static_cast<std::uint8_t>(journal.read(1, event)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebEventReadResult::Available),
		static_cast<std::uint8_t>(journal.read(2, event)));
	TEST_ASSERT_EQUAL_UINT32(2, event.sequence);
	TEST_ASSERT_EQUAL_UINT32(2, event.correlationId);
}

struct WebSecurityFixture final
{
	switch_actuator::ports::WebSecurityRecord record{};
	std::uint32_t nowMs{1000};
	std::uint8_t randomValue{1};
	std::uint32_t passwordVerifications{0};
	bool saveSucceeds{true};
};

switch_actuator::ports::WebSecurityStoreResult loadWebSecurity(void *const context,
	switch_actuator::ports::WebSecurityRecord &record) noexcept
{
	const auto &stored = static_cast<WebSecurityFixture *>(context)->record;
	if (std::none_of(stored.signingKey.begin(), stored.signingKey.end(), [](const auto value) { return value != 0; }))
		return switch_actuator::ports::WebSecurityStoreResult::NotFound;
	record = stored;
	return switch_actuator::ports::WebSecurityStoreResult::Applied;
}

switch_actuator::ports::WebSecurityStoreResult saveWebSecurity(void *const context,
	const switch_actuator::ports::WebSecurityRecord &record) noexcept
{
	auto &fixture = *static_cast<WebSecurityFixture *>(context);
	if (!fixture.saveSucceeds) return switch_actuator::ports::WebSecurityStoreResult::IoFailure;
	fixture.record = record;
	return switch_actuator::ports::WebSecurityStoreResult::Applied;
}

switch_actuator::ports::WebSecurityStoreResult eraseWebSecurity(void *) noexcept
{
	return switch_actuator::ports::WebSecurityStoreResult::Applied;
}

bool webRandom(void *const context, std::uint8_t *const output, const std::size_t size) noexcept
{
	auto &fixture = *static_cast<WebSecurityFixture *>(context);
	for (std::size_t index = 0; index < size; ++index) output[index] = fixture.randomValue++;
	return true;
}

bool webHmac(void *, const std::uint8_t *const key, const std::size_t keySize,
	const std::string_view message, std::uint8_t *const output, const std::size_t outputSize) noexcept
{
	if (key == nullptr || keySize == 0 || output == nullptr || outputSize != 32) return false;
	std::uint8_t value{key[0]};
	for (const auto character : message) value = static_cast<std::uint8_t>((value * 33U) ^ character);
	for (std::size_t index = 0; index < outputSize; ++index) output[index] = static_cast<std::uint8_t>(value + index);
	return true;
}

bool verifyWebPassword(void *const context, const std::string_view password, const std::uint8_t *, std::size_t,
	std::uint32_t, const std::uint8_t *const expected, const std::size_t expectedSize) noexcept
{
	++static_cast<WebSecurityFixture *>(context)->passwordVerifications;
	return password == "correct-password" && expected != nullptr && expectedSize == 32 && expected[0] == 0xA5;
}

bool deriveWebPassword(void *, const std::string_view password, const std::uint8_t *, std::size_t,
	std::uint32_t, std::uint8_t *const output, const std::size_t outputSize) noexcept
{
	if (password.empty() || output == nullptr || outputSize != 32) return false;
	std::fill_n(output, outputSize, 0xA5);
	return true;
}

bool generateWebIdentity(void *, const std::string_view hostName, char *const certificate,
	const std::size_t certificateCapacity, char *const privateKey, const std::size_t privateKeyCapacity) noexcept
{
	if (hostName.empty() || certificateCapacity < 12U || privateKeyCapacity < 12U) return false;
	std::snprintf(certificate, certificateCapacity, "cert:%.*s", static_cast<int>(hostName.size()), hostName.data());
	std::snprintf(privateKey, privateKeyCapacity, "private-key");
	return true;
}

std::uint32_t webClock(void *const context) noexcept
{
	return static_cast<WebSecurityFixture *>(context)->nowMs;
}

WebSecurityFixture validWebSecurityFixture()
{
	WebSecurityFixture fixture{};
	fixture.record.signingKey[0] = 0x42;
	setText(fixture.record.certificate, "certificate");
	setText(fixture.record.privateKey, "private-key");
	auto &administrator = fixture.record.users[0];
	administrator.id = 1;
	administrator.enabled = true;
	administrator.role = switch_actuator::ports::WebUserRole::Administrator;
	administrator.passwordIterations = 100'000;
	administrator.passwordVerifier[0] = 0xA5;
	setText(administrator.username, "admin");
	return fixture;
}

WebSecurityService makeWebSecurityService(WebSecurityFixture &fixture)
{
	return {{loadWebSecurity, saveWebSecurity, eraseWebSecurity, &fixture},
		{webRandom, webHmac, verifyWebPassword, deriveWebPassword, generateWebIdentity, &fixture},
		{webClock, &fixture}};
}

void testWebSecurityEnforcesOriginHostCsrfAndPermissions()
{
	auto fixture = validWebSecurityFixture();
	auto service = makeWebSecurityService(fixture);
	TEST_ASSERT_EQUAL_UINT8(0, static_cast<std::uint8_t>(service.initialize("https://relay.local", "relay.local")));
	WebSessionCreated created{};
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebSessionResult::Applied),
		static_cast<std::uint8_t>(service.createSession("admin", "correct-password", created)));
	auto security = service.port();
	switch_actuator::ports::WebAuthorization authorization{};
	TEST_ASSERT_TRUE(security.authorize(created.jwt.data(), "https://relay.local", "relay.local",
		created.view.csrfToken.data(), switch_actuator::ports::WebPermission::RelayCommand, true, authorization));
	TEST_ASSERT_FALSE(security.authorize(created.jwt.data(), "https://attacker.invalid", "relay.local",
		created.view.csrfToken.data(), switch_actuator::ports::WebPermission::RelayCommand, true, authorization));
	TEST_ASSERT_FALSE(security.authorize(created.jwt.data(), "https://relay.local", "other.local",
		created.view.csrfToken.data(), switch_actuator::ports::WebPermission::RelayCommand, true, authorization));
	TEST_ASSERT_FALSE(security.authorize(created.jwt.data(), "https://relay.local", "relay.local", "wrong",
		switch_actuator::ports::WebPermission::RelayCommand, true, authorization));
}

void testWebSecurityBoundsSessionsAndExpiresTokens()
{
	auto fixture = validWebSecurityFixture();
	auto service = makeWebSecurityService(fixture);
	static_cast<void>(service.initialize("https://relay.local", "relay.local"));
	WebSessionCreated first{};
	WebSessionCreated second{};
	WebSessionCreated third{};
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebSessionResult::Applied),
		static_cast<std::uint8_t>(service.createSession("admin", "correct-password", first)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebSessionResult::Applied),
		static_cast<std::uint8_t>(service.createSession("admin", "correct-password", second)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebSessionResult::CapacityFull),
		static_cast<std::uint8_t>(service.createSession("admin", "correct-password", third)));
	fixture.nowMs += 15U * 60U * 1000U;
	switch_actuator::app::WebSessionView view{};
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebSessionResult::Unauthorized),
		static_cast<std::uint8_t>(service.inspectSession(first.jwt.data(), view)));
}

void testWebSecurityRateLimitsFailedLogins()
{
	auto fixture = validWebSecurityFixture();
	auto service = makeWebSecurityService(fixture);
	static_cast<void>(service.initialize("https://relay.local", "relay.local"));
	WebSessionCreated created{};
	for (std::size_t attempt = 0; attempt < 5; ++attempt)
	{
		TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebSessionResult::InvalidCredentials),
			static_cast<std::uint8_t>(service.createSession("admin", "wrong-password", created)));
	}
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebSessionResult::RateLimited),
		static_cast<std::uint8_t>(service.createSession("admin", "correct-password", created)));
}

void testWebSecurityCreatesAndUpdatesUserWithoutReplacingPassword()
{
	auto fixture = validWebSecurityFixture();
	auto service = makeWebSecurityService(fixture);
	static_cast<void>(service.initialize("https://relay.local", "relay.local"));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebUserManagementResult::Applied),
		static_cast<std::uint8_t>(service.saveUser(0, "operator", switch_actuator::ports::WebUserRole::Guest,
			true, "operator-password", true)));
	TEST_ASSERT_EQUAL_UINT32(2, fixture.record.users[1].id);
	TEST_ASSERT_EQUAL_UINT8(0xA5, fixture.record.users[1].passwordVerifier[0]);
	const auto verifier = fixture.record.users[1].passwordVerifier;
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebUserManagementResult::Applied),
		static_cast<std::uint8_t>(service.saveUser(2, "operator-renamed", switch_actuator::ports::WebUserRole::Guest,
			true, {}, false)));
	TEST_ASSERT_EQUAL_STRING("operator-renamed", fixture.record.users[1].username.data());
	TEST_ASSERT_TRUE(verifier == fixture.record.users[1].passwordVerifier);
}

void testWebSecurityRejectsDuplicateAndLastAdministrator()
{
	auto fixture = validWebSecurityFixture();
	auto service = makeWebSecurityService(fixture);
	static_cast<void>(service.initialize("https://relay.local", "relay.local"));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebUserManagementResult::Applied),
		static_cast<std::uint8_t>(service.saveUser(0, "operator", switch_actuator::ports::WebUserRole::Guest,
			true, "operator-password", true)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebUserManagementResult::DuplicateUsername),
		static_cast<std::uint8_t>(service.saveUser(2, "admin", switch_actuator::ports::WebUserRole::Guest,
			true, {}, false)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebUserManagementResult::LastAdministrator),
		static_cast<std::uint8_t>(service.saveUser(1, "admin", switch_actuator::ports::WebUserRole::Guest,
			true, {}, false)));
}

void testWebSecurityPersistenceFailureIsAtomic()
{
	auto fixture = validWebSecurityFixture();
	auto service = makeWebSecurityService(fixture);
	static_cast<void>(service.initialize("https://relay.local", "relay.local"));
	fixture.saveSucceeds = false;
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebUserManagementResult::PersistenceFailure),
		static_cast<std::uint8_t>(service.saveUser(1, "changed-admin", switch_actuator::ports::WebUserRole::Administrator,
			true, {}, false)));
	std::array<switch_actuator::app::WebUserView, switch_actuator::ports::webUserCapacity> users{};
	TEST_ASSERT_EQUAL_UINT32(1, service.users(users));
	TEST_ASSERT_EQUAL_STRING("admin", users[0].username.data());
	TEST_ASSERT_EQUAL_STRING("admin", fixture.record.users[0].username.data());
}

void testWebSecurityProvisionsInitialAdministratorAndIdentity()
{
	WebSecurityFixture fixture{};
	auto service = makeWebSecurityService(fixture);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::app::WebSecurityInitializeResult::NotProvisioned),
		static_cast<std::uint8_t>(service.initialize("https://relay.local", "relay.local")));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebUserManagementResult::Applied),
		static_cast<std::uint8_t>(service.provisionInitialAdministrator("admin", "correct-password", "relay.local")));
	TEST_ASSERT_TRUE(service.isInitialized());
	TEST_ASSERT_EQUAL_STRING("admin", fixture.record.users[0].username.data());
	TEST_ASSERT_EQUAL_UINT8(0xA5, fixture.record.users[0].passwordVerifier[0]);
	TEST_ASSERT_EQUAL_STRING("cert:relay.local", fixture.record.certificate.data());
	TEST_ASSERT_EQUAL_STRING("private-key", fixture.record.privateKey.data());
}

void testWebRequestQueueBoundsCapacityAndResultOwnership()
{
	switch_actuator::app::WebRequestQueue queue{};
	for (std::uint32_t index = 0; index < queue.capacity; ++index)
	{
		switch_actuator::app::WebApplicationRequest request{};
		request.operationId = index + 1U;
		request.sessionId = 7;
		request.receivedAtMs = 100;
		TEST_ASSERT_TRUE(queue.enqueue(request));
	}
	switch_actuator::app::WebApplicationRequest overflow{};
	overflow.operationId = 99;
	overflow.sessionId = 7;
	TEST_ASSERT_FALSE(queue.enqueue(overflow));
	TEST_ASSERT_EQUAL_UINT32(queue.capacity, queue.highWaterMark());
	switch_actuator::app::WebOperationResult result{};
	TEST_ASSERT_FALSE(queue.findResult(8, 1, result));
	TEST_ASSERT_TRUE(queue.findResult(7, 1, result));
	for (std::size_t index = 0; index < queue.capacity; ++index)
	{
		switch_actuator::app::WebApplicationRequest request{};
		TEST_ASSERT_TRUE(queue.dequeue(request));
	}
	TEST_ASSERT_TRUE(queue.complete(1, switch_actuator::app::WebOperationStatus::Applied, 200));
	queue.expire(200 + queue.resultRetentionMs);
	TEST_ASSERT_FALSE(queue.findResult(7, 1, result));
}

void testWebCommandTrackerDetectsDuplicateMismatchAndExpiry()
{
	switch_actuator::app::WebCommandTracker tracker{};
	switch_actuator::app::WebTrackedCommand command{};
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::app::WebCommandBeginResult::Accepted),
		static_cast<std::uint8_t>(tracker.begin(7, "request-key", {1}, switch_actuator::domain::RelayAction::SetOn,
			4, 22, 100, command)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::app::WebCommandBeginResult::Duplicate),
		static_cast<std::uint8_t>(tracker.begin(7, "request-key", {1}, switch_actuator::domain::RelayAction::SetOn,
			4, 23, 101, command)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::app::WebCommandBeginResult::IdempotencyMismatch),
		static_cast<std::uint8_t>(tracker.begin(7, "request-key", {1}, switch_actuator::domain::RelayAction::SetOff,
			4, 24, 102, command)));
	switch_actuator::domain::RelaySnapshot snapshot{};
	snapshot.appliedState = switch_actuator::domain::RelayState::On;
	snapshot.transitionSequence = 5;
	TEST_ASSERT_TRUE(tracker.complete(22, switch_actuator::app::RelayCommandStatus::Accepted,
		switch_actuator::app::RelayCommandReason::None, snapshot, 200, command));
	TEST_ASSERT_FALSE(tracker.findByCorrelation(8, 22, command));
	TEST_ASSERT_TRUE(tracker.findByCorrelation(7, 22, command));
	tracker.expire(200 + tracker.retentionMs);
	TEST_ASSERT_FALSE(tracker.findByCorrelation(7, 22, command));
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
	RUN_TEST(testRejectsDuplicateWifiProfiles);
	RUN_TEST(testRejectsIncompleteStaticWifiProfile);
	RUN_TEST(testWifiProfileUpdatePreservesStoredSecret);
	RUN_TEST(testRejectsStaticWifiGatewayOutsideSubnet);
	RUN_TEST(testWifiManagementRejectsStaleGeneration);
	RUN_TEST(testWifiProfileRemovalCompactsPriorityOrder);
	RUN_TEST(testWifiPersistenceFailureLeavesActiveConfigurationUnchanged);
	RUN_TEST(testRejectsModbusSevenDataBits);
	RUN_TEST(testEnqueuesTypedChannelCommand);
	RUN_TEST(testEnqueuesAtomicParticipantGroup);
	RUN_TEST(testGroupWithoutParticipantsDoesNotQueue);
	RUN_TEST(testRejectsInvalidChannelBeforeQueueing);
	RUN_TEST(testPolicyRejectsLockedChannelBeforeQueueing);
	RUN_TEST(testSceneRecallQueuesMixedAtomicStates);
	RUN_TEST(testSceneLearningIsFailureAtomic);
	RUN_TEST(testTimerReplacementAndWrapAroundExpiry);
	RUN_TEST(testWebEventJournalReportsOverwrittenSequenceGap);
	RUN_TEST(testWebSecurityEnforcesOriginHostCsrfAndPermissions);
	RUN_TEST(testWebSecurityBoundsSessionsAndExpiresTokens);
	RUN_TEST(testWebSecurityRateLimitsFailedLogins);
	RUN_TEST(testWebSecurityVerifiesPasswordsForUnknownAndDisabledUsers);
	RUN_TEST(testWebSecurityCreatesAndUpdatesUserWithoutReplacingPassword);
	RUN_TEST(testWebSecurityRejectsDuplicateAndLastAdministrator);
	RUN_TEST(testWebSecurityPersistenceFailureIsAtomic);
	RUN_TEST(testWebSecurityProvisionsInitialAdministratorAndIdentity);
	RUN_TEST(testWebRequestQueueBoundsCapacityAndResultOwnership);
	RUN_TEST(testWebCommandTrackerDetectsDuplicateMismatchAndExpiry);
	return UNITY_END();
}

void testWebSecurityVerifiesPasswordsForUnknownAndDisabledUsers()
{
	auto fixture = validWebSecurityFixture();
	auto &disabled = fixture.record.users[1];
	disabled.id = 2;
	disabled.enabled = false;
	disabled.passwordIterations = 100'000;
	disabled.passwordVerifier[0] = 0xA5;
	setText(disabled.username, "disabled");
	auto service = makeWebSecurityService(fixture);
	static_cast<void>(service.initialize("https://relay.local", "relay.local"));
	WebSessionCreated created{};

	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebSessionResult::InvalidCredentials),
		static_cast<std::uint8_t>(service.createSession("unknown", "correct-password", created)));
	TEST_ASSERT_EQUAL_UINT32(1, fixture.passwordVerifications);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebSessionResult::InvalidCredentials),
		static_cast<std::uint8_t>(service.createSession("disabled", "correct-password", created)));
	TEST_ASSERT_EQUAL_UINT32(2, fixture.passwordVerifications);
}