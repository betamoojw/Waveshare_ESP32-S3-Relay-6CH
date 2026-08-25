#include "../../src/domain/configuration.h"
#include "../../src/domain/device_identity.h"
#include "../../src/domain/deployment_profile.h"
#include "../../src/domain/relay_policy.h"
#include "../../src/adapters/knx/knx_application_model.h"
#include "../../src/hal/BoardDescriptor.h"
#include "../../src/hal/ButtonHal.h"
#include "../../src/hal/BuzzerHal.h"
#include "../../src/hal/RelayHal.h"
#include "../../src/hal/RgbLedHal.h"
#include "../../src/hal/Rs485Hal.h"
#include "../../src/app/configuration_service.h"
#include "../../src/app/diagnostics_service.h"
#include "../../src/app/wifi_management_service.h"
#include "../../src/app/web_event_journal.h"
#include "../../src/app/web_security_service.h"
#include "../../src/app/web_command_tracker.h"
#include "../../src/app/web_request_queue.h"
#include "../../src/app/switching_policy_service.h"
#include "../../src/app/relay_command_service.h"
#include "../../src/app/relay_timer_service.h"
#include "../../src/app/scene_service.h"
#include "../../src/app/service_mode_service.h"
#include "../../src/app/error_mapping.h"
#include "../../src/adapters/knx/knx_error_representation.h"
#include "../../src/adapters/modbus/modbus_error_representation.h"
#include "../../src/adapters/modbus/modbus_register_map.h"
#include "../../src/adapters/network/null_ethernet_adapter.h"
#include "../../src/adapters/web/web_error_representation.h"

#include <unity.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdint>
#include <cstdio>

namespace
{
static_assert(switch_actuator::app::errorCode(switch_actuator::app::SwitchingPolicyResult::QueueFull) ==
	switch_actuator::domain::ErrorCode::Busy);
static_assert(switch_actuator::app::errorCode(switch_actuator::app::SwitchingPolicyResult::SafetyLockout) ==
	switch_actuator::domain::ErrorCode::Forbidden);
static_assert(switch_actuator::adapters::web::represent(switch_actuator::domain::ErrorCode::Unauthorized).status == 401);
static_assert(switch_actuator::adapters::modbus::represent(switch_actuator::domain::ErrorCode::NotFound) ==
	NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
static_assert(switch_actuator::adapters::knx::represent(switch_actuator::domain::ErrorCode::Busy) ==
	switch_actuator::adapters::knx::KnxErrorRepresentation::SilentRejectBusy);

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
	bool eraseCalled{false};
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

switch_actuator::ports::SettingsEraseResult eraseSettings(void *const context) noexcept
{
	static_cast<SettingsFixture *>(context)->eraseCalled = true;
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

void testVersionCompatibilityContract()
{
	using namespace switch_actuator;
	TEST_ASSERT_EQUAL_STRING("FW-1.4.0+development", domain::compatibility::firmware.data());
	TEST_ASSERT_EQUAL_STRING("CFG-4", domain::compatibility::configuration.label.data());
	TEST_ASSERT_EQUAL_STRING("API-v1", domain::compatibility::api.label.data());
	TEST_ASSERT_EQUAL_STRING("MODBUS-v1", domain::compatibility::modbus.label.data());
	TEST_ASSERT_EQUAL_STRING("KNX-APP-v1", domain::compatibility::knxApplication.label.data());
	TEST_ASSERT_EQUAL_STRING("FS-v1", domain::compatibility::filesystem.label.data());
	TEST_ASSERT_EQUAL_UINT16(domain::compatibility::configuration.major,
		domain::currentConfigurationSchemaVersion);
	TEST_ASSERT_EQUAL_UINT16(domain::compatibility::modbus.major,
		adapters::modbus::ModbusRegisterMap::versionMajor);
	TEST_ASSERT_EQUAL_UINT16(domain::compatibility::knxApplication.major,
		adapters::knx::applicationModelMajorVersion);
	TEST_ASSERT_EQUAL_UINT16(104, domain::compatibility::firmwareModbusRegister);
}

void testKnxApplicationModelV1Contract()
{
	using namespace switch_actuator::adapters::knx;
	TEST_ASSERT_EQUAL_UINT8(1, applicationModelMajorVersion);
	TEST_ASSERT_EQUAL_UINT8(0, applicationModelMinorVersion);
	TEST_ASSERT_EQUAL_UINT16(16, channelObjectNumber(0, 0));
	TEST_ASSERT_EQUAL_UINT16(96, channelObjectNumber(5, 0));
	TEST_ASSERT_EQUAL_UINT16(105, channelObjectNumber(5, 9));
	TEST_ASSERT_EQUAL_UINT16(invalidCommunicationObjectNumber, channelObjectNumber(6, 0));
	TEST_ASSERT_EQUAL_UINT16(0x1234, packIndividualAddress(1, 2, 0x34));
	TEST_ASSERT_EQUAL_UINT16(0x2B44, packThreeLevelGroupAddress(5, 3, 0x44));
	TEST_ASSERT_FALSE(isConfiguredGroupAddress(packThreeLevelGroupAddress(0, 0, 0)));
	TEST_ASSERT_TRUE(hasFlag(channelSwitchFlags(true), CommunicationObjectFlag::Read));
	TEST_ASSERT_FALSE(hasFlag(channelSwitchFlags(false), CommunicationObjectFlag::Read));
	TEST_ASSERT_EQUAL_STRING("DPT 1.001", datapointTypeName(DatapointType::Switch1_001).data());
	TEST_ASSERT_FALSE(isExposedInVersion1(channelCommunicationObjectTemplates[2]));
	TEST_ASSERT_FALSE(isExposedInVersion1(channelCommunicationObjectTemplates[3]));
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
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::ports::WebAuthorizationResult::Authorized),
		static_cast<std::uint8_t>(security.authorize(created.jwt.data(), "https://relay.local", "relay.local",
			created.view.csrfToken.data(), switch_actuator::ports::WebPermission::RelayCommand, true, authorization)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::ports::WebAuthorizationResult::Forbidden),
		static_cast<std::uint8_t>(security.authorize(created.jwt.data(), "https://attacker.invalid", "relay.local",
			created.view.csrfToken.data(), switch_actuator::ports::WebPermission::RelayCommand, true, authorization)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::ports::WebAuthorizationResult::Forbidden),
		static_cast<std::uint8_t>(security.authorize(created.jwt.data(), "https://relay.local", "other.local",
			created.view.csrfToken.data(), switch_actuator::ports::WebPermission::RelayCommand, true, authorization)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::ports::WebAuthorizationResult::Forbidden),
		static_cast<std::uint8_t>(security.authorize(created.jwt.data(), "https://relay.local", "relay.local", "wrong",
			switch_actuator::ports::WebPermission::RelayCommand, true, authorization)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::ports::WebAuthorizationResult::Unauthenticated),
		static_cast<std::uint8_t>(security.authorize({}, "https://relay.local", "relay.local",
			created.view.csrfToken.data(), switch_actuator::ports::WebPermission::RelayCommand, true, authorization)));
	for (std::uint8_t request = 1; request < 10U; ++request)
	{
		TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::ports::WebAuthorizationResult::Authorized),
			static_cast<std::uint8_t>(security.authorize(created.jwt.data(), "https://relay.local", "relay.local",
				created.view.csrfToken.data(), switch_actuator::ports::WebPermission::RelayCommand, true, authorization)));
	}
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::ports::WebAuthorizationResult::RateLimited),
		static_cast<std::uint8_t>(security.authorize(created.jwt.data(), "https://relay.local", "relay.local",
			created.view.csrfToken.data(), switch_actuator::ports::WebPermission::RelayCommand, true, authorization)));
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

