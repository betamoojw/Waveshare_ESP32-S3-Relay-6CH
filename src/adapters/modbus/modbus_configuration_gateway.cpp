#include "modbus_configuration_gateway.h"

#include "../../app/error_mapping.h"

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
	return context != nullptr ? static_cast<ModbusConfigurationGateway *>(context)->apply(batch) :
		WriteBatchResult{domain::ErrorCode::InternalError};
}

WriteBatchResult ModbusConfigurationGateway::apply(const HoldingWriteBatch &batch) noexcept
{
	if (!isValid())
	{
		return domain::ErrorCode::InternalError;
	}
	if (batch.kind == HoldingWriteKind::UnitId)
	{
		return applyUnitId(batch.unitId);
	}
	if (batch.kind == HoldingWriteKind::UartSettings)
	{
		return applyUartSettings(batch.uartEncodedSettings);
	}
	return domain::ErrorCode::InvalidArgument;
}

WriteBatchResult ModbusConfigurationGateway::applyUnitId(const std::uint8_t unitId) noexcept
{
	if (unitId < 1 || unitId > 247)
	{
		return domain::ErrorCode::InvalidArgument;
	}

	auto replacement = dependencies_.configurationService->active();
	replacement.modbus.unitId = unitId;
	return commit(replacement);
}

WriteBatchResult ModbusConfigurationGateway::applyUartSettings(const std::uint16_t encodedSettings) noexcept
{
	auto replacement = dependencies_.configurationService->active();
	if (!ModbusRegisterMap::decodeUartSettings(encodedSettings, replacement.modbus))
	{
		return domain::ErrorCode::InvalidArgument;
	}
	return commit(replacement);
}

WriteBatchResult ModbusConfigurationGateway::commit(domain::Configuration replacement) noexcept
{
	if (dependencies_.configurationService->stage(replacement) != app::ConfigurationStageResult::Staged)
	{
		return domain::ErrorCode::ConfigurationError;
	}

	const auto commitResult = dependencies_.configurationService->commit();
	updateConfigurationDiagnostics();
	if (commitResult == app::ConfigurationCommitResult::PersistenceFailure)
	{
		static_cast<void>(dependencies_.diagnostics->recordFault(domain::FaultCode::SettingsSaveFailure,
			domain::FaultSeverity::Warning,
			dependencies_.clock.nowMs()));
		return domain::ErrorCode::StorageError;
	}
	if (commitResult != app::ConfigurationCommitResult::CommittedRestartRequired &&
		commitResult != app::ConfigurationCommitResult::Committed)
	{
		return domain::ErrorCode::InternalError;
	}

	static_cast<void>(dependencies_.diagnostics->clearFault(domain::FaultCode::SettingsSaveFailure));
	if (commitResult == app::ConfigurationCommitResult::CommittedRestartRequired)
	{
		const auto restartResult = dependencies_.lifecycleSupervisor->requestRestart(dependencies_.clock.nowMs());
		if (restartResult == app::LifecycleResult::InvalidTransition || restartResult == app::LifecycleResult::InvalidEventSink)
		{
			return domain::ErrorCode::InternalError;
		}
	}
	return std::nullopt;
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