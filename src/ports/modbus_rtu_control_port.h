#pragma once

#include <cstddef>
#include <cstdint>

namespace switch_actuator::ports
{
enum class ModbusRtuRole : std::uint8_t
{
	Server,
	Client
};

enum class ModbusClientResult : std::uint8_t
{
	Success,
	InvalidArgument,
	WrongRole,
	NotInitialized,
	ProtocolError,
	TransportError
};

using ModbusRoleHandler = bool (*)(void *context, ModbusRtuRole role) noexcept;
using ModbusRoleProvider = ModbusRtuRole (*)(const void *context) noexcept;
using ModbusReadHoldingHandler = ModbusClientResult (*)(void *context,
	std::uint8_t destination,
	std::uint16_t address,
	std::uint16_t quantity,
	std::uint16_t *output,
	std::size_t outputCapacity) noexcept;
using ModbusWriteRegisterHandler = ModbusClientResult (*)(void *context,
	std::uint8_t destination,
	std::uint16_t address,
	std::uint16_t value) noexcept;

struct ModbusRtuControlPort final
{
	void *context{nullptr};
	ModbusRoleHandler setRole{nullptr};
	ModbusRoleProvider role{nullptr};
	ModbusReadHoldingHandler readHoldingRegisters{nullptr};
	ModbusWriteRegisterHandler writeSingleRegister{nullptr};

	[[nodiscard]] bool isValid() const noexcept
	{
		return context != nullptr && setRole != nullptr && role != nullptr && readHoldingRegisters != nullptr &&
			   writeSingleRegister != nullptr;
	}
};
}