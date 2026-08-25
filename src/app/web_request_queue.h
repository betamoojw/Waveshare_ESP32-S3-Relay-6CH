#pragma once

#include "wifi_management_service.h"
#include "../domain/relay_types.h"
#include "../ports/modbus_rtu_control_port.h"
#include "../ports/web_security_store.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace switch_actuator::app
{
enum class WebRequestType : std::uint8_t
{
	RelayCommand,
	WifiScan,
	WifiSaveProfile,
	WifiRemoveProfile,
	WifiMoveProfile,
	WifiConnectProfile,
	WifiSaveRecoveryAp,
	SaveModbusConfiguration,
	SetModbusRole,
	SaveKnxConfiguration,
	SaveUser,
	Restart
};

struct WebApplicationRequest final
{
	WebRequestType type{WebRequestType::RelayCommand};
	std::uint32_t operationId{0};
	std::uint32_t sessionId{0};
	std::uint32_t correlationId{0};
	domain::RelayChannelId channel{0};
	domain::RelayAction action{domain::RelayAction::SetOff};
	WifiProfilePatch wifiProfile{};
	domain::RecoveryApConfiguration recoveryAp{};
	domain::ModbusConfiguration modbusConfiguration{};
	ports::ModbusRtuRole modbusRole{ports::ModbusRtuRole::Server};
	domain::KnxConfiguration knxConfiguration{};
	std::uint32_t expectedGeneration{0};
	std::uint8_t index{0};
	std::uint8_t toIndex{0};
	std::uint32_t userId{0};
	std::array<char, ports::webUsernameCapacity> username{};
	ports::WebUserRole userRole{ports::WebUserRole::Guest};
	bool userEnabled{false};
	bool replacePassword{false};
	std::array<char, 129> password{};
	std::uint32_t receivedAtMs{0};
};

enum class WebOperationStatus : std::uint8_t
{
	Pending,
	Applied,
	Conflict,
	Invalid,
	Unavailable,
	Rejected
};

struct WebOperationResult final
{
	bool active{false};
	std::uint32_t operationId{0};
	std::uint32_t sessionId{0};
	WebOperationStatus status{WebOperationStatus::Pending};
	std::uint32_t createdAtMs{0};
	std::uint32_t completedAtMs{0};
};

class WebRequestQueue final
{
public:
	static constexpr std::size_t capacity{8};
	static constexpr std::size_t resultCapacity{16};
	static constexpr std::uint32_t resultRetentionMs{60'000};

	[[nodiscard]] bool enqueue(const WebApplicationRequest &request) noexcept;
	[[nodiscard]] bool dequeue(WebApplicationRequest &request) noexcept;
	[[nodiscard]] bool complete(std::uint32_t operationId,
		WebOperationStatus status,
		std::uint32_t nowMs) noexcept;
	[[nodiscard]] bool findResult(std::uint32_t sessionId,
		std::uint32_t operationId,
		WebOperationResult &result) const noexcept;
	void expire(std::uint32_t nowMs) noexcept;
	void clear() noexcept;
	[[nodiscard]] std::size_t size() const noexcept;
	[[nodiscard]] std::size_t highWaterMark() const noexcept;

private:
	mutable std::mutex mutex_{};
	std::array<WebApplicationRequest, capacity> requests_{};
	std::array<WebOperationResult, resultCapacity> results_{};
	std::size_t head_{0};
	std::size_t tail_{0};
	std::size_t size_{0};
	std::size_t highWaterMark_{0};
};
}