void testWebSecurityResetPreservesIdentityAndErasesUsers()
{
	auto fixture = validWebSecurityFixture();
	const auto signingKey = fixture.record.signingKey;
	const auto certificate = fixture.record.certificate;
	const auto privateKey = fixture.record.privateKey;
	auto service = makeWebSecurityService(fixture);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::app::WebSecurityInitializeResult::Initialized),
		static_cast<std::uint8_t>(service.initialize("https://relay.local", "relay.local")));
	WebSessionCreated session{};
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebSessionResult::Applied),
		static_cast<std::uint8_t>(service.createSession("admin", "correct-password", session)));

	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::ports::WebSecurityStoreResult::Applied),
		static_cast<std::uint8_t>(service.eraseUsersPreservingIdentity()));
	TEST_ASSERT_TRUE(fixture.record.signingKey == signingKey);
	TEST_ASSERT_TRUE(fixture.record.certificate == certificate);
	TEST_ASSERT_TRUE(fixture.record.privateKey == privateKey);
	TEST_ASSERT_TRUE(std::all_of(fixture.record.users.begin(), fixture.record.users.end(),
		[](const auto &user) { return user.id == 0; }));
	switch_actuator::app::WebSessionView view{};
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebSessionResult::Unauthorized),
		static_cast<std::uint8_t>(service.inspectSession(session.jwt.data(), view)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(switch_actuator::app::WebSecurityInitializeResult::NotProvisioned),
		static_cast<std::uint8_t>(service.initialize("https://relay.local", "relay.local")));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(WebUserManagementResult::Applied),
		static_cast<std::uint8_t>(service.provisionInitialAdministrator("replacement-admin",
			"correct-password", "relay.local")));
	TEST_ASSERT_TRUE(fixture.record.signingKey == signingKey);
	TEST_ASSERT_TRUE(fixture.record.certificate == certificate);
	TEST_ASSERT_TRUE(fixture.record.privateKey == privateKey);
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

switch_actuator::domain::DeviceIdentitySource validDeviceIdentitySource()
{
	switch_actuator::domain::DeviceIdentitySource source{
		"SA-6CH-S3",
		"Switch Actuator 6CH",
		"Waveshare ESP32-S3-Relay-6CH",
		"1.x",
		"1.00",
		"TEST-0001",
		{},
		{},
		"2026-08-24",
		42,
	};
	source.deviceUuid[0] = 1;
	source.macAddress = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
	return source;
}

