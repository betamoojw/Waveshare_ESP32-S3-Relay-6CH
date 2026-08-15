#define EMBEDDED_CLI_IMPL
#include "cli_adapter.h"

#include <charconv>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>

namespace switch_actuator::adapters::cli
{
namespace
{
constexpr std::string_view firmwareVersion{"1.00"};

[[nodiscard]] bool hasNoArguments(const char *const arguments) noexcept
{
	return embeddedCliGetTokenCount(arguments) == 0;
}

[[nodiscard]] bool parseChannel(const char *const text, std::uint8_t &channel) noexcept
{
	if (text == nullptr || *text == '\0' || *text == '-')
	{
		return false;
	}
	unsigned int parsed{0};
	const auto length = std::strlen(text);
	const auto result = std::from_chars(text, text + length, parsed, 10);
	if (result.ec != std::errc{} || result.ptr != text + length || parsed >= domain::relayChannelCount)
	{
		return false;
	}
	channel = static_cast<std::uint8_t>(parsed);
	return true;
}

[[nodiscard]] bool parseUint8(const char *const text, const std::uint8_t maximum, std::uint8_t &value) noexcept
{
	if (text == nullptr || *text == '\0' || *text == '-')
	{
		return false;
	}
	unsigned int parsed{0};
	const auto length = std::strlen(text);
	const auto result = std::from_chars(text, text + length, parsed, 10);
	if (result.ec != std::errc{} || result.ptr != text + length || parsed > maximum)
	{
		return false;
	}
	value = static_cast<std::uint8_t>(parsed);
	return true;
}

[[nodiscard]] bool parseUint16(const char *const text, const std::uint16_t maximum, std::uint16_t &value) noexcept
{
	if (text == nullptr || *text == '\0' || *text == '-')
	{
		return false;
	}
	unsigned long parsed{0};
	const auto length = std::strlen(text);
	const auto result = std::from_chars(text, text + length, parsed, 10);
	if (result.ec != std::errc{} || result.ptr != text + length || parsed > maximum)
	{
		return false;
	}
	value = static_cast<std::uint16_t>(parsed);
	return true;
}

[[nodiscard]] bool parseUint32(const char *const text, std::uint32_t &value) noexcept
{
	if (text == nullptr || *text == '\0' || *text == '-')
	{
		return false;
	}
	unsigned long parsed{0};
	const auto length = std::strlen(text);
	const auto result = std::from_chars(text, text + length, parsed, 10);
	if (result.ec != std::errc{} || result.ptr != text + length || parsed > std::numeric_limits<std::uint32_t>::max())
	{
		return false;
	}
	value = static_cast<std::uint32_t>(parsed);
	return true;
}

[[nodiscard]] bool parseBoolean(const char *const text, bool &value) noexcept
{
	if (text == nullptr)
	{
		return false;
	}
	if (std::strcmp(text, "true") == 0 || std::strcmp(text, "yes") == 0 || std::strcmp(text, "enabled") == 0 ||
		std::strcmp(text, "1") == 0)
	{
		value = true;
		return true;
	}
	if (std::strcmp(text, "false") == 0 || std::strcmp(text, "no") == 0 || std::strcmp(text, "disabled") == 0 ||
		std::strcmp(text, "0") == 0)
	{
		value = false;
		return true;
	}
	return false;
}

[[nodiscard]] bool parseAddressPart(const char *const begin,
									const char *const end,
									const unsigned int maximum,
									unsigned int &value) noexcept
{
	if (begin == nullptr || begin == end)
	{
		return false;
	}
	unsigned int parsed{0};
	const auto result = std::from_chars(begin, end, parsed, 10);
	if (result.ec != std::errc{} || result.ptr != end || parsed > maximum)
	{
		return false;
	}
	value = parsed;
	return true;
}

[[nodiscard]] bool parseThreeLevelAddress(const char *const text,
										 const char separator,
										 const unsigned int firstMaximum,
										 const unsigned int secondMaximum,
										 unsigned int &first,
										 unsigned int &second,
										 unsigned int &third) noexcept
{
	if (text == nullptr)
	{
		return false;
	}
	const auto *const firstSeparator = std::strchr(text, separator);
	const auto *const secondSeparator = firstSeparator == nullptr ? nullptr : std::strchr(firstSeparator + 1, separator);
	const auto *const end = text + std::strlen(text);
	return firstSeparator != nullptr && secondSeparator != nullptr &&
		std::strchr(secondSeparator + 1, separator) == nullptr &&
		parseAddressPart(text, firstSeparator, firstMaximum, first) &&
		parseAddressPart(firstSeparator + 1, secondSeparator, secondMaximum, second) &&
		parseAddressPart(secondSeparator + 1, end, 255, third);
}

[[nodiscard]] bool parseIndividualAddress(const char *const text, std::uint16_t &address) noexcept
{
	unsigned int area{0};
	unsigned int line{0};
	unsigned int device{0};
	if (!parseThreeLevelAddress(text, '.', 15, 15, area, line, device))
	{
		return false;
	}
	address = static_cast<std::uint16_t>((area << 12U) | (line << 8U) | device);
	return address != 0;
}

[[nodiscard]] bool parseGroupAddress(const char *const text, std::uint16_t &address) noexcept
{
	if (text != nullptr && std::strcmp(text, "none") == 0)
	{
		address = 0;
		return true;
	}
	unsigned int main{0};
	unsigned int middle{0};
	unsigned int sub{0};
	if (!parseThreeLevelAddress(text, '/', 31, 7, main, middle, sub))
	{
		return false;
	}
	address = static_cast<std::uint16_t>((main << 11U) | (middle << 8U) | sub);
	return address != 0;
}

void formatGroupAddress(const std::uint16_t address, char *const output, const std::size_t outputSize) noexcept
{
	if (address == 0)
	{
		std::snprintf(output, outputSize, "none");
		return;
	}
	std::snprintf(output,
		outputSize,
		"%u/%u/%u",
		static_cast<unsigned int>(address >> 11U),
		static_cast<unsigned int>((address >> 8U) & 0x07U),
		static_cast<unsigned int>(address & 0xFFU));
}

[[nodiscard]] const char *clientError(const ports::ModbusClientResult result) noexcept
{
	switch (result)
	{
	case ports::ModbusClientResult::InvalidArgument:
		return "invalid-argument";
	case ports::ModbusClientResult::WrongRole:
		return "wrong-role";
	case ports::ModbusClientResult::NotInitialized:
		return "not-initialized";
	case ports::ModbusClientResult::TransportError:
		return "transport-error";
	case ports::ModbusClientResult::ProtocolError:
	default:
		return "protocol-error";
	}
}

[[nodiscard]] bool parseState(const char *const text, domain::RelayAction &action) noexcept
{
	if (text == nullptr)
	{
		return false;
	}
	if (std::strcmp(text, "on") == 0 || std::strcmp(text, "1") == 0)
	{
		action = domain::RelayAction::SetOn;
		return true;
	}
	if (std::strcmp(text, "off") == 0 || std::strcmp(text, "0") == 0)
	{
		action = domain::RelayAction::SetOff;
		return true;
	}
	return false;
}
}

CliAdapter::CliAdapter(const CliDependencies dependencies) noexcept
	: dependencies_{dependencies}
{
}

CliInitializeResult CliAdapter::initialize() noexcept
{
	initialized_ = false;
	cli_ = nullptr;
	if (!dependenciesValid())
	{
		return CliInitializeResult::InvalidDependencies;
	}

	auto *config = embeddedCliDefaultConfig();
	config->invitation = "switch-actuator> ";
	config->rxBufferSize = 128;
	config->cmdBufferSize = 128;
	config->historyBufferSize = 256;
	config->maxBindingCount = maximumBindings;
	config->cliBuffer = buffer_.data();
	config->cliBufferSize = bufferSize;
	config->enableAutoComplete = true;
	if (embeddedCliRequiredSize(config) > bufferSize)
	{
		return CliInitializeResult::InsufficientBuffer;
	}

	cli_ = embeddedCliNew(config);
	if (cli_ == nullptr)
	{
		return CliInitializeResult::InsufficientBuffer;
	}
	cli_->appContext = this;
	cli_->writeChar = writeCharacter;
	cli_->onCommand = unknownCommand;
	const std::array<CliCommandBinding, 22> bindings{{
		{"version", "Firmware and CLI version", true, this, versionCommand},
		{"status", "Machine-readable device status", true, this, statusCommand},
		{"get-relay", "get-relay [all|0..5]", true, this, getRelayCommand},
		{"set-relay", "set-relay [all|0..5] [on|off]", true, this, setRelayCommand},
		{"toggle-relay", "toggle-relay [0..5]", true, this, toggleRelayCommand},
		{"get-indicator", "Machine-readable RGB and buzzer status", true, this, getIndicatorCommand},
		{"set-rgb", "set-rgb [0..255] [0..255] [0..255] [brightness]", true, this, setRgbCommand},
		{"buzzer", "buzzer [0..7]", true, this, buzzerCommand},
		{"get-button", "Machine-readable BOOT button status", true, this, getButtonCommand},
		{"get-modbus-role", "Get active Modbus RTU role", true, this, getModbusRoleCommand},
		{"set-modbus-role", "set-modbus-role [server|client]", true, this, setModbusRoleCommand},
		{"get-modbus-config", "Get Modbus RTU serial configuration", true, this, getModbusConfigCommand},
		{"set-modbus-config", "set-modbus-config [slave_id] [baud] [none|even|odd] [stop_bits]", true, this, setModbusConfigCommand},
		{"modbus-read-holding", "modbus-read-holding [unit] [address] [count]", true, this, modbusReadHoldingCommand},
		{"modbus-write-register", "modbus-write-register [unit] [address] [value]", true, this, modbusWriteRegisterCommand},
		{"get-knx", "get-knx [general|channel 0..5]", true, this, getKnxCommand},
		{"set-knx", "set-knx [parameter] [value]", true, this, setKnxCommand},
		{"set-knx-channel", "set-knx-channel [0..5] [parameter] [value]", true, this, setKnxChannelCommand},
		{"set-wifi", "set-wifi [profile_0..2] [ssid] [passphrase]", true, this, setWifiCommand},
		{"load-config", "Load /config JSON files into validated active configuration", true, this, loadConfigCommand},
		{"store-config", "Store active configuration into /config JSON files", true, this, storeConfigCommand},
		{"reboot", "Request a controlled restart", true, this, rebootCommand},
	}};
	for (const auto &binding : bindings)
	{
		if (!embeddedCliAddBinding(cli_, binding))
		{
			return CliInitializeResult::BindingFailure;
		}
	}

	correlationId_ = 0;
	maintenanceAuthorized_ = false;
	initialized_ = true;
	print("ready=true adapter=cli version=1");
	return CliInitializeResult::Initialized;
}

CliPollResult CliAdapter::poll() noexcept
{
	if (!initialized_)
	{
		return CliPollResult::NotInitialized;
	}

	embeddedCliProcess(cli_);
	return CliPollResult::Idle;
}

void CliAdapter::ingest(const std::uint8_t value) noexcept
{
	if (initialized_)
	{
		embeddedCliReceiveChar(cli_, static_cast<char>(value));
	}
}

void CliAdapter::setMaintenanceAuthorized(const bool authorized) noexcept
{
	maintenanceAuthorized_ = authorized;
}

bool CliAdapter::isMaintenanceAuthorized() const noexcept
{
	return maintenanceAuthorized_;
}

bool CliAdapter::isInitialized() const noexcept
{
	return initialized_;
}

void CliAdapter::writeCharacter(EmbeddedCli *const cli, const char character) noexcept
{
	if (cli != nullptr && cli->appContext != nullptr)
	{
		auto &adapter = *static_cast<CliAdapter *>(cli->appContext);
		adapter.dependencies_.stream->write(static_cast<std::uint8_t>(character));
	}
}

void CliAdapter::unknownCommand(EmbeddedCli *const cli, CliCommand *) noexcept
{
	if (cli != nullptr && cli->appContext != nullptr)
	{
		static_cast<CliAdapter *>(cli->appContext)->print("ok=false error=unknown-command");
	}
}

void CliAdapter::versionCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	auto &adapter = *static_cast<CliAdapter *>(context);
	adapter.print(hasNoArguments(arguments) ? "ok=true firmware=1.00 cli=1" : "ok=false error=unexpected-argument");
}

