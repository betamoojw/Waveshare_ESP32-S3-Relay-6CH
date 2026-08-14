#pragma once

#include "modbus_rtu_adapter.h"
#include "../../app/configuration_service.h"
#include "../../app/diagnostics_service.h"
#include "../../app/lifecycle_supervisor.h"
#include "../../app/relay_command_queue.h"
#include "../../app/relay_command_service.h"

#include <cstdint>

namespace switch_actuator::adapters::modbus
{
using SnapshotExtensionProvider = bool (*)(void *context, RegisterMapSnapshot &snapshot) noexcept;
using NonRelayWriteHandler = WriteBatchResult (*)(void *context, const HoldingWriteBatch &batch) noexcept;

struct ModbusApplicationGatewayDependencies final
{
	app::RelayCommandQueue *commandQueue{nullptr};
	const app::RelayCommandService *relayService{nullptr};
	const app::ConfigurationService *configurationService{nullptr};
	const app::LifecycleSupervisor *lifecycleSupervisor{nullptr};
	const app::DiagnosticsService *diagnostics{nullptr};
	SnapshotExtensionProvider snapshotExtensionProvider{nullptr};
	void *snapshotExtensionContext{nullptr};
	NonRelayWriteHandler nonRelayWriteHandler{nullptr};
	void *nonRelayWriteContext{nullptr};
};

class ModbusApplicationGateway final
{
public:
	explicit ModbusApplicationGateway(ModbusApplicationGatewayDependencies dependencies) noexcept;

	[[nodiscard]] bool isValid() const noexcept;
	[[nodiscard]] static bool provideSnapshot(void *context, RegisterMapSnapshot &snapshot) noexcept;
	[[nodiscard]] static WriteBatchResult handleWriteBatch(void *context, const HoldingWriteBatch &batch) noexcept;

private:
	[[nodiscard]] bool buildSnapshot(RegisterMapSnapshot &snapshot) const noexcept;
	[[nodiscard]] WriteBatchResult submit(const HoldingWriteBatch &batch) noexcept;
	[[nodiscard]] static std::uint16_t saturateToRegister(std::uint32_t value) noexcept;

	ModbusApplicationGatewayDependencies dependencies_;
};
}