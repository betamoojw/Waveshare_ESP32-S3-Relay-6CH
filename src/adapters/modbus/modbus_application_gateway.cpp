#include "modbus_application_gateway.h"

#include <limits>

namespace switch_actuator::adapters::modbus
{
ModbusApplicationGateway::ModbusApplicationGateway(const ModbusApplicationGatewayDependencies dependencies) noexcept
	: dependencies_{dependencies}
{
}

bool ModbusApplicationGateway::isValid() const noexcept
{
	return dependencies_.commandQueue != nullptr && dependencies_.relayService != nullptr &&
		   dependencies_.configurationService != nullptr && dependencies_.lifecycleSupervisor != nullptr &&
		   dependencies_.diagnostics != nullptr && dependencies_.snapshotExtensionProvider != nullptr;
}

bool ModbusApplicationGateway::provideSnapshot(void *const context, RegisterMapSnapshot &snapshot) noexcept
{
	if (context == nullptr)
	{
		return false;
	}
	return static_cast<const ModbusApplicationGateway *>(context)->buildSnapshot(snapshot);
}

WriteBatchResult ModbusApplicationGateway::handleWriteBatch(void *const context, const HoldingWriteBatch &batch) noexcept
{
	if (context == nullptr)
	{
		return WriteBatchResult::Failure;
	}
	return static_cast<ModbusApplicationGateway *>(context)->submit(batch);
}

bool ModbusApplicationGateway::buildSnapshot(RegisterMapSnapshot &snapshot) const noexcept
{
	if (!isValid())
	{
		return false;
	}

	snapshot = {};
	if (!dependencies_.snapshotExtensionProvider(dependencies_.snapshotExtensionContext, snapshot))
	{
		return false;
	}
	snapshot.relays = dependencies_.relayService->snapshots();
	snapshot.unitId = dependencies_.configurationService->active().modbus.unitId;
	snapshot.lifecycleState = dependencies_.lifecycleSupervisor->state();
	const auto &diagnostics = dependencies_.diagnostics->snapshot();
	snapshot.uptimeSeconds = diagnostics.uptimeMs / 1000U;
	snapshot.acceptedCommandCount = saturateToRegister(diagnostics.commands.accepted);
	snapshot.rejectedCommandCount = saturateToRegister(diagnostics.commands.rejected);
	return true;
}

WriteBatchResult ModbusApplicationGateway::submit(const HoldingWriteBatch &batch) noexcept
{
	if (!isValid())
	{
		return WriteBatchResult::Failure;
	}
	if (batch.kind != HoldingWriteKind::RelayCommands)
	{
		return dependencies_.nonRelayWriteHandler != nullptr
				   ? dependencies_.nonRelayWriteHandler(dependencies_.nonRelayWriteContext, batch)
				   : WriteBatchResult::IllegalValue;
	}
	if (!dependencies_.lifecycleSupervisor->acceptsOrdinaryCommands())
	{
		return WriteBatchResult::Failure;
	}
	if (batch.relayCommandCount == 0 || batch.relayCommandCount > batch.relayCommands.size())
	{
		return WriteBatchResult::IllegalValue;
	}

	app::RelayCommandBatch commandBatch{};
	commandBatch.count = batch.relayCommandCount;
	for (std::size_t index = 0; index < batch.relayCommandCount; ++index)
	{
		commandBatch.commands[index] = batch.relayCommands[index];
	}

	switch (dependencies_.commandQueue->enqueue(commandBatch))
	{
	case app::RelayCommandEnqueueResult::Accepted:
		return WriteBatchResult::Accepted;
	case app::RelayCommandEnqueueResult::QueueFull:
		return WriteBatchResult::QueueFull;
	case app::RelayCommandEnqueueResult::EmptyBatch:
	case app::RelayCommandEnqueueResult::TooManyCommands:
	case app::RelayCommandEnqueueResult::InvalidSafetyBatch:
	default:
		return WriteBatchResult::IllegalValue;
	}
}

std::uint16_t ModbusApplicationGateway::saturateToRegister(const std::uint32_t value) noexcept
{
	constexpr auto maximum = std::numeric_limits<std::uint16_t>::max();
	return value > maximum ? maximum : static_cast<std::uint16_t>(value);
}
}