#pragma once

#include "modbus_rtu_adapter.h"
#include "../../app/configuration_service.h"
#include "../../app/diagnostics_service.h"
#include "../../app/lifecycle_supervisor.h"
#include "../../ports/clock_port.h"

namespace switch_actuator::adapters::modbus
{
struct ModbusConfigurationGatewayDependencies final
{
	app::ConfigurationService *configurationService{nullptr};
	app::LifecycleSupervisor *lifecycleSupervisor{nullptr};
	app::DiagnosticsService *diagnostics{nullptr};
	ports::ClockPort clock{};
};

class ModbusConfigurationGateway final
{
public:
	explicit ModbusConfigurationGateway(ModbusConfigurationGatewayDependencies dependencies) noexcept;

	[[nodiscard]] bool isValid() const noexcept;
	[[nodiscard]] static WriteBatchResult handle(void *context, const HoldingWriteBatch &batch) noexcept;

private:
	[[nodiscard]] WriteBatchResult apply(const HoldingWriteBatch &batch) noexcept;
	[[nodiscard]] WriteBatchResult applyUnitId(std::uint8_t unitId) noexcept;
	void updateConfigurationDiagnostics() noexcept;

	ModbusConfigurationGatewayDependencies dependencies_;
};
}