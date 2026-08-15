#pragma once

#include "configuration_service.h"
#include "diagnostics_service.h"
#include "lifecycle_supervisor.h"
#include "relay_command_queue.h"
#include "relay_command_service.h"
#include "relay_timer_service.h"
#include "scene_service.h"
#include "switching_policy_service.h"
#include "../adapters/bsp/esp32_relay_output.h"
#include "../adapters/bsp/waveshare_esp32s3_relay6ch.h"
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
#include "../adapters/nvs/nvs_settings_store.h"
#include "../adapters/watchdog/esp32_task_watchdog.h"
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
	WatchdogFailure
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
	[[nodiscard]] bool onButtonEvent(const adapters::button::ButtonEvent &event) noexcept;
	[[nodiscard]] bool performFactoryReset(std::uint32_t nowMs) noexcept;
	[[nodiscard]] static domain::ResetCategory resetCategory() noexcept;
	[[nodiscard]] bool applyRestorePlan(std::uint32_t nowMs) noexcept;
	void handleWatchdogFailure(std::uint32_t nowMs) noexcept;
	void processRelayCommand() noexcept;
	void updateDiagnostics(std::uint32_t nowMs) noexcept;

	DiagnosticsService diagnostics_{};
	LifecycleSupervisor lifecycle_;
	adapters::bsp::Esp32RelayOutput relayOutput_;
	CommandArbiter commandArbiter_{};
	RelayCommandService relayService_;
	RelayCommandQueue commandQueue_{};
	SwitchingPolicyService switchingPolicy_;
	SceneService sceneService_;
	RelayTimerService relayTimerService_;
	adapters::nvs::NvsSettingsStore settingsStore_{};
	adapters::filesystem::LittleFsConfigurationSource defaultConfigurationSource_;
	ConfigurationService configurationService_;
	adapters::network::NetworkManager network_{adapters::bsp::waveshareEsp32S3Relay6Ch, configurationService_, Serial};
	adapters::indicators::StatusIndicator statusIndicator_;
	adapters::button::ButtonAdapter button_;
	adapters::knx::KnxAdapter knx_;
	adapters::modbus::ModbusConfigurationGateway modbusConfigurationGateway_;
	adapters::modbus::ModbusApplicationGateway modbusApplicationGateway_;
	adapters::modbus::Esp32ModbusSerialTransport modbusSerialTransport_;
	adapters::modbus::ModbusRtuAdapter modbusRtu_;
	adapters::cli::CliAdapter cli_;
	adapters::watchdog::Esp32TaskWatchdog watchdog_{};
	improv_wifi_busware::SerialFilter serialFilter_{};
	std::uint32_t lastRelayProcessAtMs_{0};
	std::uint32_t lastModbusPollAtMs_{0};
	std::uint32_t lastButtonUpdateAtMs_{0};
	std::uint32_t lastCliPollAtMs_{0};
	std::uint32_t lastIndicatorUpdateAtMs_{0};
	std::uint32_t lastDiagnosticsUpdateAtMs_{0};
	bool modbusAvailable_{false};
	bool cliAvailable_{false};
	bool initialized_{false};
};
}