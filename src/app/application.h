#pragma once

#include "configuration_service.h"
#include "wifi_management_service.h"
#include "web_event_journal.h"
#include "web_security_service.h"
#include "web_command_tracker.h"
#include "web_request_queue.h"
#include "diagnostics_service.h"
#include "lifecycle_supervisor.h"
#include "relay_command_queue.h"
#include "relay_command_service.h"
#include "relay_timer_service.h"
#include "scene_service.h"
#include "service_mode_service.h"
#include "switching_policy_service.h"
#include "../adapters/bsp/esp32_relay_output.h"
#include "../adapters/bsp/esp32_button_hal.h"
#include "../adapters/bsp/esp32_indicator_hal.h"
#include "../hal/Board.h"
#include "../adapters/button/button_adapter.h"
#include "../adapters/cli/cli_adapter.h"
#include "../adapters/configuration/json_configuration_source.h"
#include "../adapters/filesystem/littlefs_configuration_source.h"
#include "../adapters/indicators/status_indicator.h"
#include "../adapters/knx/knx_adapter.h"
#include "../adapters/modbus/esp32_modbus_serial_transport.h"
#include "../adapters/modbus/modbus_application_gateway.h"
#include "../adapters/modbus/modbus_configuration_gateway.h"
#include "../adapters/network/network_manager.h"
#include "../adapters/network/null_ethernet_adapter.h"
#include "../adapters/network/wifi_adapter.h"
#include "../adapters/nvs/nvs_settings_store.h"
#include "../adapters/nvs/nvs_web_security_store.h"
#include "../adapters/watchdog/esp32_task_watchdog.h"
#include "../adapters/web/web_server_adapter.h"
#include "../adapters/web/esp32_web_crypto.h"
#include "../domain/relay_policy.h"

#include <improv_wifi/serial_filter.h>

#include <cstdint>

namespace switch_actuator::app
{
enum class ApplicationInitializeResult : std::uint8_t
{
	Initialized,
	Degraded,
	RelayOutputFailure,
	IndicatorFailure,
	LifecycleFailure,
	ServiceFailure,
	ButtonFailure,
	CliFailure,
	WatchdogFailure,
	SecurityPolicyFailure,
	UnsupportedBoard
};

class Application final
{
public:
	Application() noexcept;

	[[nodiscard]] ApplicationInitializeResult initialize(std::uint32_t nowMs) noexcept;
	void update(std::uint32_t nowMs) noexcept;
	[[nodiscard]] bool isInitialized() const noexcept;

private:
	[[nodiscard]] static bool handleButtonEvent(const adapters::button::ButtonEvent &event, void *context) noexcept;
	[[nodiscard]] static std::uint32_t monotonicMilliseconds(void *context) noexcept;
	static void routeCliBytes(const std::uint8_t *data, std::size_t length, void *context) noexcept;
	static void routeProvisioningBytes(const std::uint8_t *data, std::size_t length, void *context) noexcept;
	[[nodiscard]] static bool extendModbusSnapshot(void *context,
															 adapters::modbus::RegisterMapSnapshot &snapshot) noexcept;
	[[nodiscard]] static adapters::modbus::WriteBatchResult handleModbusNonRelayWrite(
		void *context, const adapters::modbus::HoldingWriteBatch &batch) noexcept;
	[[nodiscard]] bool onButtonEvent(const adapters::button::ButtonEvent &event) noexcept;
	[[nodiscard]] bool performFactoryReset(std::uint32_t nowMs) noexcept;
	[[nodiscard]] static domain::ResetCategory resetCategory() noexcept;
	[[nodiscard]] bool applyRestorePlan(domain::ResetCategory resetCategory, std::uint32_t nowMs) noexcept;
	void handleWatchdogFailure(std::uint32_t nowMs) noexcept;
	void processRelayCommand(std::uint32_t nowMs) noexcept;
	void processWebRequest(std::uint32_t nowMs) noexcept;
	void publishWebStateEvents(std::uint32_t nowMs) noexcept;
	void updateDiagnostics(std::uint32_t nowMs) noexcept;
	void flushDiagnosticCounters(std::uint32_t nowMs, bool force) noexcept;

