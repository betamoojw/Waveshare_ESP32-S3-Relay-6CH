#include "application.h"

#include "../adapters/bsp/waveshare_esp32s3_relay6ch.h"

#include <Arduino.h>
#include <esp_system.h>

#include <string_view>

namespace switch_actuator::app
{
namespace
{
constexpr std::uint32_t relayProcessIntervalMs{10};
constexpr std::uint32_t modbusPollIntervalMs{2};
constexpr std::uint32_t buttonUpdateIntervalMs{10};
constexpr std::uint32_t cliPollIntervalMs{10};
constexpr std::uint32_t indicatorUpdateIntervalMs{20};
constexpr std::uint32_t diagnosticsUpdateIntervalMs{1000};
constexpr std::size_t maximumSerialInputBytesPerUpdate{64};

#ifdef FIRMWARE_VERSION
constexpr std::string_view firmwareVersion{FIRMWARE_VERSION};
#else
constexpr std::string_view firmwareVersion{"1.00"};
#endif

#ifdef PIOENV
constexpr std::string_view buildId{PIOENV};
#else
constexpr std::string_view buildId{"local"};
#endif

#ifdef ENABLE_MUTATING_CLI_COMMANDS
constexpr bool mutatingCliCommandsEnabled{true};
#else
constexpr bool mutatingCliCommandsEnabled{false};
#endif
}

Application::Application() noexcept
	: lifecycle_{diagnostics_.lifecycleEventSink()},
	  relayOutput_{adapters::bsp::waveshareEsp32S3Relay6Ch},
	  relayService_{relayOutput_.port(), diagnostics_.relayEventSink(), commandArbiter_},
	  switchingPolicy_{commandQueue_, commandArbiter_},
	  sceneService_{switchingPolicy_, relayService_},
	  relayTimerService_{switchingPolicy_},
	  defaultConfigurationSource_{adapters::configuration::embeddedDefaultConfigurationJson()},
	  configurationService_{settingsStore_.port()},
	  network_{adapters::bsp::waveshareEsp32S3Relay6Ch, configurationService_, Serial},
	  statusIndicator_{adapters::bsp::waveshareEsp32S3Relay6Ch},
	  button_{adapters::bsp::waveshareEsp32S3Relay6Ch, handleButtonEvent, this},
	  knx_{{&switchingPolicy_, &relayService_, &diagnostics_, ports::ClockPort{monotonicMilliseconds, this}, network_.statusPort()}},
	  modbusConfigurationGateway_{{&configurationService_,
		&lifecycle_,
		&diagnostics_,
		ports::ClockPort{monotonicMilliseconds, this}}},
	  modbusApplicationGateway_{{&switchingPolicy_,
		&relayService_,
		&configurationService_,
		&lifecycle_,
		&diagnostics_,
		extendModbusSnapshot,
		this,
		adapters::modbus::ModbusConfigurationGateway::handle,
		&modbusConfigurationGateway_}},
	  modbusSerialTransport_{Serial1, adapters::bsp::waveshareEsp32S3Relay6Ch},
	  modbusRtu_{{adapters::modbus::Esp32ModbusSerialTransport::read,
		adapters::modbus::Esp32ModbusSerialTransport::write,
		&modbusSerialTransport_,
		adapters::modbus::ModbusApplicationGateway::provideSnapshot,
		&modbusApplicationGateway_,
		adapters::modbus::ModbusApplicationGateway::handleWriteBatch,
		&modbusApplicationGateway_,
		&diagnostics_}},
	  cli_{{&Serial,
		&switchingPolicy_,
		&relayService_,
		&lifecycle_,
		&diagnostics_,
		&configurationService_,
		&statusIndicator_,
		&button_,
		&network_,
		modbusRtu_.controlPort(),
		ports::ClockPort{monotonicMilliseconds, this},
		mutatingCliCommandsEnabled}}
{
}

ApplicationInitializeResult Application::initialize(const std::uint32_t nowMs) noexcept
{
	initialized_ = false;
	modbusAvailable_ = false;
	cliAvailable_ = false;
	commandQueue_.clear();
	relayTimerService_.cancelAll();
	sceneService_.disable();
	if (resetCategory() == domain::ResetCategory::Watchdog)
	{
		static_cast<void>(diagnostics_.recordFault(domain::FaultCode::WatchdogReset,
			domain::FaultSeverity::Warning,
			nowMs));
	}
	static_cast<void>(diagnostics_.setIdentity(firmwareVersion,
		buildId,
		adapters::bsp::waveshareEsp32S3Relay6Ch.model,
		adapters::bsp::waveshareEsp32S3Relay6Ch.hardwareRevision));
	if (lifecycle_.initialize(nowMs) != LifecycleResult::Applied)
	{
		return ApplicationInitializeResult::LifecycleFailure;
	}
	if (relayOutput_.initialize() != adapters::bsp::RelayOutputResult::Applied)
	{
		static_cast<void>(diagnostics_.recordFault(domain::FaultCode::RelayOutputFailure,
			domain::FaultSeverity::Critical,
			nowMs));
		static_cast<void>(lifecycle_.enterFault(LifecycleReason::CriticalFault, nowMs));
		return ApplicationInitializeResult::RelayOutputFailure;
	}
	if (statusIndicator_.initialize() != adapters::indicators::IndicatorResult::Applied)
	{
		return ApplicationInitializeResult::IndicatorFailure;
	}
	if (lifecycle_.beginConfiguration(nowMs) != LifecycleResult::Applied)
	{
		return ApplicationInitializeResult::LifecycleFailure;
	}

	const auto nvsReady = settingsStore_.initialize() == adapters::nvs::NvsInitializeResult::Initialized;
	configurationService_.setDefaultSource(defaultConfigurationSource_.port());
	static_cast<void>(configurationService_.initialize());
	const auto configurationValid = configurationService_.hasValidActiveConfiguration();
	const auto settingsLoadResult = configurationService_.lastLoadResult();
	const auto persistenceHealthy = nvsReady && settingsLoadResult != ports::SettingsLoadResult::Corrupt &&
										settingsLoadResult != ports::SettingsLoadResult::IoFailure;
	diagnostics_.updateConfiguration(configurationValid,
		configurationService_.active().generation,
		configurationService_.lastLoadResult(),
		configurationService_.lastSaveResult());
	if (!configurationValid)
	{
		static_cast<void>(diagnostics_.recordFault(domain::FaultCode::InvalidConfiguration,
			domain::FaultSeverity::Warning,
			nowMs));
	}
	if (!persistenceHealthy)
	{
		static_cast<void>(diagnostics_.recordFault(domain::FaultCode::SettingsLoadFailure,
			domain::FaultSeverity::Warning,
			nowMs));
	}
	else
	{
		static_cast<void>(diagnostics_.clearFault(domain::FaultCode::SettingsLoadFailure));
	}

	if (relayService_.initialize(nowMs) != RelayServiceInitializeResult::Initialized)
	{
		static_cast<void>(diagnostics_.recordFault(domain::FaultCode::RelayOutputFailure,
			domain::FaultSeverity::Critical,
			nowMs));
		static_cast<void>(lifecycle_.enterFault(LifecycleReason::CriticalFault, nowMs));
		return ApplicationInitializeResult::ServiceFailure;
	}
	if (button_.initialize(nowMs) != adapters::button::ButtonInitializeResult::Initialized)
	{
		static_cast<void>(lifecycle_.enterFault(LifecycleReason::CriticalFault, nowMs));
		return ApplicationInitializeResult::ButtonFailure;
	}
	Serial.begin(115200);
	cliAvailable_ = cli_.initialize() == adapters::cli::CliInitializeResult::Initialized;
	serialFilter_.setSinks(routeCliBytes, this, routeProvisioningBytes, this);
	network_.initialize(nowMs);
	const auto knxResult = knx_.initialize(configurationService_.active().knx, nowMs);
	const auto &modbusConfiguration = configurationService_.active().modbus;
	const adapters::modbus::ModbusRtuConfiguration rtuConfiguration{
		modbusConfiguration.unitId,
		modbusConfiguration.baudRate,
		modbusConfiguration.parity,
		modbusConfiguration.dataBits,
		modbusConfiguration.stopBits,
	};
	if (modbusSerialTransport_.initialize(rtuConfiguration) == adapters::modbus::SerialTransportInitializeResult::Initialized &&
		modbusRtu_.initialize(rtuConfiguration) == adapters::modbus::ModbusInitializeResult::Initialized)
	{
		modbusAvailable_ = true;
		static_cast<void>(diagnostics_.clearFault(domain::FaultCode::ModbusTransportError));
	}
	else
	{
		static_cast<void>(diagnostics_.recordFault(domain::FaultCode::ModbusTransportError,
			domain::FaultSeverity::Warning,
			nowMs));
	}
	if (!applyRestorePlan(nowMs))
	{
		static_cast<void>(lifecycle_.enterFault(LifecycleReason::CriticalFault, nowMs));
		return ApplicationInitializeResult::ServiceFailure;
	}

	lastRelayProcessAtMs_ = nowMs;
	lastModbusPollAtMs_ = nowMs;
	lastButtonUpdateAtMs_ = nowMs;
	lastCliPollAtMs_ = nowMs;
	lastIndicatorUpdateAtMs_ = nowMs;
	lastDiagnosticsUpdateAtMs_ = nowMs;
	initialized_ = true;
	if (!configurationValid || !persistenceHealthy || !modbusAvailable_ || !cliAvailable_ ||
		knxResult == adapters::knx::KnxInitializeResult::Unavailable)
	{
		static_cast<void>(lifecycle_.enterDegraded(
			configurationValid ? LifecycleReason::AdapterUnavailable : LifecycleReason::ConfigurationInvalid,
			nowMs));
		statusIndicator_.setBusDegraded(true);
		if (watchdog_.initialize() == adapters::watchdog::WatchdogInitializeResult::RegistrationFailure)
		{
			handleWatchdogFailure(nowMs);
			return ApplicationInitializeResult::WatchdogFailure;
		}
		return ApplicationInitializeResult::Degraded;
	}

	static_cast<void>(lifecycle_.enterOperational(nowMs));
	if (watchdog_.initialize() == adapters::watchdog::WatchdogInitializeResult::RegistrationFailure)
	{
		handleWatchdogFailure(nowMs);
		return ApplicationInitializeResult::WatchdogFailure;
	}
	return ApplicationInitializeResult::Initialized;
}

void Application::update(const std::uint32_t nowMs) noexcept
{
	if (!initialized_)
	{
		return;
	}
	static_cast<void>(relayTimerService_.update(nowMs));
	if (nowMs - lastRelayProcessAtMs_ >= relayProcessIntervalMs)
	{
		lastRelayProcessAtMs_ = nowMs;
		processRelayCommand();
	}
	if (modbusAvailable_ && nowMs - lastModbusPollAtMs_ >= modbusPollIntervalMs)
	{
		lastModbusPollAtMs_ = nowMs;
		const auto pollResult = modbusRtu_.poll();
		if (pollResult == adapters::modbus::ModbusPollResult::ProtocolError)
		{
			static_cast<void>(diagnostics_.recordFault(domain::FaultCode::ModbusProtocolError,
				domain::FaultSeverity::Warning,
				nowMs));
		}
		else if (pollResult == adapters::modbus::ModbusPollResult::TransportError)
		{
			modbusAvailable_ = false;
			statusIndicator_.setBusDegraded(true);
			if (lifecycle_.state() == LifecycleState::Operational)
			{
				static_cast<void>(lifecycle_.enterDegraded(LifecycleReason::AdapterUnavailable, nowMs));
			}
		}
	}
	if (nowMs - lastButtonUpdateAtMs_ >= buttonUpdateIntervalMs)
	{
		lastButtonUpdateAtMs_ = nowMs;
		static_cast<void>(button_.update(nowMs));
	}
	if (cliAvailable_ && nowMs - lastCliPollAtMs_ >= cliPollIntervalMs)
	{
		lastCliPollAtMs_ = nowMs;
		std::size_t received{0};
		while (received < maximumSerialInputBytesPerUpdate && Serial.available() > 0)
		{
			const auto value = Serial.read();
			if (value < 0)
			{
				break;
			}
			const auto byte = static_cast<std::uint8_t>(value);
			serialFilter_.feed(&byte, 1);
			++received;
		}
		static_cast<void>(cli_.poll());
	}
	network_.update(nowMs);
	if (nowMs - lastIndicatorUpdateAtMs_ >= indicatorUpdateIntervalMs)
	{
		lastIndicatorUpdateAtMs_ = nowMs;
		static_cast<void>(statusIndicator_.update(nowMs));
	}
	if (nowMs - lastDiagnosticsUpdateAtMs_ >= diagnosticsUpdateIntervalMs)
	{
		lastDiagnosticsUpdateAtMs_ = nowMs;
		updateDiagnostics(nowMs);
	}
	static_cast<void>(knx_.poll());
	if (watchdog_.feed() != adapters::watchdog::WatchdogFeedResult::Fed)
	{
		handleWatchdogFailure(nowMs);
	}
}

bool Application::isInitialized() const noexcept
{
	return initialized_;
}

void Application::routeCliBytes(const std::uint8_t *const data, const std::size_t length, void *const context) noexcept
{
	if (context == nullptr || data == nullptr)
	{
		return;
	}

	auto &application = *static_cast<Application *>(context);
	for (std::size_t index = 0; index < length; ++index)
	{
		application.cli_.ingest(data[index]);
	}
}

void Application::routeProvisioningBytes(const std::uint8_t *const data,
	const std::size_t length,
	void *const context) noexcept
{
	if (context != nullptr)
	{
		static_cast<Application *>(context)->network_.ingestProvisioning(data, length);
	}
}

bool Application::handleButtonEvent(const adapters::button::ButtonEvent &event, void *const context) noexcept
{
	return context != nullptr && static_cast<Application *>(context)->onButtonEvent(event);
}

bool Application::onButtonEvent(const adapters::button::ButtonEvent &event) noexcept
{
	switch (event.type)
	{
	case adapters::button::ButtonEventType::IdentifyRequested:
		statusIndicator_.notifyCommand(adapters::indicators::CommandFeedback::Accepted, event.occurredAtMs);
		return true;
	case adapters::button::ButtonEventType::CommissioningRequested:
		statusIndicator_.setCommissioning(true);
		cli_.setMaintenanceAuthorized(true);
		return true;
	case adapters::button::ButtonEventType::FactoryResetArmed:
		statusIndicator_.notifyCommand(adapters::indicators::CommandFeedback::Rejected, event.occurredAtMs);
		return true;
	case adapters::button::ButtonEventType::FactoryResetRequested:
		return performFactoryReset(event.occurredAtMs);
	default:
		return false;
	}
}

bool Application::performFactoryReset(const std::uint32_t nowMs) noexcept
{
	cli_.setMaintenanceAuthorized(false);
	for (std::uint8_t channel = 0; channel < domain::relayChannelCount; ++channel)
	{
		const auto result = relayService_.setSafetyLockout(
			domain::RelayChannelId{channel}, true, static_cast<std::uint32_t>(0xFFFF'FF00U + channel), nowMs);
		if (result.status != RelayCommandStatus::Accepted)
		{
			static_cast<void>(diagnostics_.recordFault(domain::FaultCode::RelayOutputFailure,
				domain::FaultSeverity::Critical,
				nowMs));
			statusIndicator_.setCriticalFault(true);
			return false;
		}
	}

	commandQueue_.clear();
	if (configurationService_.factoryReset() != ConfigurationFactoryResetResult::Erased)
	{
		static_cast<void>(diagnostics_.recordFault(domain::FaultCode::SettingsSaveFailure,
			domain::FaultSeverity::Warning,
			nowMs));
		statusIndicator_.notifyCommand(adapters::indicators::CommandFeedback::Rejected, nowMs);
		return false;
	}

	static_cast<void>(diagnostics_.clearFault(domain::FaultCode::SettingsSaveFailure));
	diagnostics_.updateConfiguration(false,
		configurationService_.active().generation,
		configurationService_.lastLoadResult(),
		configurationService_.lastSaveResult());
	const auto restartResult = lifecycle_.requestRestart(nowMs);
	if (restartResult == LifecycleResult::InvalidTransition || restartResult == LifecycleResult::InvalidEventSink)
	{
		return false;
	}
	statusIndicator_.notifyCommand(adapters::indicators::CommandFeedback::Accepted, nowMs);
	return true;
}

domain::ResetCategory Application::resetCategory() noexcept
{
	switch (esp_reset_reason())
	{
	case ESP_RST_POWERON:
		return domain::ResetCategory::PowerOn;
	case ESP_RST_SW:
		return domain::ResetCategory::ControlledRestart;
	case ESP_RST_BROWNOUT:
		return domain::ResetCategory::Brownout;
	case ESP_RST_INT_WDT:
	case ESP_RST_TASK_WDT:
	case ESP_RST_WDT:
		return domain::ResetCategory::Watchdog;
	case ESP_RST_PANIC:
		return domain::ResetCategory::Panic;
	default:
		return domain::ResetCategory::Unknown;
	}
}

bool Application::applyRestorePlan(const std::uint32_t nowMs) noexcept
{
	domain::RelayRestorePlan plan{};
	const domain::RelayRestoreContext context{resetCategory(), {}, false, 1, nowMs};
	if (domain::makeRelayRestorePlan(configurationService_.active().relayChannels, context, plan) !=
		domain::RelayRestorePlanResult::Planned)
	{
		return false;
	}
	return relayService_.executeBatch(plan.commands.data(), plan.count).status == RelayCommandStatus::Accepted;
}

void Application::handleWatchdogFailure(const std::uint32_t nowMs) noexcept
{
	for (std::uint8_t channel = 0; channel < domain::relayChannelCount; ++channel)
	{
		static_cast<void>(relayService_.setSafetyLockout(
			domain::RelayChannelId{channel}, true, static_cast<std::uint32_t>(0xFFFF'FE00U + channel), nowMs));
	}
	commandQueue_.clear();
	static_cast<void>(diagnostics_.recordFault(domain::FaultCode::TaskWatchdogFailure,
		domain::FaultSeverity::Critical,
		nowMs));
	statusIndicator_.setCriticalFault(true);
	if (lifecycle_.state() != LifecycleState::Fault && lifecycle_.state() != LifecycleState::Restarting)
	{
		static_cast<void>(lifecycle_.enterFault(LifecycleReason::CriticalFault, nowMs));
	}
	initialized_ = false;
}

void Application::processRelayCommand() noexcept
{
	RelayCommandBatchResult result{};
	if (!commandQueue_.processNext(relayService_, result))
	{
		return;
	}
	if (result.status == RelayCommandStatus::Accepted)
	{
		diagnostics_.recordCommandAccepted();
		return;
	}
	diagnostics_.recordCommandRejected();
}

void Application::updateDiagnostics(const std::uint32_t nowMs) noexcept
{
	diagnostics_.updateRuntime(nowMs, ESP.getMinFreeHeap(), watchdog_.isHealthy());
}

std::uint32_t Application::monotonicMilliseconds(void *) noexcept
{
	return millis();
}

bool Application::extendModbusSnapshot(void *, adapters::modbus::RegisterMapSnapshot &snapshot) noexcept
{
	snapshot.softwareVersion = 100;
	snapshot.uartEncodedSettingsAvailable = false;
	return true;
}
}