void CliAdapter::statusCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	auto &adapter = *static_cast<CliAdapter *>(context);
	if (!hasNoArguments(arguments))
	{
		adapter.print("ok=false error=unexpected-argument");
		return;
	}
	adapter.printStatus();
}

void CliAdapter::getRelayCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	auto &adapter = *static_cast<CliAdapter *>(context);
	if (embeddedCliGetTokenCount(arguments) != 1)
	{
		adapter.print("ok=false error=usage usage=get-relay_[all|0..5]");
		return;
	}
	const auto *token = embeddedCliGetToken(arguments, 1);
	if (std::strcmp(token, "all") == 0)
	{
		adapter.printStatus();
		return;
	}
	std::uint8_t channel{0};
	if (!parseChannel(token, channel))
	{
		adapter.print("ok=false error=invalid-channel");
		return;
	}
	adapter.printRelay(channel);
}

void CliAdapter::setRelayCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	static_cast<CliAdapter *>(context)->handleSetRelay(arguments);
}

void CliAdapter::toggleRelayCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	static_cast<CliAdapter *>(context)->handleToggleRelay(arguments);
}

void CliAdapter::getIndicatorCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	auto &adapter = *static_cast<CliAdapter *>(context);
	if (!hasNoArguments(arguments))
	{
		adapter.print("ok=false error=unexpected-argument");
		return;
	}
	adapter.printIndicator();
}

