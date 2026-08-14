#include "modbus_register_map.h"

#include <limits>

namespace switch_actuator::adapters::modbus
{
RegisterMapResult ModbusRegisterMap::readCoils(const std::uint16_t address,
												  const std::uint16_t quantity,
												  const RegisterMapSnapshot &snapshot,
												  bool *const values,
												  const std::size_t valueCapacity) const noexcept
{
	const auto outputResult = validateOutput(values, quantity, valueCapacity);
	if (outputResult != RegisterMapResult::Success)
	{
		return outputResult;
	}
	if (!rangeWithin(address, quantity, relayCoilAddress, domain::relayChannelCount))
	{
		return RegisterMapResult::IllegalAddress;
	}

	const auto offset = static_cast<std::size_t>(address - relayCoilAddress);
	for (std::size_t index = 0; index < quantity; ++index)
	{
		values[index] = snapshot.relays[offset + index].appliedState == domain::RelayState::On;
	}
	return RegisterMapResult::Success;
}

RegisterMapResult ModbusRegisterMap::readDiscreteInputs(const std::uint16_t address,
															 const std::uint16_t quantity,
															 const RegisterMapSnapshot &snapshot,
															 bool *const values,
															 const std::size_t valueCapacity) const noexcept
{
	return readCoils(address, quantity, snapshot, values, valueCapacity);
}

RegisterMapResult ModbusRegisterMap::readHoldingRegisters(const std::uint16_t address,
															const std::uint16_t quantity,
															const RegisterMapSnapshot &snapshot,
															std::uint16_t *const values,
															const std::size_t valueCapacity) const noexcept
{
	const auto outputResult = validateOutput(values, quantity, valueCapacity);
	if (outputResult != RegisterMapResult::Success)
	{
		return outputResult;
	}

	if (rangeWithin(address, quantity, relayHoldingAddress, domain::relayChannelCount))
	{
		const auto offset = static_cast<std::size_t>(address - relayHoldingAddress);
		for (std::size_t index = 0; index < quantity; ++index)
		{
			values[index] = snapshot.relays[offset + index].appliedState == domain::RelayState::On ? 1 : 0;
		}
		return RegisterMapResult::Success;
	}
	if (rangeWithin(address, quantity, indicatorHoldingAddress, 4))
	{
		const std::array<std::uint16_t, 4> indicator{snapshot.indicator.red,
														 snapshot.indicator.green,
														 snapshot.indicator.blue,
														 snapshot.indicator.brightness};
		const auto offset = static_cast<std::size_t>(address - indicatorHoldingAddress);
		for (std::size_t index = 0; index < quantity; ++index)
		{
			values[index] = indicator[offset + index];
		}
		return RegisterMapResult::Success;
	}
	if (quantity == 1 && address == uartSettingsHoldingAddress)
	{
		if (!snapshot.uartEncodedSettingsAvailable)
		{
			return RegisterMapResult::IllegalAddress;
		}
		values[0] = snapshot.uartEncodedSettings;
		return RegisterMapResult::Success;
	}
	if (quantity == 1 && address == unitIdHoldingAddress)
	{
		values[0] = snapshot.unitId;
		return RegisterMapResult::Success;
	}
	if (quantity == 1 && address == softwareVersionHoldingAddress)
	{
		values[0] = snapshot.softwareVersion;
		return RegisterMapResult::Success;
	}
	return RegisterMapResult::IllegalAddress;
}

RegisterMapResult ModbusRegisterMap::readInputRegisters(const std::uint16_t address,
														  const std::uint16_t quantity,
														  const RegisterMapSnapshot &snapshot,
														  std::uint16_t *const values,
														  const std::size_t valueCapacity) const noexcept
{
	const auto outputResult = validateOutput(values, quantity, valueCapacity);
	if (outputResult != RegisterMapResult::Success)
	{
		return outputResult;
	}

	if (rangeWithin(address, quantity, relayFaultInputAddress, domain::relayChannelCount))
	{
		const auto offset = static_cast<std::size_t>(address - relayFaultInputAddress);
		for (std::size_t index = 0; index < quantity; ++index)
		{
			values[index] = static_cast<std::uint16_t>(snapshot.relays[offset + index].fault);
		}
		return RegisterMapResult::Success;
	}
	if (quantity == 1 && address == lifecycleInputAddress)
	{
		values[0] = static_cast<std::uint16_t>(snapshot.lifecycleState);
		return RegisterMapResult::Success;
	}
	if (rangeWithin(address, quantity, uptimeInputAddress, 2))
	{
		const std::array<std::uint16_t, 2> uptime{static_cast<std::uint16_t>(snapshot.uptimeSeconds >> 16U),
													 static_cast<std::uint16_t>(snapshot.uptimeSeconds)};
		const auto offset = static_cast<std::size_t>(address - uptimeInputAddress);
		for (std::size_t index = 0; index < quantity; ++index)
		{
			values[index] = uptime[offset + index];
		}
		return RegisterMapResult::Success;
	}
	if (rangeWithin(address, quantity, commandCountersInputAddress, 2))
	{
		const std::array<std::uint16_t, 2> counters{snapshot.acceptedCommandCount, snapshot.rejectedCommandCount};
		const auto offset = static_cast<std::size_t>(address - commandCountersInputAddress);
		for (std::size_t index = 0; index < quantity; ++index)
		{
			values[index] = counters[offset + index];
		}
		return RegisterMapResult::Success;
	}
	return RegisterMapResult::IllegalAddress;
}

RegisterMapResult ModbusRegisterMap::parseCoilWrite(const std::uint16_t address,
													 const bool *const values,
													 const std::uint16_t quantity,
													 const domain::CommandSource source,
													 const std::uint32_t firstCorrelationId,
													 const std::uint32_t receivedAtMs,
													 HoldingWriteBatch &batch) const noexcept
{
	if (validateOutput(values, quantity, quantity) != RegisterMapResult::Success)
	{
		return RegisterMapResult::InvalidBuffer;
	}
	if (!rangeWithin(address, quantity, relayCoilAddress, domain::relayChannelCount))
	{
		return RegisterMapResult::IllegalAddress;
	}
	if (quantity - 1U > std::numeric_limits<std::uint32_t>::max() - firstCorrelationId)
	{
		return RegisterMapResult::IllegalValue;
	}

	batch = {};
	batch.kind = HoldingWriteKind::RelayCommands;
	batch.relayCommandCount = quantity;
	const auto channelOffset = static_cast<std::uint8_t>(address - relayCoilAddress);
	for (std::uint8_t index = 0; index < quantity; ++index)
	{
		batch.relayCommands[index] = domain::RelayCommand{domain::RelayChannelId{static_cast<std::uint8_t>(channelOffset + index)},
			values[index] ? domain::RelayAction::SetOn : domain::RelayAction::SetOff,
			source,
			firstCorrelationId + index,
			receivedAtMs};
	}
	return RegisterMapResult::Success;
}

RegisterMapResult ModbusRegisterMap::parseHoldingWrite(const std::uint16_t address,
														const std::uint16_t *const values,
														const std::uint16_t quantity,
														const domain::CommandSource source,
														const std::uint32_t firstCorrelationId,
														const std::uint32_t receivedAtMs,
														HoldingWriteBatch &batch) const noexcept
{
	if (validateOutput(values, quantity, quantity) != RegisterMapResult::Success)
	{
		return RegisterMapResult::InvalidBuffer;
	}

	batch = {};
	if (rangeWithin(address, quantity, relayHoldingAddress, domain::relayChannelCount))
	{
		if (quantity - 1U > std::numeric_limits<std::uint32_t>::max() - firstCorrelationId)
		{
			return RegisterMapResult::IllegalValue;
		}
		for (std::size_t index = 0; index < quantity; ++index)
		{
			if (values[index] > 2)
			{
				return RegisterMapResult::IllegalValue;
			}
		}

		batch.kind = HoldingWriteKind::RelayCommands;
		batch.relayCommandCount = quantity;
		const auto channelOffset = static_cast<std::uint8_t>(address - relayHoldingAddress);
		for (std::uint8_t index = 0; index < quantity; ++index)
		{
			const auto action = values[index] == 0 ? domain::RelayAction::SetOff
												 : values[index] == 1 ? domain::RelayAction::SetOn : domain::RelayAction::Toggle;
			batch.relayCommands[index] = domain::RelayCommand{domain::RelayChannelId{static_cast<std::uint8_t>(channelOffset + index)},
				action,
				source,
				firstCorrelationId + index,
				receivedAtMs};
		}
		return RegisterMapResult::Success;
	}
	if (rangeWithin(address, quantity, indicatorHoldingAddress, 4))
	{
		for (std::size_t index = 0; index < quantity; ++index)
		{
			if (values[index] > 255)
			{
				return RegisterMapResult::IllegalValue;
			}
		}
		batch.kind = HoldingWriteKind::Indicator;
		const auto offset = static_cast<std::size_t>(address - indicatorHoldingAddress);
		for (std::size_t index = 0; index < quantity; ++index)
		{
			const auto component = offset + index;
			const auto value = static_cast<std::uint8_t>(values[index]);
			if (component == 0)
			{
				batch.indicator.red = value;
			}
			else if (component == 1)
			{
				batch.indicator.green = value;
			}
			else if (component == 2)
			{
				batch.indicator.blue = value;
			}
			else
			{
				batch.indicator.brightness = value;
			}
			batch.indicator.updateMask = static_cast<std::uint8_t>(batch.indicator.updateMask | (1U << component));
		}
		return RegisterMapResult::Success;
	}
	if (quantity == 1 && address == buzzerHoldingAddress)
	{
		if (values[0] > 7)
		{
			return RegisterMapResult::IllegalValue;
		}
		batch.kind = HoldingWriteKind::Buzzer;
		batch.buzzerTone = static_cast<std::uint8_t>(values[0]);
		return RegisterMapResult::Success;
	}
	if (quantity == 1 && address == uartSettingsHoldingAddress)
	{
		batch.kind = HoldingWriteKind::UartSettings;
		batch.uartEncodedSettings = values[0];
		return RegisterMapResult::Success;
	}
	if (quantity == 1 && address == unitIdHoldingAddress)
	{
		if (values[0] < 1 || values[0] > 247)
		{
			return RegisterMapResult::IllegalValue;
		}
		batch.kind = HoldingWriteKind::UnitId;
		batch.unitId = static_cast<std::uint8_t>(values[0]);
		return RegisterMapResult::Success;
	}
	return RegisterMapResult::IllegalAddress;
}

bool ModbusRegisterMap::rangeWithin(const std::uint16_t address,
									const std::uint16_t quantity,
									const std::uint16_t base,
									const std::uint16_t count) noexcept
{
	if (quantity == 0 || address < base)
	{
		return false;
	}
	const auto offset = static_cast<std::uint16_t>(address - base);
	return offset <= count && quantity <= static_cast<std::uint16_t>(count - offset);
}

RegisterMapResult ModbusRegisterMap::validateOutput(const void *const values,
																const std::uint16_t quantity,
																const std::size_t capacity) noexcept
{
	return values != nullptr && quantity > 0 && quantity <= capacity ? RegisterMapResult::Success : RegisterMapResult::InvalidBuffer;
}
}