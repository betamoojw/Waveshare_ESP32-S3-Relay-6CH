#include "application.h"
#include "../adapters/logging/logger_adapter.h"
#include "../domain/version_compatibility.h"

#include <Arduino.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <esp_flash_encrypt.h>
#include <esp_secure_boot.h>

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
constexpr std::uint32_t diagnosticCounterFlushIntervalMs{60000};
constexpr std::uint32_t minimumFreeInternalHeapBytes{64U * 1024U};
constexpr std::uint32_t minimumLargestInternalHeapBlockBytes{32U * 1024U};
constexpr std::size_t maximumSerialInputBytesPerUpdate{64};
constexpr std::uint32_t controlledRestartDrainMs{500};
constexpr std::string_view productName{"Switch Actuator 6CH"};

#ifdef PIOENV
constexpr std::string_view buildId{PIOENV};
#else
constexpr std::string_view buildId{"local"};
#endif

}

Application::Application() noexcept
	: lifecycle_{diagnostics_.lifecycleEventSink()},
	  relayOutput_{hal::board()},
	  relayService_{relayOutput_.hal(), diagnostics_.relayEventSink(), commandArbiter_},
	  switchingPolicy_{commandQueue_, commandArbiter_},
	  sceneService_{switchingPolicy_, relayService_},
	  relayTimerService_{switchingPolicy_},
	  defaultConfigurationSource_{adapters::configuration::embeddedDefaultConfigurationJson()},
	  configurationService_{settingsStore_.port()},
	  wifiManagementService_{configurationService_},
	  network_{hal::board(), configurationService_, wifiManagementService_, wifiAdapter_, ethernetAdapter_.port(), Serial},
	  networkHal_{network_.hal()},
	  rgbLedHardware_{hal::board()},
	  buzzerHardware_{hal::board()},
	  statusIndicator_{rgbLedHardware_.hal(), buzzerHardware_.hal()},
	  buttonHardware_{hal::board()},
	  button_{buttonHardware_.hal(), handleButtonEvent, this},
	  knx_{{&switchingPolicy_, &relayService_, &diagnostics_, ports::IClock{monotonicMilliseconds, this}, networkHal_.status}},
	  modbusConfigurationGateway_{{&configurationService_,
		&lifecycle_,
		&diagnostics_,
		ports::IClock{monotonicMilliseconds, this}}},
	  modbusApplicationGateway_{{&switchingPolicy_,
		&relayService_,
		&configurationService_,
		&lifecycle_,
		&diagnostics_,
		extendModbusSnapshot,
		this,
		handleModbusNonRelayWrite,
		this}},
	  modbusSerialTransport_{Serial1, hal::board()},
	  uart_{modbusSerialTransport_.hal()},
	  modbusRtu_{{uart_.read,
		uart_.write,
		uart_.context,
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
		&webSecurityService_,
		&serviceMode_,
		&hal::board(),
		defaultConfigurationSource_.filePort(),
		&statusIndicator_,
		&button_,
		&network_,
		modbusRtu_.controlPort(),
		ports::IClock{monotonicMilliseconds, this},
		domain::deploymentProfile}}
{
}