void CliAdapter::setRgbCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	static_cast<CliAdapter *>(context)->handleSetRgb(arguments);
}

void CliAdapter::buzzerCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	static_cast<CliAdapter *>(context)->handleBuzzer(arguments);
}

void CliAdapter::getButtonCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	auto &adapter = *static_cast<CliAdapter *>(context);
	if (!hasNoArguments(arguments))
	{
		adapter.print("ok=false error=unexpected-argument");
		return;
	}
	adapter.printButton();
}

void CliAdapter::getModbusRoleCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	auto &adapter = *static_cast<CliAdapter *>(context);
	if (!hasNoArguments(arguments))
	{
		adapter.print("ok=false error=unexpected-argument");
		return;
	}
	const auto role = adapter.dependencies_.modbus.role(adapter.dependencies_.modbus.context);
	adapter.print(role == ports::ModbusRtuRole::Server ? "ok=true role=server" : "ok=true role=client");
}

void CliAdapter::setModbusRoleCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	static_cast<CliAdapter *>(context)->handleSetModbusRole(arguments);
}

void CliAdapter::getModbusConfigCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	static_cast<CliAdapter *>(context)->handleGetModbusConfig(arguments);
}

void CliAdapter::setModbusConfigCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	static_cast<CliAdapter *>(context)->handleSetModbusConfig(arguments);
}

void CliAdapter::modbusReadHoldingCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	static_cast<CliAdapter *>(context)->handleModbusReadHolding(arguments);
}

void CliAdapter::modbusWriteRegisterCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	static_cast<CliAdapter *>(context)->handleModbusWriteRegister(arguments);
}

void CliAdapter::getKnxCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	static_cast<CliAdapter *>(context)->handleGetKnx(arguments);
}

void CliAdapter::setKnxCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	static_cast<CliAdapter *>(context)->handleSetKnx(arguments);
}

void CliAdapter::setKnxChannelCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	static_cast<CliAdapter *>(context)->handleSetKnxChannel(arguments);
}

void CliAdapter::setWifiCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	static_cast<CliAdapter *>(context)->handleSetWifi(arguments);
}

void CliAdapter::loadConfigCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	static_cast<CliAdapter *>(context)->handleLoadConfig(arguments);
}

void CliAdapter::storeConfigCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	static_cast<CliAdapter *>(context)->handleStoreConfig(arguments);
}

void CliAdapter::rebootCommand(EmbeddedCli *, char *const arguments, void *const context) noexcept
{
	static_cast<CliAdapter *>(context)->handleReboot(arguments);
}

bool CliAdapter::dependenciesValid() const noexcept
{
	return dependencies_.stream != nullptr && dependencies_.switchingPolicy != nullptr && dependencies_.relayService != nullptr &&
		   dependencies_.lifecycleSupervisor != nullptr && dependencies_.diagnostics != nullptr &&
		   dependencies_.configurationService != nullptr && dependencies_.configurationFile.isValid() &&
		   dependencies_.statusIndicator != nullptr &&
		   dependencies_.button != nullptr && dependencies_.networkManager != nullptr && dependencies_.modbus.isValid() &&
		   dependencies_.clock.isValid();
}

bool CliAdapter::mutatingCommandAllowed() const noexcept
{
	return dependencies_.mutatingCommandsEnabled && maintenanceAuthorized_ &&
		   dependencies_.lifecycleSupervisor->acceptsOrdinaryCommands();
}

bool CliAdapter::enqueueRelayBatch(const app::RelayCommandBatch &batch) noexcept
{
	const auto result = dependencies_.switchingPolicy->requestBatch(batch);
	if (result == app::SwitchingPolicyResult::Accepted)
	{
		print("ok=true result=queued");
		return true;
	}
	if (result == app::SwitchingPolicyResult::QueueFull)
	{
		dependencies_.diagnostics->recordCommandQueueFull(dependencies_.clock.nowMs());
		print("ok=false error=queue-full");
		return false;
	}
	print("ok=false error=invalid-command");
	return false;
}