void testBuildsProductionAndUnprovisionedDeviceIdentity()
{
	auto source = validDeviceIdentitySource();
	const auto productionIdentity = switch_actuator::domain::makeDeviceIdentity(source);
	TEST_ASSERT_TRUE(productionIdentity.has_value());
	TEST_ASSERT_TRUE(switch_actuator::domain::isManufacturingIdentityProvisioned(*productionIdentity));
	TEST_ASSERT_EQUAL_STRING("SA-6CH-S3", productionIdentity->productId.value.data());
	TEST_ASSERT_EQUAL_UINT32(42, productionIdentity->manufacturingBatch);

	source.manufacturingDate = {};
	source.manufacturingBatch = 0;
	const auto developmentIdentity = switch_actuator::domain::makeDeviceIdentity(source);
	TEST_ASSERT_TRUE(developmentIdentity.has_value());
	TEST_ASSERT_FALSE(switch_actuator::domain::isManufacturingIdentityProvisioned(*developmentIdentity));
}

void testRejectsInvalidDeviceIdentity()
{
	auto source = validDeviceIdentitySource();
	source.manufacturingDate = "2025-02-29";
	TEST_ASSERT_FALSE(switch_actuator::domain::makeDeviceIdentity(source).has_value());

	source = validDeviceIdentitySource();
	source.macAddress.fill(0);
	TEST_ASSERT_FALSE(switch_actuator::domain::makeDeviceIdentity(source).has_value());

	source = validDeviceIdentitySource();
	source.manufacturingDate = {};
	TEST_ASSERT_FALSE(switch_actuator::domain::makeDeviceIdentity(source).has_value());
}

void testProductionProfileLocksAfterSecurityProvisioning()
{
	using switch_actuator::domain::DeploymentProfile;
	using switch_actuator::domain::factoryConfigurationLocked;
	TEST_ASSERT_FALSE(factoryConfigurationLocked(DeploymentProfile::Development, true, true));
	TEST_ASSERT_FALSE(factoryConfigurationLocked(DeploymentProfile::Engineering, true, true));
	TEST_ASSERT_FALSE(factoryConfigurationLocked(DeploymentProfile::Production, false, false));
	TEST_ASSERT_TRUE(factoryConfigurationLocked(DeploymentProfile::Production, true, false));
	TEST_ASSERT_TRUE(factoryConfigurationLocked(DeploymentProfile::Production, false, true));
}

void testRejectsPartialManufacturingConfiguration()
{
	auto configuration = validConfiguration();
	setText(configuration.productId.value, "SA-6CH-S3");
	setText(configuration.manufacturingDate.iso8601, "2026-08-24");
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(ConfigurationValidationError::InvalidManufacturingIdentity),
		static_cast<std::uint8_t>(switch_actuator::domain::validateConfiguration(configuration)));
	configuration.manufacturingBatch = 42;
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(ConfigurationValidationError::None),
		static_cast<std::uint8_t>(switch_actuator::domain::validateConfiguration(configuration)));
}

void testRelaySafetyEventDecisionTable()
{
	using switch_actuator::domain::RelayBootState;
	using switch_actuator::domain::RelaySafetyEvent;
	using switch_actuator::domain::ResetCategory;
	using switch_actuator::domain::relayBootStateFor;
	using switch_actuator::domain::relaySafetyEventForReset;
	const auto assertState = [](const RelaySafetyEvent event, const RelayBootState expected) {
		TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(expected),
			static_cast<std::uint8_t>(relayBootStateFor(event)));
	};
	assertState(RelaySafetyEvent::PowerOn, RelayBootState::Restore);
	assertState(RelaySafetyEvent::Brownout, RelayBootState::SafeState);
	assertState(RelaySafetyEvent::WatchdogReset, RelayBootState::SafeState);
	assertState(RelaySafetyEvent::SoftwareReboot, RelayBootState::Restore);
	assertState(RelaySafetyEvent::OtaReboot, RelayBootState::ConfiguredState);
	assertState(RelaySafetyEvent::FactoryReset, RelayBootState::Off);
	assertState(RelaySafetyEvent::ConfigurationUpdate, RelayBootState::LastState);
	assertState(RelaySafetyEvent::NetworkFailure, RelayBootState::LastState);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(RelaySafetyEvent::PowerOn),
		static_cast<std::uint8_t>(relaySafetyEventForReset(ResetCategory::PowerOn)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(RelaySafetyEvent::SoftwareReboot),
		static_cast<std::uint8_t>(relaySafetyEventForReset(ResetCategory::ControlledRestart)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(RelaySafetyEvent::WatchdogReset),
		static_cast<std::uint8_t>(relaySafetyEventForReset(ResetCategory::Unknown)));
}

