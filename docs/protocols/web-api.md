# Web API v1.0

Status: frozen production contract  
Base path: `/api/v1`  
Transport: HTTPS only  
Media type: `application/json; charset=utf-8`

This document defines the stable management API implemented by the device. The
firmware constants are in `src/adapters/web/web_api_v1_contract.h`; frontend
response validation is in `web/src/api/types.ts`.

## Compatibility policy

- The major version is encoded in the path. A breaking request or response
  change requires a new base path such as `/api/v2`.
- Within v1, fields and endpoints may be added. Clients must ignore unknown
  response fields and use `GET /capabilities` before offering optional features.
- Existing fields do not change type, meaning, or units within v1. Enum values
  may be added only where the client has a documented unknown-value fallback.
- Existing endpoints are retained for the supported life of v1. Deprecated
  aliases remain behaviorally equivalent and are removed only in a new major
  version.
- `apiVersion` retains the v1 semantic value `1.0`. `versions.api` reports the
  canonical `API-v1` compatibility label. `minimumUiVersion` is the
  oldest bundled UI contract accepted by the firmware.
- `Capabilities.versions` reports hardware, firmware, configuration, API,
  Modbus, KNX application, and filesystem compatibility labels. Additive
  version fields are permitted within v1.

## Authentication and authorization

`POST /session` is the only public API endpoint. A successful login returns the
redacted session and sets `__Host-switch_session` with `HttpOnly`, `Secure`,
`SameSite=Strict`, and `Path=/`. The token must not appear in URLs, logs,
WebSocket messages, or browser storage.

All other endpoints require that cookie. Every mutation additionally requires:

- `Origin` exactly matching the device HTTPS origin;
- `Host` matching the configured `<hostname>.local` identity or active device
  IPv4 address;
- `X-CSRF-Token` matching the current session value;
- the permission shown in the endpoint table.

Guests receive `relay:read`, `diagnostics:read`, and `configuration:read`.
Administrators additionally receive `relay:command`, `configuration:write`, and
`users:manage`. The `firmware:update` permission is reserved and is not granted
in v1.0.

Authentication failures return `401`; authenticated permission, Origin, Host,
and CSRF failures return `403`; mutation-rate failures return `429` with
`Retry-After: 1`. Responses do not disclose which forbidden check failed.

## Resource limits

| Limit | Value |
|---|---:|
| JSON request body | 2,048 bytes |
| WebSocket text frame | 2,048 bytes |
| Concurrent WebSocket clients | 2 |
| WebSocket idle timeout | 60 seconds |
| Relay mutations per session | 10 per second |
| Other mutations per session | 2 per second |
| Queued application operations | 8 |
| Retained operation results | 16 for 60 seconds |
| Retained relay command results | 32 for 60 seconds |
| Wi-Fi profiles | 3 |
| Wi-Fi scan results | 16 |

Oversized or malformed request bodies are rejected and never partially
applied. The implementation uses fixed-capacity buffers and queues.

## Endpoints

Paths in this table are relative to `/api/v1`.

| Method and path | Permission | Success | Contract |
|---|---|---:|---|
| `POST /session` | public | `200` | Create session from `{username,password}` |
| `GET /session` | authenticated | `200` | Current `Session` |
| `DELETE /session` | authenticated mutation | `204` | Revoke session |
| `GET /capabilities` | `relay:read` | `200` | `Capabilities` |
| `GET /status` | `diagnostics:read` | `200` | `Diagnostics`; canonical status resource |
| `GET /device` | `diagnostics:read` | `200` | `Device` |
| `GET /diagnostics` | `diagnostics:read` | `200` | Compatibility alias of `/status` |
| `GET /network` | `diagnostics:read` | `200` | `NetworkStatus` |
| `GET /relays` | `relay:read` | `200` | `RelayList` |
| `PUT /relays/{id}` | `relay:command` | `202` | Canonical idempotent `RelayCommand` |
| `POST /relays/{id}/commands` | `relay:command` | `202` | Compatibility alias of relay `PUT` |
| `GET /commands/{correlationId}` | `relay:read` | `200` | `CommandResult` |
| `GET /network/wifi` | `configuration:read` | `200` | Redacted `WifiManagement` |
| `POST /network/wifi/scan` | `configuration:write` | `202` | Queue scan operation |
| `PUT, DELETE /network/wifi/profiles/{index}` | `configuration:write` | `202` | Queue profile mutation |
| `POST /network/wifi/profiles/{index}/move` | `configuration:write` | `202` | Queue profile reorder |
| `POST /network/wifi/profiles/{index}/connect` | `configuration:write` | `202` | Queue connection attempt |
| `PUT /network/wifi/recovery-ap` | `configuration:write` | `202` | Queue recovery AP update |
| `GET, PUT /protocols/modbus` | config read/write | `200/202` | `ModbusConfiguration` |
| `PUT /protocols/modbus/role` | `configuration:write` | `202` | Queue runtime role change |
| `GET, PUT /protocols/knx` | config read/write | `200/202` | `KnxConfiguration` |
| `GET, POST /users` | `users:manage` | `200/202` | List or create `User` |
| `PUT /users/{id}` | `users:manage` | `202` | Queue user update |
| `GET /operations/{operationId}` | `configuration:read` | `200` | `Operation` |
| `POST /reboot` | `configuration:write` | `202` | Canonical queued restart |
| `POST /maintenance/restart` | `configuration:write` | `202` | Compatibility alias of `/reboot` |
| `POST /factory-reset` | `configuration:write` | `403` | Always requires physical BOOT gesture |
| `POST /ota` | `configuration:write` | `501` | Always unavailable while capability is false |
| `GET /ws` | `relay:read` | `101` | Authenticated versioned live event stream |