std::uint32_t CliAdapter::nextCorrelationId() noexcept
{
	correlationId_ = correlationId_ == std::numeric_limits<std::uint32_t>::max() ? 1 : correlationId_ + 1;
	return correlationId_;
}

void CliAdapter::print(const char *const message) noexcept
{
	embeddedCliPrint(cli_, message);
}

void CliAdapter::printStatus() noexcept
{
	const auto &snapshots = dependencies_.relayService->snapshots();
	const auto &diagnostics = dependencies_.diagnostics->snapshot();
	char output[256]{};
	std::snprintf(output,
		sizeof(output),
		"{\"ok\":true,\"lifecycle\":%u,\"uptime_ms\":%lu,\"authorized\":%s,\"relays\":[%u,%u,%u,%u,%u,%u]}",
		static_cast<unsigned int>(dependencies_.lifecycleSupervisor->state()),
		static_cast<unsigned long>(diagnostics.uptimeMs),
		maintenanceAuthorized_ ? "true" : "false",
		static_cast<unsigned int>(snapshots[0].appliedState == domain::RelayState::On),
		static_cast<unsigned int>(snapshots[1].appliedState == domain::RelayState::On),
		static_cast<unsigned int>(snapshots[2].appliedState == domain::RelayState::On),
		static_cast<unsigned int>(snapshots[3].appliedState == domain::RelayState::On),
		static_cast<unsigned int>(snapshots[4].appliedState == domain::RelayState::On),
		static_cast<unsigned int>(snapshots[5].appliedState == domain::RelayState::On));
	print(output);
}

void CliAdapter::printRelay(const std::uint8_t channel) noexcept
{
	const auto *snapshot = dependencies_.relayService->snapshot(domain::RelayChannelId{channel});
	if (snapshot == nullptr)
	{
		print("ok=false error=invalid-channel");
		return;
	}
	char output[128]{};
	std::snprintf(output,
		sizeof(output),
		"ok=true channel=%u state=%s locked=%s fault=%u",
		static_cast<unsigned int>(channel),
		snapshot->appliedState == domain::RelayState::On ? "on" : "off",
		snapshot->lockedOut ? "true" : "false",
		static_cast<unsigned int>(snapshot->fault));
	print(output);
}

void CliAdapter::printIndicator() noexcept
{
	const auto nowMs = dependencies_.clock.nowMs();
	const auto mode = dependencies_.statusIndicator->activeMode(nowMs);
	const auto maintenance = dependencies_.statusIndicator->maintenanceState(nowMs);
	char output[192]{};
	std::snprintf(output,
		sizeof(output),
		"ok=true initialized=%s mode=%u maintenance=%s red=%u green=%u blue=%u brightness=%u tone=%u duty_percent=%u",
		dependencies_.statusIndicator->isInitialized() ? "true" : "false",
		static_cast<unsigned int>(mode),
		maintenance.active ? "true" : "false",
		static_cast<unsigned int>(maintenance.red),
		static_cast<unsigned int>(maintenance.green),
		static_cast<unsigned int>(maintenance.blue),
		static_cast<unsigned int>(maintenance.brightness),
		static_cast<unsigned int>(maintenance.tone),
		static_cast<unsigned int>(maintenance.buzzerDutyPercent));
	print(output);
}

void CliAdapter::printButton() noexcept
{
	char output[96]{};
	std::snprintf(output,
		sizeof(output),
		"ok=true initialized=%s pressed=%s",
		dependencies_.button->isInitialized() ? "true" : "false",
		dependencies_.button->isPressed() ? "true" : "false");
	print(output);
}

void CliAdapter::printKnxGeneral() noexcept
{
	const auto &configuration = dependencies_.configurationService->active();
	const auto &knx = configuration.knx;
	char heartbeatAddress[16]{};
	char centralSwitchAddress[16]{};
	char centralOffAddress[16]{};
	char deviceFaultAddress[16]{};
	formatGroupAddress(knx.heartbeatGroupAddress, heartbeatAddress, sizeof(heartbeatAddress));
	formatGroupAddress(knx.centralSwitchGroupAddress, centralSwitchAddress, sizeof(centralSwitchAddress));
	formatGroupAddress(knx.centralOffGroupAddress, centralOffAddress, sizeof(centralOffAddress));
	formatGroupAddress(knx.deviceFaultGroupAddress, deviceFaultAddress, sizeof(deviceFaultAddress));
	char output[512]{};
	std::snprintf(output,
		sizeof(output),
		"ok=true scope=general generation=%lu enabled=%s individual_address=%u.%u.%u startup_delay_ms=%lu "
		"telegram_interval_ms=%u cyclic_status_ms=%lu heartbeat_interval_ms=%lu read_switch=%s heartbeat_ga=%s "
		"central_switch_ga=%s central_off_ga=%s device_fault_ga=%s",
		static_cast<unsigned long>(configuration.generation),
		knx.enabled ? "true" : "false",
		static_cast<unsigned int>(knx.individualAddress >> 12U),
		static_cast<unsigned int>((knx.individualAddress >> 8U) & 0x0FU),
		static_cast<unsigned int>(knx.individualAddress & 0xFFU),
		static_cast<unsigned long>(knx.startupTransmitDelayMs),
		static_cast<unsigned int>(knx.minimumTelegramIntervalMs),
		static_cast<unsigned long>(knx.cyclicStatusIntervalMs),
		static_cast<unsigned long>(knx.heartbeatIntervalMs),
		knx.readSwitchObject ? "true" : "false",
		heartbeatAddress,
		centralSwitchAddress,
		centralOffAddress,
		deviceFaultAddress);
	print(output);
}