void testResolvesEveryRelayBootStateFailSafe()
{
	using switch_actuator::domain::PersistedRelayState;
	using switch_actuator::domain::RelayBootState;
	using switch_actuator::domain::RelayChannelConfiguration;
	using switch_actuator::domain::RelayState;
	using switch_actuator::domain::RestorePolicy;
	using switch_actuator::domain::resolveRelayBootState;
	RelayChannelConfiguration configuration{true, RestorePolicy::ConfiguredDefault, RelayState::On};
	PersistedRelayState persisted{};
	persisted.valid = true;
	persisted.states[0] = RelayState::On;
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(RelayState::Off),
		static_cast<std::uint8_t>(resolveRelayBootState(RelayBootState::Off, configuration, persisted, 0)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(RelayState::On),
		static_cast<std::uint8_t>(resolveRelayBootState(RelayBootState::On, configuration, persisted, 0)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(RelayState::On),
		static_cast<std::uint8_t>(resolveRelayBootState(RelayBootState::Restore, configuration, persisted, 0)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(RelayState::On),
		static_cast<std::uint8_t>(resolveRelayBootState(RelayBootState::LastState, configuration, persisted, 0)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(RelayState::Off),
		static_cast<std::uint8_t>(resolveRelayBootState(RelayBootState::SafeState, configuration, persisted, 0)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(RelayState::On),
		static_cast<std::uint8_t>(resolveRelayBootState(RelayBootState::ConfiguredState, configuration, persisted, 0)));

	persisted.valid = false;
	configuration.restorePolicy = RestorePolicy::LastKnown;
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(RelayState::Off),
		static_cast<std::uint8_t>(resolveRelayBootState(RelayBootState::Restore, configuration, persisted, 0)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(RelayState::Off),
		static_cast<std::uint8_t>(resolveRelayBootState(RelayBootState::LastState, configuration, persisted, 0)));
	persisted.valid = true;
	persisted.states[0] = static_cast<RelayState>(0xFF);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(RelayState::Off),
		static_cast<std::uint8_t>(resolveRelayBootState(RelayBootState::LastState, configuration, persisted, 0)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(RelayState::Off),
		static_cast<std::uint8_t>(resolveRelayBootState(static_cast<RelayBootState>(0xFF), configuration, persisted, 0)));
	configuration.enabled = false;
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(RelayState::Off),
		static_cast<std::uint8_t>(resolveRelayBootState(RelayBootState::On, configuration, persisted, 0)));
}

struct HalFixture final
{
	bool initialized{false};
	bool pressed{true};
	std::uint32_t writes{0};
};

bool initializeHal(void *const context) noexcept
{
	static_cast<HalFixture *>(context)->initialized = true;
	return true;
}

bool readButtonHal(void *const context) noexcept
{
	return static_cast<HalFixture *>(context)->pressed;
}

bool writeRgbHal(void *const context, std::uint8_t, std::uint8_t, std::uint8_t) noexcept
{
	++static_cast<HalFixture *>(context)->writes;
	return true;
}

bool writeBuzzerHal(void *const context, std::uint16_t, std::uint8_t) noexcept
{
	++static_cast<HalFixture *>(context)->writes;
	return true;
}

std::int32_t readRs485Hal(void *, std::uint8_t *, const std::uint16_t count, std::int32_t) noexcept
{
	return count;
}

std::int32_t writeRs485Hal(void *, const std::uint8_t *, const std::uint16_t count, std::int32_t) noexcept
{
	return count;
}

void testHalContractsDispatchAndFailClosed()
{
	using namespace switch_actuator;
	HalFixture fixture{};
	hal::RelayHal invalidRelay{};
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(hal::RelayHalResult::HardwareFailure),
		static_cast<std::uint8_t>(invalidRelay.apply({0}, domain::RelayState::On)));
	hal::ButtonHal button{initializeHal, readButtonHal, &fixture};
	TEST_ASSERT_TRUE(button.initialize());
	TEST_ASSERT_TRUE(button.isPressed());
	hal::RgbLedHal rgb{writeRgbHal, &fixture};
	hal::BuzzerHal buzzer{initializeHal, writeBuzzerHal, &fixture};
	TEST_ASSERT_TRUE(rgb.write(1, 2, 3));
	TEST_ASSERT_TRUE(buzzer.initialize());
	TEST_ASSERT_TRUE(buzzer.write(2000, 10));
	TEST_ASSERT_EQUAL_UINT32(2, fixture.writes);
	hal::Rs485Hal rs485{readRs485Hal, writeRs485Hal, &fixture};
	TEST_ASSERT_TRUE(rs485.isValid());
	std::array<std::uint8_t, 4> bytes{};
	TEST_ASSERT_EQUAL_INT32(4, rs485.read(rs485.context, bytes.data(), bytes.size(), 10));
	TEST_ASSERT_EQUAL_INT32(4, rs485.write(rs485.context, bytes.data(), bytes.size(), 10));
	constexpr std::array<std::uint8_t, 6> relayPins{1, 2, 3, 4, 5, 6};
	constexpr hal::BoardDescriptor descriptor{"TEST-6CH", "test", "HW-A01",
		static_cast<std::uint8_t>(relayPins.size()), relayPins.data(),
		hal::RelayPolarity::ActiveLow, {0, hal::ButtonPullMode::PullUp, true}, {9, 10}, {7, 8, true},
		{true, false, hal::EthernetImplementation::None}};
	TEST_ASSERT_TRUE(hal::isValid(descriptor));
	TEST_ASSERT_TRUE(hal::supportsRelayCount(descriptor, domain::relayChannelCount));
	TEST_ASSERT_FALSE(descriptor.relayActiveLevel());
	TEST_ASSERT_TRUE(descriptor.relayInactiveLevel());

	constexpr std::array<std::uint8_t, 6> duplicateRelayPins{1, 2, 3, 4, 5, 5};
	constexpr hal::BoardDescriptor duplicateDescriptor{"TEST-DUPLICATE", "test", "HW-A01",
		static_cast<std::uint8_t>(duplicateRelayPins.size()), duplicateRelayPins.data(),
		hal::RelayPolarity::ActiveHigh, {0, hal::ButtonPullMode::PullUp, true}, {9, 10}, {7, 8, true},
		{true, false, hal::EthernetImplementation::None}};
	TEST_ASSERT_FALSE(hal::isValid(duplicateDescriptor));

	constexpr std::array<std::uint8_t, 12> twelveRelayPins{1, 2, 3, 4, 5, 6, 11, 12, 13, 14, 15, 16};
	constexpr hal::BoardDescriptor twelveChannelDescriptor{"TEST-12CH", "test", "HW-A01",
		static_cast<std::uint8_t>(twelveRelayPins.size()), twelveRelayPins.data(),
		hal::RelayPolarity::ActiveHigh, {0, hal::ButtonPullMode::PullUp, true}, {9, 10}, {7, 8, true},
		{true, false, hal::EthernetImplementation::None}};
	TEST_ASSERT_TRUE(hal::isValid(twelveChannelDescriptor));
	TEST_ASSERT_FALSE(hal::supportsRelayCount(twelveChannelDescriptor, domain::relayChannelCount));
}

void testModbusV1UartSettingsEncoding()
{
	using namespace switch_actuator;
	domain::ModbusConfiguration configuration{};
	std::uint16_t encoded{};
	TEST_ASSERT_TRUE(adapters::modbus::ModbusRegisterMap::encodeUartSettings(configuration, encoded));
	TEST_ASSERT_EQUAL_UINT16(4, encoded);

	configuration.baudRate = 57600;
	configuration.parity = domain::SerialParity::Odd;
	configuration.stopBits = 2;
	TEST_ASSERT_TRUE(adapters::modbus::ModbusRegisterMap::encodeUartSettings(configuration, encoded));
	TEST_ASSERT_EQUAL_UINT16(51, encoded);

	domain::ModbusConfiguration decoded{};
	TEST_ASSERT_TRUE(adapters::modbus::ModbusRegisterMap::decodeUartSettings(encoded, decoded));
	TEST_ASSERT_EQUAL_UINT32(57600, decoded.baudRate);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(domain::SerialParity::Odd),
		static_cast<std::uint8_t>(decoded.parity));
	TEST_ASSERT_EQUAL_UINT8(2, decoded.stopBits);
	TEST_ASSERT_FALSE(adapters::modbus::ModbusRegisterMap::decodeUartSettings(0x0040, decoded));
	TEST_ASSERT_FALSE(adapters::modbus::ModbusRegisterMap::decodeUartSettings(0x0018, decoded));
}

void testModbusV1HoldingWriteValidationIsAtomic()
{
	using namespace switch_actuator;
	adapters::modbus::ModbusRegisterMap map{};
	adapters::modbus::HoldingWriteBatch batch{};
	constexpr std::array<std::uint16_t, 2> invalidRelayValues{1, 3};
	const auto relayResult = map.parseHoldingWrite(32, invalidRelayValues.data(), invalidRelayValues.size(),
		domain::CommandSource::Modbus, 1, 10, batch);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(adapters::modbus::RegisterMapResult::IllegalValue),
		static_cast<std::uint8_t>(relayResult));
	TEST_ASSERT_EQUAL_UINT32(0, batch.relayCommandCount);

	constexpr std::uint16_t uartValue{51};
	const auto uartResult = map.parseHoldingWrite(128, &uartValue, 1,
		domain::CommandSource::Modbus, 2, 10, batch);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(adapters::modbus::RegisterMapResult::Success),
		static_cast<std::uint8_t>(uartResult));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(adapters::modbus::HoldingWriteKind::UartSettings),
		static_cast<std::uint8_t>(batch.kind));
	TEST_ASSERT_EQUAL_UINT16(uartValue, batch.uartEncodedSettings);
}
}

