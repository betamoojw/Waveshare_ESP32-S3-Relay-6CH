#pragma once

#include "../../domain/version_compatibility.h"

#include <cstddef>
#include <cstdint>

namespace switch_actuator::adapters::web::api_v1
{
inline constexpr char basePath[]{"/api/v1"};
inline constexpr char version[]{"1.0"};
inline constexpr auto compatibilityVersion{domain::compatibility::api.label};
inline constexpr char minimumUiVersion[]{"1.0.0"};

inline constexpr std::size_t maximumRequestBodyBytes{2048};
inline constexpr std::size_t maximumWebSocketFrameBytes{2048};
inline constexpr std::size_t maximumWebSocketClients{2};
inline constexpr std::uint32_t webSocketIdleTimeoutMs{60'000};
inline constexpr std::uint8_t relayMutationsPerSecond{10};
inline constexpr std::uint8_t configurationMutationsPerSecond{2};

namespace route
{
inline constexpr char session[]{"/api/v1/session"};
inline constexpr char capabilities[]{"/api/v1/capabilities"};
inline constexpr char status[]{"/api/v1/status"};
inline constexpr char device[]{"/api/v1/device"};
inline constexpr char diagnostics[]{"/api/v1/diagnostics"};
inline constexpr char network[]{"/api/v1/network"};
inline constexpr char relays[]{"/api/v1/relays"};
inline constexpr char relay[]{"/api/v1/relays/*"};
inline constexpr char relayCommands[]{"/api/v1/relays/*/commands"};
inline constexpr char wifi[]{"/api/v1/network/wifi"};
inline constexpr char wifiScan[]{"/api/v1/network/wifi/scan"};
inline constexpr char wifiProfiles[]{"/api/v1/network/wifi/profiles/*"};
inline constexpr char wifiProfilesPrefix[]{"/api/v1/network/wifi/profiles/"};
inline constexpr char wifiProfileMove[]{"/api/v1/network/wifi/profiles/*/move"};
inline constexpr char wifiProfileConnect[]{"/api/v1/network/wifi/profiles/*/connect"};
inline constexpr char wifiRecoveryAp[]{"/api/v1/network/wifi/recovery-ap"};
inline constexpr char modbus[]{"/api/v1/protocols/modbus"};
inline constexpr char modbusRole[]{"/api/v1/protocols/modbus/role"};
inline constexpr char knx[]{"/api/v1/protocols/knx"};
inline constexpr char users[]{"/api/v1/users"};
inline constexpr char user[]{"/api/v1/users/*"};
inline constexpr char usersPrefix[]{"/api/v1/users/"};
inline constexpr char restart[]{"/api/v1/maintenance/restart"};
inline constexpr char reboot[]{"/api/v1/reboot"};
inline constexpr char factoryReset[]{"/api/v1/factory-reset"};
inline constexpr char ota[]{"/api/v1/ota"};
inline constexpr char commands[]{"/api/v1/commands/*"};
inline constexpr char commandsPrefix[]{"/api/v1/commands/"};
inline constexpr char operations[]{"/api/v1/operations/*"};
inline constexpr char operationsPrefix[]{"/api/v1/operations/"};
inline constexpr char relaysPrefix[]{"/api/v1/relays/"};
inline constexpr char webSocket[]{"/api/v1/ws"};
}
}