void CliAdapter::printKnxChannel(const std::uint8_t channel) noexcept
{
	const auto &configuration = dependencies_.configurationService->active().knx.channels[channel];
	char switchAddress[16]{};
	char statusAddress[16]{};
	char faultAddress[16]{};
	formatGroupAddress(configuration.switchGroupAddress, switchAddress, sizeof(switchAddress));
	formatGroupAddress(configuration.statusGroupAddress, statusAddress, sizeof(statusAddress));
	formatGroupAddress(configuration.faultGroupAddress, faultAddress, sizeof(faultAddress));
	char output[384]{};
	std::snprintf(output,
		sizeof(output),
		"ok=true scope=channel channel=%u switch_ga=%s status_ga=%s fault_ga=%s command_inverted=%s "
		"status_inverted=%s startup_status=%s central_switch=%s central_off=%s",
		static_cast<unsigned int>(channel),
		switchAddress,
		statusAddress,
		faultAddress,
		configuration.commandPolarityInverted ? "true" : "false",
		configuration.statusPolarityInverted ? "true" : "false",
		configuration.sendStatusAfterStartup ? "true" : "false",
		configuration.participatesInCentralSwitch ? "true" : "false",
		configuration.participatesInCentralOff ? "true" : "false");
	print(output);
}

void CliAdapter::handleSetRelay(char *const arguments) noexcept
{
	if (!mutatingCommandAllowed())
	{
		print("ok=false error=not-authorized");
		return;
	}
	if (embeddedCliGetTokenCount(arguments) != 2)
	{
		print("ok=false error=usage usage=set-relay_[all|0..5]_[on|off]");
		return;
	}
	const auto *channelToken = embeddedCliGetToken(arguments, 1);
	domain::RelayAction action{};
	if (!parseState(embeddedCliGetToken(arguments, 2), action))
	{
		print("ok=false error=invalid-state");
		return;
	}

	app::RelayCommandBatch batch{};
	const auto nowMs = dependencies_.clock.nowMs();
	if (std::strcmp(channelToken, "all") == 0)
	{
		batch.count = domain::relayChannelCount;
		for (std::uint8_t channel = 0; channel < domain::relayChannelCount; ++channel)
		{
			batch.commands[channel] = {{channel}, action, domain::CommandSource::Cli, nextCorrelationId(), nowMs};
		}
	}
	else
	{
		std::uint8_t channel{0};
		if (!parseChannel(channelToken, channel))
		{
			print("ok=false error=invalid-channel");
			return;
		}
		batch.count = 1;
		batch.commands[0] = {{channel}, action, domain::CommandSource::Cli, nextCorrelationId(), nowMs};
	}
	static_cast<void>(enqueueRelayBatch(batch));
}

void CliAdapter::handleToggleRelay(char *const arguments) noexcept
{
	if (!mutatingCommandAllowed())
	{
		print("ok=false error=not-authorized");
		return;
	}
	if (embeddedCliGetTokenCount(arguments) != 1)
	{
		print("ok=false error=usage usage=toggle-relay_[0..5]");
		return;
	}
	std::uint8_t channel{0};
	if (!parseChannel(embeddedCliGetToken(arguments, 1), channel))
	{
		print("ok=false error=invalid-channel");
		return;
	}
	app::RelayCommandBatch batch{};
	batch.count = 1;
	batch.commands[0] = {{channel},
		domain::RelayAction::Toggle,
		domain::CommandSource::Cli,
		nextCorrelationId(),
		dependencies_.clock.nowMs()};
	static_cast<void>(enqueueRelayBatch(batch));
}

void CliAdapter::handleSetRgb(char *const arguments) noexcept
{
	if (!mutatingCommandAllowed())
	{
		print("ok=false error=not-authorized");
		return;
	}
	const auto tokenCount = embeddedCliGetTokenCount(arguments);
	if (tokenCount != 3 && tokenCount != 4)
	{
		print("ok=false error=usage usage=set-rgb_[red]_[green]_[blue]_[brightness]");
		return;
	}
	std::uint8_t red{0};
	std::uint8_t green{0};
	std::uint8_t blue{0};
	std::uint8_t brightness = dependencies_.configurationService->active().indicators.maximumBrightness;
	if (!parseUint8(embeddedCliGetToken(arguments, 1), 255, red) ||
		!parseUint8(embeddedCliGetToken(arguments, 2), 255, green) ||
		!parseUint8(embeddedCliGetToken(arguments, 3), 255, blue) ||
		(tokenCount == 4 && !parseUint8(embeddedCliGetToken(arguments, 4), 255, brightness)))
	{
		print("ok=false error=invalid-value");
		return;
	}
	const auto &policy = dependencies_.configurationService->active().indicators;
	const auto result = dependencies_.statusIndicator->setMaintenanceColor(
		red, green, blue, brightness, policy.maximumBrightness, dependencies_.clock.nowMs());
	print(result == indicators::IndicatorResult::Applied ? "ok=true result=applied" : "ok=false error=indicator-unavailable");
}

void CliAdapter::handleBuzzer(char *const arguments) noexcept
{
	if (!mutatingCommandAllowed())
	{
		print("ok=false error=not-authorized");
		return;
	}
	if (embeddedCliGetTokenCount(arguments) != 1)
	{
		print("ok=false error=usage usage=buzzer_[0..7]");
		return;
	}
	std::uint8_t tone{0};
	if (!parseUint8(embeddedCliGetToken(arguments, 1), 7, tone))
	{
		print("ok=false error=invalid-tone");
		return;
	}
	const auto duty = dependencies_.configurationService->active().indicators.maximumBuzzerDutyPercent;
	const auto result = dependencies_.statusIndicator->playMaintenanceTone(tone, duty, dependencies_.clock.nowMs());
	print(result == indicators::IndicatorResult::Applied ? "ok=true result=applied" : "ok=false error=indicator-unavailable");
}

void CliAdapter::handleSetModbusRole(char *const arguments) noexcept
{
	if (!mutatingCommandAllowed())
	{
		print("ok=false error=not-authorized");
		return;
	}
	if (embeddedCliGetTokenCount(arguments) != 1)
	{
		print("ok=false error=usage usage=set-modbus-role_[server|client]");
		return;
	}
	const auto *roleToken = embeddedCliGetToken(arguments, 1);
	ports::ModbusRtuRole role{};
	if (std::strcmp(roleToken, "server") == 0)
	{
		role = ports::ModbusRtuRole::Server;
	}
	else if (std::strcmp(roleToken, "client") == 0)
	{
		role = ports::ModbusRtuRole::Client;
	}
	else
	{
		print("ok=false error=invalid-role");
		return;
	}
	if (!dependencies_.modbus.setRole(dependencies_.modbus.context, role))
	{
		print("ok=false error=role-switch-failed");
		return;
	}
	print(role == ports::ModbusRtuRole::Server ? "ok=true role=server" : "ok=true role=client");
}