void testPersistentDiagnosticCountersTrackErrorsAndDirtyState()
{
	switch_actuator::app::DiagnosticsService diagnostics{};
	switch_actuator::domain::PersistentDiagnosticCounters counters{};
	counters.bootCount = 12;
	diagnostics.setPersistentCounters(counters);
	TEST_ASSERT_EQUAL_UINT32(12, diagnostics.snapshot().bootCount);
	TEST_ASSERT_FALSE(diagnostics.persistentCountersDirty());

	diagnostics.recordModbus(switch_actuator::app::ModbusDiagnosticEvent::ValidRequest);
	TEST_ASSERT_FALSE(diagnostics.persistentCountersDirty());
	diagnostics.recordModbus(switch_actuator::app::ModbusDiagnosticEvent::CrcError);
	diagnostics.recordKnxTelegram(false);
	diagnostics.recordOtaFailure();
	diagnostics.recordStorageFailure();
	TEST_ASSERT_EQUAL_UINT32(1, diagnostics.snapshot().persistentCounters.modbusErrorCount);
	TEST_ASSERT_EQUAL_UINT32(1, diagnostics.snapshot().persistentCounters.knxErrorCount);
	TEST_ASSERT_EQUAL_UINT32(1, diagnostics.snapshot().persistentCounters.otaFailureCount);
	TEST_ASSERT_EQUAL_UINT32(1, diagnostics.snapshot().persistentCounters.storageErrorCount);
	TEST_ASSERT_TRUE(diagnostics.persistentCountersDirty());
	diagnostics.markPersistentCountersSaved();
	TEST_ASSERT_FALSE(diagnostics.persistentCountersDirty());
}

