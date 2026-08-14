#pragma once

#include "configuration_service.h"
#include "diagnostics_service.h"
#include "lifecycle_supervisor.h"
#include "relay_command_queue.h"
#include "relay_command_service.h"
#include "../adapters/bsp/esp32_relay_output.h"
#include "../adapters/button/button_adapter.h"
#include "../adapters/cli/cli_adapter.h"
#include "../adapters/configuration/json_configuration_source.h"
#include "../adapters/indicators/status_indicator.h"
#include "../adapters/knx/knx_adapter.h"
#include "../adapters/modbus/esp32_modbus_serial_transport.h"
#include "../adapters/modbus/modbus_application_gateway.h"
#include "../adapters/modbus/modbus_configuration_gateway.h"
#include "../adapters/nvs/nvs_settings_store.h"
#include "../adapters/watchdog/esp32_task_watchdog.h"
#include "../domain/relay_policy.h"

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
	RelayCommandService relayService_;
	RelayCommandQueue commandQueue_{};
	adapters::nvs::NvsSettingsStore settingsStore_{};
	adapters::configuration::JsonConfigurationSource defaultConfigurationSource_;
	ConfigurationService configurationService_;
	adapters::indicators::StatusIndicator statusIndicator_;
	adapters::button::ButtonAdapter button_;
	adapters::knx::KnxAdapter knx_;
	adapters::modbus::ModbusConfigurationGateway modbusConfigurationGateway_;
	adapters::modbus::ModbusApplicationGateway modbusApplicationGateway_;
	adapters::modbus::Esp32ModbusSerialTransport modbusSerialTransport_;
	adapters::modbus::ModbusRtuAdapter modbusRtu_;
	adapters::cli::CliAdapter cli_;
	adapters::watchdog::Esp32TaskWatchdog watchdog_{};
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