void CliAdapter::handleGetModbusConfig(char *const arguments) noexcept
{
	if (!hasNoArguments(arguments))
	{
		print("ok=false error=usage usage=get-modbus-config");
		return;
	}
	const auto &configuration = dependencies_.configurationService->active().modbus;
	const auto parity = configuration.parity == domain::SerialParity::None ? "none" :
		configuration.parity == domain::SerialParity::Even ? "even" : "odd";
	char response[160]{};
	std::snprintf(response,
		sizeof(response),
		"ok=true slave_id=%u baud=%lu data_bits=%u parity=%s stop_bits=%u",
		static_cast<unsigned int>(configuration.unitId),
		static_cast<unsigned long>(configuration.baudRate),
		static_cast<unsigned int>(configuration.dataBits),
		parity,
		static_cast<unsigned int>(configuration.stopBits));
	print(response);
}

void CliAdapter::handleSetModbusConfig(char *const arguments) noexcept
{
	if (!mutatingCommandAllowed())
	{
		print("ok=false error=not-authorized");
		return;
	}
	if (embeddedCliGetTokenCount(arguments) != 4)
	{
		print("ok=false error=usage usage=set-modbus-config_[slave_id_1..247]_[baud]_[none|even|odd]_[stop_bits_1|2]");
		return;
	}
	std::uint8_t slaveId{0};
	std::uint32_t baudRate{0};
	std::uint8_t stopBits{0};
	if (!parseUint8(embeddedCliGetToken(arguments, 1), 247, slaveId) || slaveId == 0 ||
		!parseUint32(embeddedCliGetToken(arguments, 2), baudRate) ||
		!parseUint8(embeddedCliGetToken(arguments, 4), 2, stopBits) || stopBits == 0)
	{
		print("ok=false error=invalid-value");
		return;
	}
	domain::SerialParity parity{};
	const auto *const parityToken = embeddedCliGetToken(arguments, 3);
	if (std::strcmp(parityToken, "none") == 0)
	{
		parity = domain::SerialParity::None;
	}
	else if (std::strcmp(parityToken, "even") == 0)
	{
		parity = domain::SerialParity::Even;
	}
	else if (std::strcmp(parityToken, "odd") == 0)
	{
		parity = domain::SerialParity::Odd;
	}
	else
	{
		print("ok=false error=invalid-parity");
		return;
	}
	auto configuration = dependencies_.configurationService->active();
	configuration.modbus.unitId = slaveId;
	configuration.modbus.baudRate = baudRate;
	configuration.modbus.dataBits = 8;
	configuration.modbus.parity = parity;
	configuration.modbus.stopBits = stopBits;
	commitConfiguration(configuration);
}

void CliAdapter::handleModbusReadHolding(char *const arguments) noexcept
{
	if (!mutatingCommandAllowed())
	{
		print("ok=false error=not-authorized");
		return;
	}
	if (embeddedCliGetTokenCount(arguments) != 3)
	{
		print("ok=false error=usage usage=modbus-read-holding_[unit]_[address]_[count]");
		return;
	}
	std::uint8_t destination{0};
	std::uint16_t address{0};
	std::uint16_t quantity{0};
	if (!parseUint8(embeddedCliGetToken(arguments, 1), 247, destination) || destination == 0 ||
		!parseUint16(embeddedCliGetToken(arguments, 2), std::numeric_limits<std::uint16_t>::max(), address) ||
		!parseUint16(embeddedCliGetToken(arguments, 3), 20, quantity) || quantity == 0)
	{
		print("ok=false error=invalid-argument");
		return;
	}
	std::array<std::uint16_t, 20> values{};
	const auto result = dependencies_.modbus.readHoldingRegisters(
		dependencies_.modbus.context, destination, address, quantity, values.data(), values.size());
	if (result != ports::ModbusClientResult::Success)
	{
		char output[64]{};
		std::snprintf(output, sizeof(output), "ok=false error=%s", clientError(result));
		print(output);
		return;
	}

	char output[256]{};
	auto used = static_cast<std::size_t>(std::snprintf(output,
		sizeof(output),
		"ok=true unit=%u address=%u count=%u values=[",
		static_cast<unsigned int>(destination),
		static_cast<unsigned int>(address),
		static_cast<unsigned int>(quantity)));
	for (std::uint16_t index = 0; index < quantity && used < sizeof(output); ++index)
	{
		const auto written = std::snprintf(output + used,
			sizeof(output) - used,
			index == 0 ? "%u" : ",%u",
			static_cast<unsigned int>(values[index]));
		if (written < 0 || static_cast<std::size_t>(written) >= sizeof(output) - used)
		{
			print("ok=false error=response-too-large");
			return;
		}
		used += static_cast<std::size_t>(written);
	}
	if (used + 2 > sizeof(output))
	{
		print("ok=false error=response-too-large");
		return;
	}
	output[used] = ']';
	output[used + 1] = '\0';
	print(output);
}