void testPersistentDiagnosticFaultCountersCountIncidentsOnly()
{
	switch_actuator::app::DiagnosticsService diagnostics{};
	diagnostics.setPersistentCounters({});
	static_cast<void>(diagnostics.recordFault(switch_actuator::domain::FaultCode::InvalidConfiguration,
		switch_actuator::domain::FaultSeverity::Warning, 10));
	static_cast<void>(diagnostics.recordFault(switch_actuator::domain::FaultCode::InvalidConfiguration,
		switch_actuator::domain::FaultSeverity::Warning, 20));
	TEST_ASSERT_EQUAL_UINT32(1, diagnostics.snapshot().persistentCounters.configErrorCount);
	static_cast<void>(diagnostics.clearFault(switch_actuator::domain::FaultCode::InvalidConfiguration));
	static_cast<void>(diagnostics.recordFault(switch_actuator::domain::FaultCode::InvalidConfiguration,
		switch_actuator::domain::FaultSeverity::Warning, 30));
	TEST_ASSERT_EQUAL_UINT32(2, diagnostics.snapshot().persistentCounters.configErrorCount);

	static_cast<void>(diagnostics.recordFault(switch_actuator::domain::FaultCode::TaskWatchdogFailure,
		switch_actuator::domain::FaultSeverity::Critical, 40));
	static_cast<void>(diagnostics.recordFault(switch_actuator::domain::FaultCode::FileSystemFailure,
		switch_actuator::domain::FaultSeverity::Warning, 50));
	TEST_ASSERT_EQUAL_UINT32(1, diagnostics.snapshot().persistentCounters.watchdogCount);
	TEST_ASSERT_EQUAL_UINT32(1, diagnostics.snapshot().persistentCounters.storageErrorCount);
}

void testPersistentNetworkFailuresCountStateTransitionsOnly()
{
	switch_actuator::app::DiagnosticsService diagnostics{};
	diagnostics.setPersistentCounters({});
	switch_actuator::ports::NetworkStatusSnapshot network{};
	network.state = switch_actuator::ports::NetworkLifecycleState::OnlineWifi;
	network.infrastructureOnline = true;
	diagnostics.updateNetwork(network);
	diagnostics.updateNetwork(network);
	TEST_ASSERT_EQUAL_UINT32(0, diagnostics.snapshot().persistentCounters.networkFailureCount);

	network.state = switch_actuator::ports::NetworkLifecycleState::ConnectingWifi;
	network.infrastructureOnline = false;
	diagnostics.updateNetwork(network);
	diagnostics.updateNetwork(network);
	TEST_ASSERT_EQUAL_UINT32(1, diagnostics.snapshot().persistentCounters.networkFailureCount);
	network.state = switch_actuator::ports::NetworkLifecycleState::RecoveryAp;
	network.recoveryApActive = true;
	diagnostics.updateNetwork(network);
	diagnostics.updateNetwork(network);
	TEST_ASSERT_EQUAL_UINT32(2, diagnostics.snapshot().persistentCounters.networkFailureCount);
}

void testUnsupportedEthernetAdapterIsUnavailableAndInert()
{
	using namespace switch_actuator;
	adapters::network::NullEthernetAdapter adapter{};
	const auto port = adapter.port();
	TEST_ASSERT_FALSE(port.isAvailable());
	TEST_ASSERT_FALSE(port.initialize("relay-actuator"));
	port.update(1000);
	port.shutdown();
	const auto &snapshot = port.snapshot();
	TEST_ASSERT_FALSE(snapshot.available);
	TEST_ASSERT_FALSE(snapshot.linkUp);
	TEST_ASSERT_FALSE(snapshot.online);
	TEST_ASSERT_EQUAL_UINT8(0, snapshot.ipv4Address[0]);
}

void testBoardDescriptorRejectsInconsistentEthernetCapability()
{
	using namespace switch_actuator;
	constexpr std::array<std::uint8_t, 6> relayPins{1, 2, 3, 4, 5, 6};
	constexpr hal::BoardDescriptor claimsEthernetWithoutImplementation{"TEST-ETH", "test", "HW-A01",
		static_cast<std::uint8_t>(relayPins.size()), relayPins.data(), hal::RelayPolarity::ActiveLow,
		{0, hal::ButtonPullMode::PullUp, true}, {9, 10}, {7, 8, true},
		{true, true, hal::EthernetImplementation::None}};
	constexpr hal::BoardDescriptor hidesEthernetImplementation{"TEST-HIDDEN-ETH", "test", "HW-A01",
		static_cast<std::uint8_t>(relayPins.size()), relayPins.data(), hal::RelayPolarity::ActiveLow,
		{0, hal::ButtonPullMode::PullUp, true}, {9, 10}, {7, 8, true},
		{true, false, hal::EthernetImplementation::SpiController}};
	TEST_ASSERT_FALSE(hal::isValid(claimsEthernetWithoutImplementation));
	TEST_ASSERT_FALSE(hal::isValid(hidesEthernetImplementation));
}