	DiagnosticsService diagnostics_{};
	LifecycleSupervisor lifecycle_;
	ServiceModeService serviceMode_{};
	adapters::bsp::Esp32RelayOutput relayOutput_;
	CommandArbiter commandArbiter_{};
	RelayCommandService relayService_;
	WebEventJournal webEventJournal_{};
	WebCommandTracker webCommandTracker_{};
	WebRequestQueue webRequestQueue_{};
	RelayCommandQueue commandQueue_{};
	SwitchingPolicyService switchingPolicy_;
	SceneService sceneService_;
	RelayTimerService relayTimerService_;
	adapters::nvs::NvsSettingsStore settingsStore_{};
	adapters::filesystem::LittleFsConfigurationSource defaultConfigurationSource_;
	ConfigurationService configurationService_;
	WifiManagementService wifiManagementService_{configurationService_};
	adapters::network::WifiAdapter wifiAdapter_{};
	adapters::network::NullEthernetAdapter ethernetAdapter_{};
	adapters::nvs::NvsWebSecurityStore webSecurityStore_{};
	adapters::web::Esp32WebCrypto webCrypto_{};
	WebSecurityService webSecurityService_{webSecurityStore_.port(), webCrypto_.port(),
		ports::IClock{monotonicMilliseconds, this}};
	adapters::network::NetworkManager network_{hal::board(),
		configurationService_, wifiManagementService_, wifiAdapter_, ethernetAdapter_.port(), Serial};
	hal::INetwork networkHal_{};
	adapters::bsp::Esp32RgbLedHal rgbLedHardware_;
	adapters::bsp::Esp32BuzzerHal buzzerHardware_;
	adapters::indicators::StatusIndicator statusIndicator_;
	adapters::bsp::Esp32ButtonHal buttonHardware_;
	adapters::button::ButtonAdapter button_;
	adapters::knx::KnxAdapter knx_;
	adapters::modbus::ModbusConfigurationGateway modbusConfigurationGateway_;
	adapters::modbus::ModbusApplicationGateway modbusApplicationGateway_;
	adapters::modbus::Esp32ModbusSerialTransport modbusSerialTransport_;
	hal::IUart uart_{};
	adapters::modbus::ModbusRtuAdapter modbusRtu_;
	adapters::cli::CliAdapter cli_;
	adapters::watchdog::Esp32TaskWatchdog watchdog_{};
	hal::IWatchdog watchdogHal_{watchdog_.hal()};
	adapters::web::WebServerAdapter webServer_{{&relayService_,
		&switchingPolicy_,
		&diagnostics_,
		&configurationService_,
		&wifiManagementService_,
		&webEventJournal_,
		networkHal_.status,
		networkHal_.control,
		modbusRtu_.controlPort(),
		webSecurityService_.port(),
		&webSecurityService_,
		&webCommandTracker_,
		&webRequestQueue_}};
	improv_wifi_busware::SerialFilter serialFilter_{};
	std::uint32_t lastRelayProcessAtMs_{0};
	std::uint32_t lastModbusPollAtMs_{0};
	std::uint32_t lastButtonUpdateAtMs_{0};
	std::uint32_t lastCliPollAtMs_{0};
	std::uint32_t lastIndicatorUpdateAtMs_{0};
	std::uint32_t lastDiagnosticsUpdateAtMs_{0};
	std::uint32_t lastDiagnosticCounterFlushAtMs_{0};
	std::uint32_t lastPublishedNetworkSequence_{0};
	std::uint32_t lastPublishedWifiScanSequence_{0};
	std::uint32_t lastPublishedConfigurationGeneration_{0};
	std::uint32_t diagnosticsEventSequence_{0};
	std::uint32_t restartRequestedAtMs_{0};
	bool restartPending_{false};
	bool modbusAvailable_{false};
	bool cliAvailable_{false};
	bool webAvailable_{false};
	bool initialized_{false};
};
}