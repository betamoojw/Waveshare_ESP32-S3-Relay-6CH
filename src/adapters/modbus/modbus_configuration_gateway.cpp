#include "modbus_configuration_gateway.h"

namespace switch_actuator::adapters::modbus
{
ModbusConfigurationGateway::ModbusConfigurationGateway(const ModbusConfigurationGatewayDependencies dependencies) noexcept
	: dependencies_{dependencies}
{
}

bool ModbusConfigurationGateway::isValid() const noexcept
{
	return dependencies_.configurationService != nullptr && dependencies_.lifecycleSupervisor != nullptr &&
		   dependencies_.diagnostics != nullptr && dependencies_.clock.isValid();
}

WriteBatchResult ModbusConfigurationGateway::handle(void *const context, const HoldingWriteBatch &batch) noexcept
{
	return context != nullptr ? static_cast<ModbusConfigurationGateway *>(context)->apply(batch) : WriteBatchResult::Failure;
}

WriteBatchResult ModbusConfigurationGateway::apply(const HoldingWriteBatch &batch) noexcept
{
	if (!isValid())
	{
		return WriteBatchResult::Failure;
	}
	if (batch.kind == HoldingWriteKind::UnitId)
	{
		return applyUnitId(batch.unitId);
	}
	return WriteBatchResult::IllegalValue;
}

WriteBatchResult ModbusConfigurationGateway::applyUnitId(const std::uint8_t unitId) noexcept
{
	if (unitId < 1 || unitId > 247)
	{
		return WriteBatchResult::IllegalValue;
	}

	auto replacement = dependencies_.configurationService->active();
	replacement.modbus.unitId = unitId;
	if (dependencies_.configurationService->stage(replacement) != app::ConfigurationStageResult::Staged)
	{
		return WriteBatchResult::IllegalValue;
	}

	const auto commitResult = dependencies_.configurationService->commit();
	updateConfigurationDiagnostics();
	if (commitResult == app::ConfigurationCommitResult::PersistenceFailure)
	{
		static_cast<void>(dependencies_.diagnostics->recordFault(domain::FaultCode::SettingsSaveFailure,
			domain::FaultSeverity::Warning,
			dependencies_.clock.nowMs()));
		return WriteBatchResult::Failure;
	}
	if (commitResult != app::ConfigurationCommitResult::CommittedRestartRequired &&
		commitResult != app::ConfigurationCommitResult::Committed)
	{
		return WriteBatchResult::Failure;
	}

	static_cast<void>(dependencies_.diagnostics->clearFault(domain::FaultCode::SettingsSaveFailure));
	if (commitResult == app::ConfigurationCommitResult::CommittedRestartRequired)
	{
		const auto restartResult = dependencies_.lifecycleSupervisor->requestRestart(dependencies_.clock.nowMs());
		if (restartResult == app::LifecycleResult::InvalidTransition || restartResult == app::LifecycleResult::InvalidEventSink)
		{
			return WriteBatchResult::Failure;
		}
	}
	return WriteBatchResult::Accepted;
}

void ModbusConfigurationGateway::updateConfigurationDiagnostics() noexcept
{
	const auto &configuration = dependencies_.configurationService->active();
	dependencies_.diagnostics->updateConfiguration(dependencies_.configurationService->hasValidActiveConfiguration(),
		configuration.generation,
		dependencies_.configurationService->lastLoadResult(),
		dependencies_.configurationService->lastSaveResult());
}
}