void testServiceModeRequiresPhysicalEntryAndExpires()
{
	using namespace switch_actuator;
	app::ServiceModeService serviceMode{};
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(app::ServiceModeResult::NotAuthorized),
		static_cast<std::uint8_t>(serviceMode.authorize(app::ServiceModeOperation::ReadIdentity,
			domain::DeploymentProfile::Development, false)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(app::ServiceModeResult::Applied),
		static_cast<std::uint8_t>(serviceMode.enterFromPhysicalPresence(1000)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(app::ServiceModeState::Service),
		static_cast<std::uint8_t>(serviceMode.snapshot().state));
	TEST_ASSERT_FALSE(serviceMode.update(1000 + app::ServiceModeService::sessionDurationMs - 1));
	TEST_ASSERT_TRUE(serviceMode.update(1000 + app::ServiceModeService::sessionDurationMs));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(app::ServiceModeState::User),
		static_cast<std::uint8_t>(serviceMode.snapshot().state));
}

void testServiceModeSeparatesFieldAndFactoryOperations()
{
	using namespace switch_actuator;
	app::ServiceModeService serviceMode{};
	static_cast<void>(serviceMode.enterFromPhysicalPresence(2000));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(app::ServiceModeResult::Applied),
		static_cast<std::uint8_t>(serviceMode.authorize(app::ServiceModeOperation::EraseUserConfiguration,
			domain::DeploymentProfile::Production, true)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(app::ServiceModeResult::FactoryConfigurationLocked),
		static_cast<std::uint8_t>(serviceMode.authorize(app::ServiceModeOperation::ProvisionIdentity,
			domain::DeploymentProfile::Production, true)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(app::ServiceModeResult::Unsupported),
		static_cast<std::uint8_t>(serviceMode.authorize(app::ServiceModeOperation::FirmwareRecovery,
			domain::DeploymentProfile::Production, true)));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(app::ServiceModeResult::Applied),
		static_cast<std::uint8_t>(serviceMode.exit()));
}

void testServiceResetPreservesIdentityAndErasesUserConfiguration()
{
	using namespace switch_actuator;
	SettingsFixture fixture{};
	setText(fixture.stored.manufacturingDate.iso8601, "2026-08-24");
	fixture.stored.manufacturingBatch = 42;
	fixture.stored.network.wifiProfiles[0].enabled = true;
	setText(fixture.stored.network.wifiProfiles[0].ssid, "Factory-Wifi");
	setText(fixture.stored.network.wifiProfiles[0].passphrase, "factory-secret");
	fixture.stored.knx.enabled = true;
	fixture.stored.knx.individualAddress = 0x1101;
	fixture.stored.web.enabled = true;
	fixture.stored.web.securityProvisioned = true;
	fixture.stored.relayChannels[0].configuredDefault = domain::RelayState::On;
	auto service = configurationService(fixture);

	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(app::ConfigurationUserResetResult::Erased),
		static_cast<std::uint8_t>(service.eraseUserConfiguration()));
	const auto &reset = service.active();
	TEST_ASSERT_EQUAL_STRING("TEST-0001", reset.deviceSerial.data());
	TEST_ASSERT_EQUAL_UINT8(1, reset.deviceUuid[0]);
	TEST_ASSERT_EQUAL_STRING("2026-08-24", reset.manufacturingDate.iso8601.data());
	TEST_ASSERT_EQUAL_UINT32(42, reset.manufacturingBatch);
	TEST_ASSERT_FALSE(reset.network.wifiProfiles[0].enabled);
	TEST_ASSERT_FALSE(reset.knx.enabled);
	TEST_ASSERT_FALSE(reset.web.enabled);
	TEST_ASSERT_FALSE(reset.web.securityProvisioned);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(domain::RelayState::Off),
		static_cast<std::uint8_t>(reset.relayChannels[0].configuredDefault));
}

