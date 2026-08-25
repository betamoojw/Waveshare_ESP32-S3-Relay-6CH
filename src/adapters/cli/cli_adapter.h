#pragma once

#include "../../app/diagnostics_service.h"
#include "../../app/configuration_service.h"
#include "../../app/lifecycle_supervisor.h"
#include "../../app/service_mode_service.h"
#include "../../app/relay_command_queue.h"
#include "../../app/relay_command_service.h"
#include "../../app/switching_policy_service.h"
#include "../../app/web_security_service.h"
#include "../../ports/clock_port.h"
#include "../../domain/deployment_profile.h"
#include "../../hal/BoardDescriptor.h"
#include "../../ports/configuration_file_port.h"
#include "../../ports/modbus_rtu_control_port.h"
#include "../button/button_adapter.h"
#include "../indicators/status_indicator.h"
#include "../network/network_manager.h"

#include <Arduino.h>
#include <embedded_cli.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace switch_actuator::adapters::cli
{
enum class CliInitializeResult : std::uint8_t
{
	Initialized,
	InvalidDependencies,
	InsufficientBuffer,
	BindingFailure
};

enum class CliPollResult : std::uint8_t
{
	Processed,
	Idle,
	NotInitialized
};

struct CliDependencies final
{
	Stream *stream{nullptr};
	app::SwitchingPolicyService *switchingPolicy{nullptr};
	const app::RelayCommandService *relayService{nullptr};
	app::LifecycleSupervisor *lifecycleSupervisor{nullptr};
	app::DiagnosticsService *diagnostics{nullptr};
	app::ConfigurationService *configurationService{nullptr};
	app::WebSecurityService *webSecurityService{nullptr};
	app::ServiceModeService *serviceModeService{nullptr};
	const hal::BoardDescriptor *board{nullptr};
	ports::ConfigurationFilePort configurationFile{};
	indicators::StatusIndicator *statusIndicator{nullptr};
	const button::ButtonAdapter *button{nullptr};
	network::NetworkManager *networkManager{nullptr};
	ports::ModbusRtuControlPort modbus{};
	ports::IClock clock{};
	domain::DeploymentProfile deploymentProfile{domain::DeploymentProfile::Development};
};

class CliAdapter final
{
public:
	explicit CliAdapter(CliDependencies dependencies) noexcept;

	[[nodiscard]] CliInitializeResult initialize() noexcept;
	[[nodiscard]] CliPollResult poll() noexcept;
	void ingest(std::uint8_t value) noexcept;
	[[nodiscard]] bool isInitialized() const noexcept;

private:
	static constexpr std::size_t bufferSize{4096};
	static constexpr std::uint16_t maximumBindings{26};

	static void writeCharacter(EmbeddedCli *cli, char character) noexcept;
	static void unknownCommand(EmbeddedCli *cli, CliCommand *command) noexcept;
	static void versionCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void statusCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void setLogLevelCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void getRelayCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void setRelayCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void toggleRelayCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void getIndicatorCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void setRgbCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void buzzerCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void getButtonCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void getModbusRoleCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void setModbusRoleCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void getModbusConfigCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void setModbusConfigCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void modbusReadHoldingCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void modbusWriteRegisterCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void getKnxCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void setKnxCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void setKnxChannelCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void setWifiCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void loadConfigCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void storeConfigCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void rebootCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void provisionWebCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void manufacturingTestCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;
	static void serviceCommand(EmbeddedCli *cli, char *arguments, void *context) noexcept;

	[[nodiscard]] bool dependenciesValid() const noexcept;
	[[nodiscard]] bool mutatingCommandAllowed() const noexcept;
	[[nodiscard]] bool enqueueRelayBatch(const app::RelayCommandBatch &batch) noexcept;
	[[nodiscard]] std::uint32_t nextCorrelationId() noexcept;
	void print(const char *message) noexcept;
	void printStatus() noexcept;
	void printRelay(std::uint8_t channel) noexcept;
	void printIndicator() noexcept;
	void printButton() noexcept;
	void printKnxGeneral() noexcept;
	void printKnxChannel(std::uint8_t channel) noexcept;
	void handleSetRelay(char *arguments) noexcept;
	void handleToggleRelay(char *arguments) noexcept;
	void handleSetRgb(char *arguments) noexcept;
	void handleBuzzer(char *arguments) noexcept;
	void handleSetModbusRole(char *arguments) noexcept;
	void handleGetModbusConfig(char *arguments) noexcept;
	void handleSetModbusConfig(char *arguments) noexcept;
	void handleModbusReadHolding(char *arguments) noexcept;
	void handleModbusWriteRegister(char *arguments) noexcept;
	void handleGetKnx(char *arguments) noexcept;
	void handleSetKnx(char *arguments) noexcept;
	void handleSetKnxChannel(char *arguments) noexcept;
	void handleSetWifi(char *arguments) noexcept;
	void handleLoadConfig(char *arguments) noexcept;
	void handleStoreConfig(char *arguments) noexcept;
	void commitConfiguration(const domain::Configuration &configuration) noexcept;
	void handleReboot(char *arguments) noexcept;
	void handleProvisionWeb(char *arguments) noexcept;
	void handleManufacturingTest(char *arguments) noexcept;
	void handleService(char *arguments) noexcept;
	void printServiceIdentity() noexcept;
	void printServiceDiagnostics() noexcept;
	[[nodiscard]] bool authorizeServiceOperation(app::ServiceModeOperation operation) noexcept;
	void printManufacturingSnapshot() noexcept;

	CliDependencies dependencies_;
	alignas(CLI_UINT) std::array<CLI_UINT, BYTES_TO_CLI_UINTS(bufferSize)> buffer_{};
	EmbeddedCli *cli_{nullptr};
	std::uint32_t correlationId_{0};
	bool initialized_{false};
};
}