void CliAdapter::handleModbusWriteRegister(char *const arguments) noexcept
{
	if (!mutatingCommandAllowed())
	{
		print("ok=false error=not-authorized");
		return;
	}
	if (embeddedCliGetTokenCount(arguments) != 3)
	{
		print("ok=false error=usage usage=modbus-write-register_[unit]_[address]_[value]");
		return;
	}
	std::uint8_t destination{0};
	std::uint16_t address{0};
	std::uint16_t value{0};
	if (!parseUint8(embeddedCliGetToken(arguments, 1), 247, destination) || destination == 0 ||
		!parseUint16(embeddedCliGetToken(arguments, 2), std::numeric_limits<std::uint16_t>::max(), address) ||
		!parseUint16(embeddedCliGetToken(arguments, 3), std::numeric_limits<std::uint16_t>::max(), value))
	{
		print("ok=false error=invalid-argument");
		return;
	}
	const auto result = dependencies_.modbus.writeSingleRegister(
		dependencies_.modbus.context, destination, address, value);
	if (result != ports::ModbusClientResult::Success)
	{
		char output[64]{};
		std::snprintf(output, sizeof(output), "ok=false error=%s", clientError(result));
		print(output);
		return;
	}
	print("ok=true result=written");
}

void CliAdapter::handleGetKnx(char *const arguments) noexcept
{
	const auto tokenCount = embeddedCliGetTokenCount(arguments);
	if (tokenCount == 0 || (tokenCount == 1 && std::strcmp(embeddedCliGetToken(arguments, 1), "general") == 0))
	{
		printKnxGeneral();
		return;
	}
	if (tokenCount == 2 && std::strcmp(embeddedCliGetToken(arguments, 1), "channel") == 0)
	{
		std::uint8_t channel{0};
		if (!parseChannel(embeddedCliGetToken(arguments, 2), channel))
		{
			print("ok=false error=invalid-channel");
			return;
		}
		printKnxChannel(channel);
		return;
	}
	print("ok=false error=usage usage=get-knx_[general|channel_0..5]");
}

void CliAdapter::handleSetKnx(char *const arguments) noexcept
{
	if (!mutatingCommandAllowed())
	{
		print("ok=false error=not-authorized");
		return;
	}
	if (embeddedCliGetTokenCount(arguments) != 2)
	{
		print("ok=false error=usage usage=set-knx_[parameter]_[value]");
		return;
	}
	const auto *const parameter = embeddedCliGetToken(arguments, 1);
	const auto *const value = embeddedCliGetToken(arguments, 2);
	auto configuration = dependencies_.configurationService->active();
	auto &knx = configuration.knx;
	bool booleanValue{false};
	std::uint16_t uint16Value{0};
	std::uint32_t uint32Value{0};
	bool validValue{false};
	if (std::strcmp(parameter, "enabled") == 0)
	{
		validValue = parseBoolean(value, booleanValue);
		knx.enabled = booleanValue;
	}
	else if (std::strcmp(parameter, "individual-address") == 0)
	{
		validValue = parseIndividualAddress(value, uint16Value);
		knx.individualAddress = uint16Value;
	}
	else if (std::strcmp(parameter, "startup-delay-ms") == 0)
	{
		validValue = parseUint32(value, uint32Value);
		knx.startupTransmitDelayMs = uint32Value;
	}
	else if (std::strcmp(parameter, "telegram-interval-ms") == 0)
	{
		validValue = parseUint16(value, 1000, uint16Value);
		knx.minimumTelegramIntervalMs = uint16Value;
	}
	else if (std::strcmp(parameter, "cyclic-status-ms") == 0)
	{
		validValue = parseUint32(value, uint32Value);
		knx.cyclicStatusIntervalMs = uint32Value;
	}
	else if (std::strcmp(parameter, "heartbeat-interval-ms") == 0)
	{
		validValue = parseUint32(value, uint32Value);
		knx.heartbeatIntervalMs = uint32Value;
	}
	else if (std::strcmp(parameter, "read-switch") == 0)
	{
		validValue = parseBoolean(value, booleanValue);
		knx.readSwitchObject = booleanValue;
	}
	else if (std::strcmp(parameter, "heartbeat-ga") == 0)
	{
		validValue = parseGroupAddress(value, uint16Value);
		knx.heartbeatGroupAddress = uint16Value;
	}
	else if (std::strcmp(parameter, "central-switch-ga") == 0)
	{
		validValue = parseGroupAddress(value, uint16Value);
		knx.centralSwitchGroupAddress = uint16Value;
	}
	else if (std::strcmp(parameter, "central-off-ga") == 0)
	{
		validValue = parseGroupAddress(value, uint16Value);
		knx.centralOffGroupAddress = uint16Value;
	}
	else if (std::strcmp(parameter, "device-fault-ga") == 0)
	{
		validValue = parseGroupAddress(value, uint16Value);
		knx.deviceFaultGroupAddress = uint16Value;
	}
	else
	{
		print("ok=false error=invalid-parameter");
		return;
	}
	if (!validValue)
	{
		print("ok=false error=invalid-value");
		return;
	}
	commitConfiguration(configuration);
}

void CliAdapter::handleSetKnxChannel(char *const arguments) noexcept
{
	if (!mutatingCommandAllowed())
	{
		print("ok=false error=not-authorized");
		return;
	}
	if (embeddedCliGetTokenCount(arguments) != 3)
	{
		print("ok=false error=usage usage=set-knx-channel_[0..5]_[parameter]_[value]");
		return;
	}
	std::uint8_t channel{0};
	if (!parseChannel(embeddedCliGetToken(arguments, 1), channel))
	{
		print("ok=false error=invalid-channel");
		return;
	}
	const auto *const parameter = embeddedCliGetToken(arguments, 2);
	const auto *const value = embeddedCliGetToken(arguments, 3);
	auto configuration = dependencies_.configurationService->active();
	auto &knxChannel = configuration.knx.channels[channel];
	std::uint16_t address{0};
	bool booleanValue{false};
	bool validValue{false};
	if (std::strcmp(parameter, "switch-ga") == 0)
	{
		validValue = parseGroupAddress(value, address);
		knxChannel.switchGroupAddress = address;
	}
	else if (std::strcmp(parameter, "status-ga") == 0)
	{
		validValue = parseGroupAddress(value, address);
		knxChannel.statusGroupAddress = address;
	}
	else if (std::strcmp(parameter, "fault-ga") == 0)
	{
		validValue = parseGroupAddress(value, address);
		knxChannel.faultGroupAddress = address;
	}
	else if (std::strcmp(parameter, "command-inverted") == 0)
	{
		validValue = parseBoolean(value, booleanValue);
		knxChannel.commandPolarityInverted = booleanValue;
	}
	else if (std::strcmp(parameter, "status-inverted") == 0)
	{
		validValue = parseBoolean(value, booleanValue);
		knxChannel.statusPolarityInverted = booleanValue;
	}
	else if (std::strcmp(parameter, "startup-status") == 0)
	{
		validValue = parseBoolean(value, booleanValue);
		knxChannel.sendStatusAfterStartup = booleanValue;
	}
	else if (std::strcmp(parameter, "central-switch") == 0)
	{
		validValue = parseBoolean(value, booleanValue);
		knxChannel.participatesInCentralSwitch = booleanValue;
	}
	else if (std::strcmp(parameter, "central-off") == 0)
	{
		validValue = parseBoolean(value, booleanValue);
		knxChannel.participatesInCentralOff = booleanValue;
	}
	else
	{
		print("ok=false error=invalid-parameter");
		return;
	}
	if (!validValue)
	{
		print("ok=false error=invalid-value");
		return;
	}
	commitConfiguration(configuration);
}

