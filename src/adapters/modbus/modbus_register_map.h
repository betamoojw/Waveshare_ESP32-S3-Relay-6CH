#pragma once

#include "../../app/lifecycle_supervisor.h"
#include "../../domain/relay_types.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace switch_actuator::adapters::modbus
{
enum class RegisterMapResult : std::uint8_t
{
	Success,
	IllegalAddress,
	IllegalValue,
	InvalidBuffer
};

enum class HoldingWriteKind : std::uint8_t
{
	None,
	RelayCommands,
	Indicator,
	Buzzer,
	UartSettings,
	UnitId
};

struct IndicatorRegisterCommand final
{
	std::uint8_t red{0};
	std::uint8_t green{0};
	std::uint8_t blue{0};
	std::uint8_t brightness{0};
	std::uint8_t updateMask{0};
};

struct HoldingWriteBatch final
{
	HoldingWriteKind kind{HoldingWriteKind::None};
	std::array<domain::RelayCommand, domain::relayChannelCount> relayCommands{};
	std::size_t relayCommandCount{0};
	IndicatorRegisterCommand indicator{};
	std::uint8_t buzzerTone{0};
	std::uint16_t uartEncodedSettings{0};
	std::uint8_t unitId{0};
};

struct RegisterMapSnapshot final
{
	std::array<domain::RelaySnapshot, domain::relayChannelCount> relays{};
	IndicatorRegisterCommand indicator{};
	std::uint16_t uartEncodedSettings{0};
	bool uartEncodedSettingsAvailable{false};
	std::uint8_t unitId{10};
	std::uint16_t softwareVersion{0};
	app::LifecycleState lifecycleState{app::LifecycleState::Booting};
	std::uint32_t uptimeSeconds{0};
	std::uint16_t acceptedCommandCount{0};
	std::uint16_t rejectedCommandCount{0};
};

class ModbusRegisterMap final
{
public:
	static constexpr std::uint16_t relayCoilAddress{0};
	static constexpr std::uint16_t relayDiscreteInputAddress{0};
	static constexpr std::uint16_t relayHoldingAddress{32};
	static constexpr std::uint16_t indicatorHoldingAddress{48};
	static constexpr std::uint16_t buzzerHoldingAddress{56};
	static constexpr std::uint16_t uartSettingsHoldingAddress{128};
	static constexpr std::uint16_t unitIdHoldingAddress{130};
	static constexpr std::uint16_t softwareVersionHoldingAddress{132};
	static constexpr std::uint16_t relayFaultInputAddress{0};
	static constexpr std::uint16_t lifecycleInputAddress{8};
	static constexpr std::uint16_t uptimeInputAddress{9};
	static constexpr std::uint16_t commandCountersInputAddress{11};

	[[nodiscard]] RegisterMapResult readCoils(std::uint16_t address,
											 std::uint16_t quantity,
											 const RegisterMapSnapshot &snapshot,
											 bool *values,
											 std::size_t valueCapacity) const noexcept;
	[[nodiscard]] RegisterMapResult readDiscreteInputs(std::uint16_t address,
													  std::uint16_t quantity,
													  const RegisterMapSnapshot &snapshot,
													  bool *values,
													  std::size_t valueCapacity) const noexcept;
	[[nodiscard]] RegisterMapResult readHoldingRegisters(std::uint16_t address,
													 std::uint16_t quantity,
													 const RegisterMapSnapshot &snapshot,
													 std::uint16_t *values,
													 std::size_t valueCapacity) const noexcept;
	[[nodiscard]] RegisterMapResult readInputRegisters(std::uint16_t address,
												   std::uint16_t quantity,
												   const RegisterMapSnapshot &snapshot,
												   std::uint16_t *values,
												   std::size_t valueCapacity) const noexcept;
	[[nodiscard]] RegisterMapResult parseCoilWrite(std::uint16_t address,
												  const bool *values,
												  std::uint16_t quantity,
												  domain::CommandSource source,
												  std::uint32_t firstCorrelationId,
												  std::uint32_t receivedAtMs,
												  HoldingWriteBatch &batch) const noexcept;
	[[nodiscard]] RegisterMapResult parseHoldingWrite(std::uint16_t address,
													 const std::uint16_t *values,
													 std::uint16_t quantity,
													 domain::CommandSource source,
													 std::uint32_t firstCorrelationId,
													 std::uint32_t receivedAtMs,
													 HoldingWriteBatch &batch) const noexcept;

private:
	[[nodiscard]] static bool rangeWithin(std::uint16_t address,
										std::uint16_t quantity,
										std::uint16_t base,
										std::uint16_t count) noexcept;
	[[nodiscard]] static RegisterMapResult validateOutput(const void *values,
														 std::uint16_t quantity,
														 std::size_t capacity) noexcept;
};
}