ApplicationInitializeResult Application::initialize(const std::uint32_t nowMs) noexcept
{
	adapters::logging::LoggerAdapter::instance().initialize(domain::deploymentProfile);
	LOG_INFO("application", "startup profile=%s", domain::deploymentProfileName(domain::deploymentProfile).data());
	initialized_ = false;
	static_cast<void>(serviceMode_.exit());
	modbusAvailable_ = false;
	cliAvailable_ = false;
	webAvailable_ = false;
	commandQueue_.clear();
	webEventJournal_.clear();
	webCommandTracker_.clear();
	webRequestQueue_.clear();
	relayTimerService_.cancelAll();
	sceneService_.disable();
	if (!hal::supportsRelayCount(hal::board(), domain::relayChannelCount))
	{
		LOG_FATAL("application", "unsupported board relay count");
		return ApplicationInitializeResult::UnsupportedBoard;
	}
	const auto startupResetCategory = resetCategory();
	LOG_DEBUG("application", "reset category=%u", static_cast<unsigned int>(startupResetCategory));
	if (startupResetCategory == domain::ResetCategory::Watchdog)
	{
		static_cast<void>(diagnostics_.recordFault(domain::FaultCode::WatchdogReset,
			domain::FaultSeverity::Warning,
			nowMs));
	}
	else if (startupResetCategory == domain::ResetCategory::Brownout)
	{
		static_cast<void>(diagnostics_.recordFault(domain::FaultCode::BrownoutReset,
			domain::FaultSeverity::Warning,
			nowMs));
	}
	if (lifecycle_.initialize(nowMs) != LifecycleResult::Applied)
	{
		LOG_FATAL("lifecycle", "initialization failed");
		return ApplicationInitializeResult::LifecycleFailure;
	}
	if (relayOutput_.initialize() != adapters::bsp::RelayOutputResult::Applied)
	{
		static_cast<void>(diagnostics_.recordFault(domain::FaultCode::RelayOutputFailure,
			domain::FaultSeverity::Critical,
			nowMs));
		static_cast<void>(lifecycle_.enterFault(LifecycleReason::CriticalFault, nowMs));
		LOG_FATAL("relay", "output initialization failed");
		return ApplicationInitializeResult::RelayOutputFailure;
	}
#if SWITCH_ACTUATOR_REQUIRE_SECURE_BOOT || SWITCH_ACTUATOR_REQUIRE_FLASH_ENCRYPTION
	const auto secureBootReady = !SWITCH_ACTUATOR_REQUIRE_SECURE_BOOT || esp_secure_boot_enabled();
	const auto flashEncryptionReady = !SWITCH_ACTUATOR_REQUIRE_FLASH_ENCRYPTION || esp_flash_encryption_enabled();
	if (!secureBootReady || !flashEncryptionReady)
	{
		static_cast<void>(diagnostics_.recordFault(domain::FaultCode::SecurityPolicyFailure,
			domain::FaultSeverity::Critical, nowMs));
		static_cast<void>(lifecycle_.enterFault(LifecycleReason::CriticalFault, nowMs));
		LOG_FATAL("security", "required hardware security state missing");
		return ApplicationInitializeResult::SecurityPolicyFailure;
	}
#endif
	if (statusIndicator_.initialize() != adapters::indicators::IndicatorResult::Applied)
	{
		LOG_ERROR("indicator", "initialization failed");
		return ApplicationInitializeResult::IndicatorFailure;
	}
	if (lifecycle_.beginConfiguration(nowMs) != LifecycleResult::Applied)
	{
		LOG_FATAL("lifecycle", "configuration transition failed");
		return ApplicationInitializeResult::LifecycleFailure;
	}

	const auto fileSystemReady =
		defaultConfigurationSource_.initialize() == adapters::filesystem::LittleFsInitializeResult::Initialized;
	const auto nvsReady = settingsStore_.initialize() == adapters::nvs::NvsInitializeResult::Initialized;
	const auto diagnosticCounters = settingsStore_.beginDiagnosticCounters(startupResetCategory);
	diagnostics_.setPersistentCounters(diagnosticCounters.counters);
	diagnostics_.updateBoot(diagnosticCounters.counters.bootCount, startupResetCategory);
	lastDiagnosticCounterFlushAtMs_ = nowMs;
	if (!diagnosticCounters.persisted) diagnostics_.recordStorageFailure();
	configurationService_.setDefaultSource(defaultConfigurationSource_.port());
	static_cast<void>(configurationService_.initialize());
	const auto configurationValid = configurationService_.hasValidActiveConfiguration();
	if (configurationValid)
	{
		std::array<std::uint8_t, domain::macAddressSize> macAddress{};
		if (esp_read_mac(macAddress.data(), ESP_MAC_BASE) == ESP_OK)
		{
			const auto &configuration = configurationService_.active();
			const domain::DeviceIdentitySource source{
				configuration.productId.value.data(),
				productName,
				hal::board().model,
				hal::board().hardwareRevision,
				domain::compatibility::firmware,
				configuration.deviceSerial.data(),
				configuration.deviceUuid,
				macAddress,
				configuration.manufacturingDate.iso8601.data(),
				configuration.manufacturingBatch,
			};
			if (const auto identity = domain::makeDeviceIdentity(source))
			{
				static_cast<void>(diagnostics_.setIdentity(*identity, buildId));
			}
		}
	}
	const auto settingsLoadResult = configurationService_.lastLoadResult();
	const auto persistenceHealthy = nvsReady && settingsLoadResult != ports::SettingsLoadResult::Corrupt &&
										settingsLoadResult != ports::SettingsLoadResult::IoFailure;
	diagnostics_.updateStorage(fileSystemReady, nvsReady, persistenceHealthy);
	diagnostics_.updateConfiguration(configurationValid,
		configurationService_.active().generation,
		configurationService_.lastLoadResult(),
		configurationService_.lastSaveResult());
	if (!configurationValid)
	{
		LOG_WARNING("configuration", "active configuration invalid");
		static_cast<void>(diagnostics_.recordFault(domain::FaultCode::InvalidConfiguration,
			domain::FaultSeverity::Warning,
			nowMs));
	}
	if (!persistenceHealthy)
	{
		LOG_ERROR("storage", "settings persistence unavailable");
		static_cast<void>(diagnostics_.recordFault(domain::FaultCode::SettingsLoadFailure,
			domain::FaultSeverity::Warning,
			nowMs));
	}
	else
	{
		static_cast<void>(diagnostics_.clearFault(domain::FaultCode::SettingsLoadFailure));
	}
	if (!fileSystemReady)
	{
		LOG_ERROR("storage", "filesystem unavailable");
		static_cast<void>(diagnostics_.recordFault(domain::FaultCode::FileSystemFailure,
			domain::FaultSeverity::Warning,
			nowMs));
	}
	else
	{
		static_cast<void>(diagnostics_.clearFault(domain::FaultCode::FileSystemFailure));
	}

	if (relayService_.initialize(nowMs) != RelayServiceInitializeResult::Initialized)
	{
		static_cast<void>(diagnostics_.recordFault(domain::FaultCode::RelayOutputFailure,
			domain::FaultSeverity::Critical,
			nowMs));
		static_cast<void>(lifecycle_.enterFault(LifecycleReason::CriticalFault, nowMs));
		LOG_FATAL("relay", "service initialization failed");
		return ApplicationInitializeResult::ServiceFailure;
	}
	if (button_.initialize(nowMs) != adapters::button::ButtonInitializeResult::Initialized)
	{
		static_cast<void>(lifecycle_.enterFault(LifecycleReason::CriticalFault, nowMs));
		LOG_ERROR("button", "initialization failed");
		return ApplicationInitializeResult::ButtonFailure;
	}
	Serial.begin(115200);
	cliAvailable_ = cli_.initialize() == adapters::cli::CliInitializeResult::Initialized;
	serialFilter_.setSinks(routeCliBytes, this, routeProvisioningBytes, this);
	network_.initialize(nowMs);
	char managementHost[96]{};
	char managementOrigin[192]{};
	std::snprintf(managementHost, sizeof(managementHost), "%s.local", configurationService_.active().network.hostName.data());
	std::snprintf(managementOrigin, sizeof(managementOrigin), "https://%s", managementHost);
	static_cast<void>(webSecurityStore_.initialize());
	static_cast<void>(webSecurityService_.initialize(managementOrigin, managementHost));
	webAvailable_ = webServer_.initialize() == adapters::web::WebServerInitializeResult::Initialized;
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
		LOG_WARNING("modbus", "transport unavailable");
		static_cast<void>(diagnostics_.recordFault(domain::FaultCode::ModbusTransportError,
			domain::FaultSeverity::Warning,
			nowMs));
	}
	if (!applyRestorePlan(startupResetCategory, nowMs))
	{
		static_cast<void>(lifecycle_.enterFault(LifecycleReason::CriticalFault, nowMs));
		LOG_FATAL("relay", "restore plan failed");
		return ApplicationInitializeResult::ServiceFailure;
	}

	lastRelayProcessAtMs_ = nowMs;
	lastModbusPollAtMs_ = nowMs;
	lastButtonUpdateAtMs_ = nowMs;
	lastCliPollAtMs_ = nowMs;
	lastIndicatorUpdateAtMs_ = nowMs;
	lastDiagnosticsUpdateAtMs_ = nowMs;
	lastPublishedNetworkSequence_ = networkHal_.status.snapshot().sequence;
	lastPublishedWifiScanSequence_ = networkHal_.status.snapshot().wifiScan.sequence;
	lastPublishedConfigurationGeneration_ = configurationService_.active().generation;
	diagnosticsEventSequence_ = 0;
	restartPending_ = false;
	initialized_ = true;
	if (!configurationValid || !persistenceHealthy || !fileSystemReady || !modbusAvailable_ || !cliAvailable_ ||
		knxResult == adapters::knx::KnxInitializeResult::Unavailable)
	{
		static_cast<void>(lifecycle_.enterDegraded(
			configurationValid ? LifecycleReason::AdapterUnavailable : LifecycleReason::ConfigurationInvalid,
			nowMs));
		statusIndicator_.setBusDegraded(true);
		LOG_WARNING("application", "initialized in degraded state");
		if (watchdogHal_.initialize() == hal::WatchdogInitializeResult::RegistrationFailure)
		{
			handleWatchdogFailure(nowMs);
			return ApplicationInitializeResult::WatchdogFailure;
		}
		return ApplicationInitializeResult::Degraded;
	}

	static_cast<void>(lifecycle_.enterOperational(nowMs));
	LOG_INFO("application", "initialized operational");
	if (watchdogHal_.initialize() == hal::WatchdogInitializeResult::RegistrationFailure)
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
	if (serviceMode_.update(nowMs))
	{
		statusIndicator_.setCommissioning(false);
	}
	static_cast<void>(relayTimerService_.update(nowMs));
	processWebRequest(nowMs);
	if (nowMs - lastRelayProcessAtMs_ >= relayProcessIntervalMs)
	{
		lastRelayProcessAtMs_ = nowMs;
		processRelayCommand(nowMs);
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
	webCommandTracker_.expire(nowMs);
	webRequestQueue_.expire(nowMs);
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
	publishWebStateEvents(nowMs);
	webServer_.update(nowMs);
	static_cast<void>(knx_.poll());
	if (watchdogHal_.feed() != hal::WatchdogFeedResult::Fed)
	{
		handleWatchdogFailure(nowMs);
	}
	if (!restartPending_ && lifecycle_.state() == LifecycleState::Restarting)
	{
		restartRequestedAtMs_ = nowMs;
		restartPending_ = true;
	}
	if (restartPending_ && nowMs - restartRequestedAtMs_ >= controlledRestartDrainMs)
	{
		flushDiagnosticCounters(nowMs, true);
		webServer_.stop();
		network_.shutdown();
		modbusSerialTransport_.shutdown();
		ESP.restart();
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
		static_cast<void>(serviceMode_.enterFromPhysicalPresence(event.occurredAtMs));
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
	static_cast<void>(serviceMode_.exit());
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
	relayTimerService_.cancelAll();
	sceneService_.disable();
	webServer_.stop();
	if (webSecurityService_.eraseUsersPreservingIdentity() != ports::WebSecurityStoreResult::Applied)
	{
		static_cast<void>(diagnostics_.recordFault(domain::FaultCode::SettingsSaveFailure,
			domain::FaultSeverity::Warning, nowMs));
		statusIndicator_.notifyCommand(adapters::indicators::CommandFeedback::Rejected, nowMs);
		return false;
	}
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

bool Application::applyRestorePlan(const domain::ResetCategory resetCategory, const std::uint32_t nowMs) noexcept
{
	domain::RelayRestorePlan plan{};
	const domain::RelayRestoreContext context{domain::relaySafetyEventForReset(resetCategory), {}, 1, nowMs};
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

void Application::processRelayCommand(const std::uint32_t nowMs) noexcept
{
	RelayCommandBatch batch{};
	if (!commandQueue_.dequeue(batch))
	{
		return;
	}
	const auto result = relayService_.executeBatch(batch.commands.data(), batch.count);
	for (std::size_t index = 0; index < batch.count; ++index)
	{
		const auto &command = batch.commands[index];
		const auto *snapshot = relayService_.snapshot(command.channel);
		WebTrackedCommand tracked{};
		if (snapshot != nullptr)
		{
			static_cast<void>(webCommandTracker_.complete(command.correlationId, result.status, result.reason,
				*snapshot, nowMs, tracked));
		}
		webEventJournal_.publish({0,
			WebEventType::RelayCommandCompleted,
			command.correlationId,
			command.channel,
			snapshot != nullptr ? snapshot->appliedState : domain::RelayState::Off,
			result.status,
			result.reason,
			snapshot != nullptr ? snapshot->transitionSequence : 0,
			nowMs});
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
	const RuntimeDiagnostics runtime{ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(),
		ESP.getPsramSize(), ESP.getFreePsram(), ESP.getMinFreePsram(), ESP.getCpuFreqMHz(),
		static_cast<std::uint8_t>(ESP.getChipCores()), watchdogHal_.isHealthy()};
	diagnostics_.updateRuntime(nowMs, runtime);
	if (runtime.freeHeapBytes < minimumFreeInternalHeapBytes ||
		runtime.largestFreeHeapBlockBytes < minimumLargestInternalHeapBlockBytes)
	{
		static_cast<void>(diagnostics_.recordFault(domain::FaultCode::ResourceExhaustion,
			domain::FaultSeverity::Warning, nowMs));
	}
	else
	{
		static_cast<void>(diagnostics_.clearFault(domain::FaultCode::ResourceExhaustion));
	}
	diagnostics_.updateNetwork(networkHal_.status.snapshot());
	diagnostics_.updateModbus(modbusAvailable_);
	flushDiagnosticCounters(nowMs, false);
	++diagnosticsEventSequence_;
	webEventJournal_.publish({0, WebEventType::DiagnosticsChanged, 0, {0}, domain::RelayState::Off,
		RelayCommandStatus::Accepted, RelayCommandReason::None, diagnosticsEventSequence_, nowMs});
}

void Application::publishWebStateEvents(const std::uint32_t nowMs) noexcept
{
	const auto &networkSnapshot = networkHal_.status.snapshot();
	if (networkSnapshot.sequence != lastPublishedNetworkSequence_)
	{
		lastPublishedNetworkSequence_ = networkSnapshot.sequence;
		webEventJournal_.publish({0, WebEventType::NetworkChanged, 0, {0}, domain::RelayState::Off,
			RelayCommandStatus::Accepted, RelayCommandReason::None, networkSnapshot.sequence, nowMs});
	}
	if (networkSnapshot.wifiScan.sequence != lastPublishedWifiScanSequence_)
	{
		lastPublishedWifiScanSequence_ = networkSnapshot.wifiScan.sequence;
		const auto eventType = networkSnapshot.wifiScan.state == ports::WifiScanState::Scanning ?
			WebEventType::WifiScanStarted : WebEventType::WifiScanCompleted;
		webEventJournal_.publish({0, eventType, 0, {0}, domain::RelayState::Off,
			RelayCommandStatus::Accepted, RelayCommandReason::None, networkSnapshot.wifiScan.sequence, nowMs});
	}
	const auto generation = configurationService_.active().generation;
	if (generation != lastPublishedConfigurationGeneration_)
	{
		lastPublishedConfigurationGeneration_ = generation;
		webEventJournal_.publish({0, WebEventType::ConfigurationChanged, 0, {0}, domain::RelayState::Off,
			RelayCommandStatus::Accepted, RelayCommandReason::None, generation, nowMs});
	}
}

std::uint32_t Application::monotonicMilliseconds(void *) noexcept
{
	return millis();
}

bool Application::extendModbusSnapshot(void *const context, adapters::modbus::RegisterMapSnapshot &snapshot) noexcept
{
	snapshot.softwareVersion = domain::compatibility::firmwareModbusRegister;
	if (context == nullptr)
	{
		return false;
	}
	auto &application = *static_cast<Application *>(context);
	const auto &configuration = application.configurationService_.active().modbus;
	snapshot.uartEncodedSettingsAvailable =
		adapters::modbus::ModbusRegisterMap::encodeUartSettings(configuration, snapshot.uartEncodedSettings);
	const auto indicator = application.statusIndicator_.maintenanceState(application.diagnostics_.snapshot().uptimeMs);
	snapshot.indicator = {indicator.red, indicator.green, indicator.blue, indicator.brightness, 0};
	return snapshot.uartEncodedSettingsAvailable;
}

adapters::modbus::WriteBatchResult Application::handleModbusNonRelayWrite(
	void *const context, const adapters::modbus::HoldingWriteBatch &batch) noexcept
{
	if (context == nullptr)
	{
		return domain::ErrorCode::InternalError;
	}
	auto &application = *static_cast<Application *>(context);
	const auto nowMs = application.diagnostics_.snapshot().uptimeMs;
	const auto &policy = application.configurationService_.active().indicators;
	if (batch.kind == adapters::modbus::HoldingWriteKind::Indicator)
	{
		auto state = application.statusIndicator_.maintenanceState(nowMs);
		if ((batch.indicator.updateMask & 0x01U) != 0) state.red = batch.indicator.red;
		if ((batch.indicator.updateMask & 0x02U) != 0) state.green = batch.indicator.green;
		if ((batch.indicator.updateMask & 0x04U) != 0) state.blue = batch.indicator.blue;
		if ((batch.indicator.updateMask & 0x08U) != 0) state.brightness = batch.indicator.brightness;
		const auto result = application.statusIndicator_.setMaintenanceColor(
			state.red, state.green, state.blue, state.brightness, policy.maximumBrightness, nowMs);
		return result == adapters::indicators::IndicatorResult::Applied
			? adapters::modbus::WriteBatchResult{} : adapters::modbus::WriteBatchResult{domain::ErrorCode::HardwareError};
	}
	if (batch.kind == adapters::modbus::HoldingWriteKind::Buzzer)
	{
		const auto result = application.statusIndicator_.playMaintenanceTone(
			batch.buzzerTone, policy.maximumBuzzerDutyPercent, nowMs);
		return result == adapters::indicators::IndicatorResult::Applied
			? adapters::modbus::WriteBatchResult{} : adapters::modbus::WriteBatchResult{domain::ErrorCode::HardwareError};
	}
	return adapters::modbus::ModbusConfigurationGateway::handle(&application.modbusConfigurationGateway_, batch);
}

void Application::processWebRequest(const std::uint32_t nowMs) noexcept
{
	WebApplicationRequest request{};
	if (!webRequestQueue_.dequeue(request)) return;
	WebOperationStatus status{WebOperationStatus::Unavailable};
	const auto mapWifiResult = [](const WifiManagementResult result) {
		switch (result)
		{
		case WifiManagementResult::Applied: return WebOperationStatus::Applied;
		case WifiManagementResult::GenerationConflict: return WebOperationStatus::Conflict;
		case WifiManagementResult::InvalidIndex:
		case WifiManagementResult::InvalidConfiguration: return WebOperationStatus::Invalid;
		default: return WebOperationStatus::Unavailable;
		}
	};
	switch (request.type)
	{
	case WebRequestType::RelayCommand:
	{
		const auto result = switchingPolicy_.requestChannel(request.channel, request.action, domain::CommandSource::Web,
			request.correlationId, request.receivedAtMs);
		status = result == SwitchingPolicyResult::Accepted ? WebOperationStatus::Applied :
			result == SwitchingPolicyResult::QueueFull ? WebOperationStatus::Unavailable : WebOperationStatus::Rejected;
		if (result != SwitchingPolicyResult::Accepted)
		{
			WebTrackedCommand tracked{};
			static_cast<void>(webCommandTracker_.reject(request.correlationId,
				result == SwitchingPolicyResult::QueueFull ? RelayCommandReason::EventRejected : RelayCommandReason::InvalidAction,
				nowMs, tracked));
		}
		break;
	}
	case WebRequestType::WifiScan:
		status = network_.startWifiScan(nowMs) ? WebOperationStatus::Applied : WebOperationStatus::Conflict;
		break;
	case WebRequestType::WifiSaveProfile:
		status = mapWifiResult(wifiManagementService_.saveProfile(request.wifiProfile));
		if (status == WebOperationStatus::Applied) network_.applyCommittedConfiguration(nowMs);
		break;
	case WebRequestType::WifiRemoveProfile:
		status = mapWifiResult(wifiManagementService_.removeProfile(request.index, request.expectedGeneration));
		if (status == WebOperationStatus::Applied) network_.applyCommittedConfiguration(nowMs);
		break;
	case WebRequestType::WifiMoveProfile:
		status = mapWifiResult(wifiManagementService_.moveProfile(request.index, request.toIndex,
			request.expectedGeneration));
		if (status == WebOperationStatus::Applied) network_.applyCommittedConfiguration(nowMs);
		break;
	case WebRequestType::WifiConnectProfile:
		status = network_.connectWifiProfile(request.index, nowMs) ? WebOperationStatus::Applied :
			WebOperationStatus::Conflict;
		break;
	case WebRequestType::WifiSaveRecoveryAp:
		status = mapWifiResult(wifiManagementService_.updateRecoveryAp(request.recoveryAp,
			request.expectedGeneration));
		if (status == WebOperationStatus::Applied) network_.applyCommittedConfiguration(nowMs);
		break;
	case WebRequestType::SaveModbusConfiguration:
	{
		if (request.expectedGeneration != configurationService_.active().generation)
		{
			status = WebOperationStatus::Conflict;
			break;
		}
		auto replacement = configurationService_.active();
		replacement.modbus = request.modbusConfiguration;
		if (configurationService_.stage(replacement) != ConfigurationStageResult::Staged)
		{
			status = WebOperationStatus::Invalid;
			break;
		}
		const auto commit = configurationService_.commit();
		diagnostics_.updateConfiguration(configurationService_.hasValidActiveConfiguration(),
			configurationService_.active().generation, configurationService_.lastLoadResult(),
			configurationService_.lastSaveResult());
		if (commit == ConfigurationCommitResult::PersistenceFailure)
		{
			static_cast<void>(diagnostics_.recordFault(domain::FaultCode::SettingsSaveFailure,
				domain::FaultSeverity::Warning, nowMs));
			status = WebOperationStatus::Unavailable;
			break;
		}
		if (commit != ConfigurationCommitResult::Committed && commit != ConfigurationCommitResult::CommittedRestartRequired)
		{
			status = WebOperationStatus::Unavailable;
			break;
		}
		static_cast<void>(diagnostics_.clearFault(domain::FaultCode::SettingsSaveFailure));
		status = WebOperationStatus::Applied;
		if (commit == ConfigurationCommitResult::CommittedRestartRequired &&
			lifecycle_.requestRestart(nowMs) != LifecycleResult::Applied) status = WebOperationStatus::Unavailable;
		break;
	}
	case WebRequestType::SetModbusRole:
	{
		const auto control = modbusRtu_.controlPort();
		status = control.isValid() && control.setRole(control.context, request.modbusRole) ?
			WebOperationStatus::Applied : WebOperationStatus::Unavailable;
		break;
	}
	case WebRequestType::SaveKnxConfiguration:
	{
		if (request.expectedGeneration != configurationService_.active().generation)
		{
			status = WebOperationStatus::Conflict;
			break;
		}
		auto replacement = configurationService_.active();
		replacement.knx = request.knxConfiguration;
		if (configurationService_.stage(replacement) != ConfigurationStageResult::Staged)
		{
			status = WebOperationStatus::Invalid;
			break;
		}
		const auto commit = configurationService_.commit();
		diagnostics_.updateConfiguration(configurationService_.hasValidActiveConfiguration(),
			configurationService_.active().generation, configurationService_.lastLoadResult(),
			configurationService_.lastSaveResult());
		if (commit == ConfigurationCommitResult::PersistenceFailure)
		{
			static_cast<void>(diagnostics_.recordFault(domain::FaultCode::SettingsSaveFailure,
				domain::FaultSeverity::Warning, nowMs));
			status = WebOperationStatus::Unavailable;
			break;
		}
		if (commit != ConfigurationCommitResult::Committed && commit != ConfigurationCommitResult::CommittedRestartRequired)
		{
			status = WebOperationStatus::Unavailable;
			break;
		}
		static_cast<void>(diagnostics_.clearFault(domain::FaultCode::SettingsSaveFailure));
		status = WebOperationStatus::Applied;
		if (commit == ConfigurationCommitResult::CommittedRestartRequired &&
			lifecycle_.requestRestart(nowMs) != LifecycleResult::Applied) status = WebOperationStatus::Unavailable;
		break;
	}
	case WebRequestType::SaveUser:
	{
		const auto result = webSecurityService_.saveUser(request.userId,
			std::string_view{request.username.data(), strnlen(request.username.data(), request.username.size())},
			request.userRole, request.userEnabled,
			std::string_view{request.password.data(), strnlen(request.password.data(), request.password.size())},
			request.replacePassword);
		status = result == WebUserManagementResult::Applied ? WebOperationStatus::Applied :
			(result == WebUserManagementResult::Invalid || result == WebUserManagementResult::DuplicateUsername ||
			 result == WebUserManagementResult::LastAdministrator) ? WebOperationStatus::Invalid :
			WebOperationStatus::Unavailable;
		std::fill(request.password.begin(), request.password.end(), '\0');
		break;
	}
	case WebRequestType::Restart:
		if (lifecycle_.requestRestart(nowMs) == LifecycleResult::Applied)
		{
			restartRequestedAtMs_ = nowMs;
			restartPending_ = true;
			status = WebOperationStatus::Applied;
		}
		else status = WebOperationStatus::Unavailable;
		break;
	}
	static_cast<void>(webRequestQueue_.complete(request.operationId, status, nowMs));
}

void Application::flushDiagnosticCounters(const std::uint32_t nowMs, const bool force) noexcept
{
	if (!diagnostics_.persistentCountersDirty() ||
		(!force && nowMs - lastDiagnosticCounterFlushAtMs_ < diagnosticCounterFlushIntervalMs))
	{
		return;
	}
	lastDiagnosticCounterFlushAtMs_ = nowMs;
	if (settingsStore_.saveDiagnosticCounters(diagnostics_.snapshot().persistentCounters))
	{
		diagnostics_.markPersistentCountersSaved();
		return;
	}
	diagnostics_.recordStorageFailure();
}
}