void testFactoryResetPreservesIdentityAndErasesUserConfiguration()
{
	using namespace switch_actuator;
	SettingsFixture fixture{};
	setText(fixture.stored.productId.value, "FACTORY-PRODUCT");
	setText(fixture.stored.boardModel, "Factory Board");
	setText(fixture.stored.hardwareRevision, "2.1");
	setText(fixture.stored.deviceSerial, "FACTORY-0007");
	fixture.stored.deviceUuid = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
	setText(fixture.stored.manufacturingDate.iso8601, "2026-08-24");
	fixture.stored.manufacturingBatch = 42;
	fixture.stored.network.wifiProfiles[0].enabled = true;
	setText(fixture.stored.network.wifiProfiles[0].ssid, "User-Wifi");
	setText(fixture.stored.network.wifiProfiles[0].passphrase, "user-secret");
	fixture.stored.modbus.unitId = 77;
	fixture.stored.knx.enabled = true;
	fixture.stored.knx.individualAddress = 0x1101;
	fixture.stored.web.enabled = true;
	fixture.stored.web.securityProvisioned = true;
	fixture.stored.relayChannels[0].configuredDefault = domain::RelayState::On;
	fixture.stored.indicators.maximumBrightness = 12;
	const auto expectedIdentity = fixture.stored;
	auto service = configurationService(fixture);

	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(app::ConfigurationFactoryResetResult::Erased),
		static_cast<std::uint8_t>(service.factoryReset()));
	const auto &reset = service.active();
	const auto defaults = domain::makeSafeConfiguration();
	TEST_ASSERT_EQUAL_STRING(expectedIdentity.productId.value.data(), reset.productId.value.data());
	TEST_ASSERT_EQUAL_STRING(expectedIdentity.boardModel.data(), reset.boardModel.data());
	TEST_ASSERT_EQUAL_STRING(expectedIdentity.hardwareRevision.data(), reset.hardwareRevision.data());
	TEST_ASSERT_EQUAL_STRING(expectedIdentity.deviceSerial.data(), reset.deviceSerial.data());
	TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedIdentity.deviceUuid.data(), reset.deviceUuid.data(), reset.deviceUuid.size());
	TEST_ASSERT_EQUAL_STRING(expectedIdentity.manufacturingDate.iso8601.data(), reset.manufacturingDate.iso8601.data());
	TEST_ASSERT_EQUAL_UINT32(expectedIdentity.manufacturingBatch, reset.manufacturingBatch);
	TEST_ASSERT_EQUAL_STRING(defaults.network.hostName.data(), reset.network.hostName.data());
	TEST_ASSERT_FALSE(reset.network.wifiProfiles[0].enabled);
	TEST_ASSERT_EQUAL_STRING("", reset.network.wifiProfiles[0].ssid.data());
	TEST_ASSERT_EQUAL_STRING("", reset.network.wifiProfiles[0].passphrase.data());
	TEST_ASSERT_EQUAL_UINT8(defaults.modbus.unitId, reset.modbus.unitId);
	TEST_ASSERT_EQUAL_UINT32(defaults.modbus.baudRate, reset.modbus.baudRate);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(defaults.modbus.parity),
		static_cast<std::uint8_t>(reset.modbus.parity));
	TEST_ASSERT_FALSE(reset.knx.enabled);
	TEST_ASSERT_EQUAL_UINT16(defaults.knx.individualAddress, reset.knx.individualAddress);
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(defaults.relayChannels[0].restorePolicy),
		static_cast<std::uint8_t>(reset.relayChannels[0].restorePolicy));
	TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(defaults.relayChannels[0].configuredDefault),
		static_cast<std::uint8_t>(reset.relayChannels[0].configuredDefault));
	TEST_ASSERT_FALSE(reset.web.enabled);
	TEST_ASSERT_FALSE(reset.web.securityProvisioned);
	TEST_ASSERT_EQUAL_UINT8(defaults.indicators.maximumBrightness, reset.indicators.maximumBrightness);
	TEST_ASSERT_EQUAL_UINT8(defaults.indicators.maximumBuzzerDutyPercent,
		reset.indicators.maximumBuzzerDutyPercent);
	TEST_ASSERT_FALSE(fixture.eraseCalled);
}

int main()
{
	UNITY_BEGIN();
	RUN_TEST(testPersistentDiagnosticCountersTrackErrorsAndDirtyState);
	RUN_TEST(testPersistentDiagnosticFaultCountersCountIncidentsOnly);
	RUN_TEST(testPersistentNetworkFailuresCountStateTransitionsOnly);
	RUN_TEST(testUnsupportedEthernetAdapterIsUnavailableAndInert);
	RUN_TEST(testBoardDescriptorRejectsInconsistentEthernetCapability);
	RUN_TEST(testServiceModeRequiresPhysicalEntryAndExpires);
	RUN_TEST(testServiceModeSeparatesFieldAndFactoryOperations);
	RUN_TEST(testServiceResetPreservesIdentityAndErasesUserConfiguration);
	RUN_TEST(testFactoryResetPreservesIdentityAndErasesUserConfiguration);
	RUN_TEST(testVersionCompatibilityContract);
	RUN_TEST(testValidKnxDefaults);
	RUN_TEST(testKnxApplicationModelV1Contract);
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
	RUN_TEST(testWebSecurityResetPreservesIdentityAndErasesUsers);
	RUN_TEST(testWebRequestQueueBoundsCapacityAndResultOwnership);
	RUN_TEST(testWebCommandTrackerDetectsDuplicateMismatchAndExpiry);
	RUN_TEST(testBuildsProductionAndUnprovisionedDeviceIdentity);
	RUN_TEST(testRejectsInvalidDeviceIdentity);
	RUN_TEST(testProductionProfileLocksAfterSecurityProvisioning);
	RUN_TEST(testRejectsPartialManufacturingConfiguration);
	RUN_TEST(testRelaySafetyEventDecisionTable);
	RUN_TEST(testResolvesEveryRelayBootStateFailSafe);
	RUN_TEST(testHalContractsDispatchAndFailClosed);
	RUN_TEST(testModbusV1UartSettingsEncoding);
	RUN_TEST(testModbusV1HoldingWriteValidationIsAtomic);
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