void CliAdapter::handleSetWifi(char *const arguments) noexcept
{
	if (!mutatingCommandAllowed())
	{
		print("ok=false error=maintenance-authorization-required");
		return;
	}
	if (embeddedCliGetTokenCount(arguments) != 3)
	{
		print("ok=false error=usage usage=set-wifi_[profile_0..2]_[ssid]_[passphrase]");
		return;
	}
	std::uint8_t profileIndex{0};
	const auto *const profile = embeddedCliGetToken(arguments, 1);
	const auto *const ssid = embeddedCliGetToken(arguments, 2);
	const auto *const passphrase = embeddedCliGetToken(arguments, 3);
	if (!parseUint8(profile, static_cast<std::uint8_t>(domain::wifiProfileCount - 1), profileIndex) || ssid == nullptr ||
		passphrase == nullptr || !dependencies_.networkManager->provisionWifiProfile(profileIndex,
			ssid,
			passphrase,
			dependencies_.clock.nowMs()))
	{
		print("ok=false error=wifi-provisioning-failed");
		return;
	}
	char response[48]{};
	std::snprintf(response, sizeof(response), "ok=true profile=%u reconfiguring=true", static_cast<unsigned int>(profileIndex));
	print(response);
}

void CliAdapter::handleLoadConfig(char *const arguments) noexcept
{
	if (!mutatingCommandAllowed())
	{
		print("ok=false error=maintenance-authorization-required");
		return;
	}
	if (!hasNoArguments(arguments))
	{
		print("ok=false error=unexpected-argument");
		return;
	}
	domain::Configuration configuration{};
	const auto result = dependencies_.configurationFile.load(configuration);
	if (result == ports::ConfigurationSourceResult::Unavailable)
	{
		print("ok=false error=config-files-unavailable");
		return;
	}
	if (result != ports::ConfigurationSourceResult::Loaded)
	{
		print("ok=false error=invalid-config-files");
		return;
	}
	commitConfiguration(configuration);
}

void CliAdapter::handleStoreConfig(char *const arguments) noexcept
{
	if (!mutatingCommandAllowed())
	{
		print("ok=false error=maintenance-authorization-required");
		return;
	}
	if (!hasNoArguments(arguments))
	{
		print("ok=false error=unexpected-argument");
		return;
	}
	const auto result = dependencies_.configurationFile.store(dependencies_.configurationService->active());
	if (result == ports::ConfigurationFileStoreResult::Stored)
	{
		print("ok=true result=config-files-stored");
		return;
	}
	if (result == ports::ConfigurationFileStoreResult::Unavailable)
	{
		print("ok=false error=config-files-unavailable");
		return;
	}
	if (result == ports::ConfigurationFileStoreResult::InvalidConfiguration)
	{
		print("ok=false error=invalid-configuration");
		return;
	}
	print("ok=false error=config-files-io-failure");
}

void CliAdapter::commitConfiguration(const domain::Configuration &configuration) noexcept
{
	dependencies_.configurationService->discardStaged();
	if (dependencies_.configurationService->stage(configuration) != app::ConfigurationStageResult::Staged)
	{
		char output[96]{};
		std::snprintf(output,
			sizeof(output),
			"ok=false error=invalid-configuration validation=%u",
			static_cast<unsigned int>(dependencies_.configurationService->lastValidationError()));
		print(output);
		return;
	}
	const auto result = dependencies_.configurationService->commit();
	if (result == app::ConfigurationCommitResult::PersistenceFailure)
	{
		dependencies_.configurationService->discardStaged();
		print("ok=false error=persistence-failure");
		return;
	}
	if (result == app::ConfigurationCommitResult::NothingStaged)
	{
		print("ok=false error=configuration-state");
		return;
	}
	const auto &active = dependencies_.configurationService->active();
	dependencies_.diagnostics->updateConfiguration(true,
		active.generation,
		dependencies_.configurationService->lastLoadResult(),
		dependencies_.configurationService->lastSaveResult());
	char output[128]{};
	std::snprintf(output,
		sizeof(output),
		"ok=true result=committed generation=%lu restart_required=%s",
		static_cast<unsigned long>(active.generation),
		result == app::ConfigurationCommitResult::CommittedRestartRequired ? "true" : "false");
	print(output);
}

void CliAdapter::handleReboot(char *const arguments) noexcept
{
	if (!mutatingCommandAllowed())
	{
		print("ok=false error=not-authorized");
		return;
	}
	if (!hasNoArguments(arguments))
	{
		print("ok=false error=unexpected-argument");
		return;
	}
	const auto result = dependencies_.lifecycleSupervisor->requestRestart(dependencies_.clock.nowMs());
	if (result == app::LifecycleResult::Applied || result == app::LifecycleResult::EventRejected)
	{
		print("ok=true result=restart-requested");
		return;
	}
	print("ok=false error=invalid-lifecycle");
}
}