`NetworkStatus.activeTransport` is `none`, `wifi`, or `ethernet`. Its lifecycle
state may additionally be `connecting-ethernet` or `online-ethernet` when the
selected board descriptor and injected Ethernet adapter both provide wired
networking. The Waveshare ESP32-S3 Relay 6CH always reports Ethernet capability
as false and never emits Ethernet lifecycle states.

## Common schemas

All integer counters are non-negative unless stated otherwise. Unknown JSON
properties must be ignored by clients.

```text
Error = { error: { code: string, message: string } }
Session = {
  user: { id: integer, username: string, role: "administrator"|"guest", enabled: boolean },
  expiresInMs: integer, csrfToken: string, permissions: string[]
}
RelayCommand = { action: "setOn"|"setOff"|"toggle", expectedSequence: integer }
CommandResult = {
  correlationId: decimal-string,
  result: "queued"|"applied"|"idempotent"|"rejected"|"unknown",
  channel?: integer, appliedState?: "on"|"off", sequence?: integer, reason?: string
}
Operation = {
  operationId: decimal-string,
  status: "pending"|"applied"|"conflict"|"invalid"|"unavailable"|"rejected"
}
```

`Relay` contains `id`, `physicalLabel`, `requestedState`, `appliedState`,
`verification`, `lastSource`, `transitionSequence`, `lastTransitionAgeMs`,
`fault`, `lockedOut`, and `enabled`. `RelayList` contains `bootId`,
`snapshotSequence`, and `relays`.

`Capabilities`, `Device`, `NetworkStatus`, `WifiManagement`,
`ModbusConfiguration`, `KnxConfiguration`, `User`, and `Diagnostics` use the
field-level schemas in `web/src/api/types.ts`. Those schemas are part of this
v1 contract and must be changed compatibly. Secret fields, passphrases,
password verifiers, signing keys, and private keys are never response fields.

## Mutation and concurrency semantics

Relay mutations require an `Idempotency-Key` header. Reusing the key with the
same session and body returns the retained result; reusing it for a different
request returns `422 configuration_error`. `expectedSequence` prevents a stale
relay write.

Configuration mutations include `expectedGeneration`. A stale generation
returns an operation with `conflict` status; a synchronous conflict returns
`422 configuration_error`. Accepted asynchronous mutations
return `202 Operation`; clients poll `/operations/{operationId}` until the
status is no longer `pending`. A missing or expired result returns `404`.

## Stable errors

Application services map their specific result enums to `domain::ErrorCode`.
Each protocol adapter then maps that code to its native representation. HTTP
uses the following complete, stable vocabulary:

| HTTP | Stable code | Domain error |
|---:|---|---|
| `400` | `invalid_argument` | `InvalidArgument` |
| `401` | `unauthorized` | `Unauthorized` |
| `403` | `forbidden` | `Forbidden` |
| `404` | `not_found` | `NotFound` |
| `429` | `busy` | `Busy` |
| `503` | `storage_error` | `StorageError` |
| `422` | `configuration_error` | `ConfigurationError` |
| `503` | `hardware_error` | `HardwareError` |
| `503` | `network_error` | `NetworkError` |
| `502` | `protocol_error` | `ProtocolError` |
| `501` | `unsupported` | `Unsupported` |
| `500` | `internal_error` | `InternalError` |

Error codes are stable machine identifiers. Message text is diagnostic and may
be clarified without a major API version change.

Modbus maps `NotFound` to illegal data address, `InvalidArgument` and
`ConfigurationError` to illegal data value, `Unsupported` to illegal function,
and all failures that cannot be represented safely to server device failure.
KNX group writes have no negative application acknowledgment; failures are
silently rejected and recorded in diagnostics, with `Busy` additionally
recorded as command-queue pressure.

## WebSocket versioning

The WebSocket handshake uses `/api/v1/ws`, the same session cookie, exact
origin checks, and `relay:read`. Payloads belong to API major version 1. New
event types or fields may be added compatibly; clients must ignore unknown
types and fields. A reconnect must begin with fresh REST snapshots because
events are bounded and sequence gaps are possible.