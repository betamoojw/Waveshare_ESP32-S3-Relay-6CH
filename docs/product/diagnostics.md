# Product Diagnostics

## Purpose

Diagnostics provide a bounded, non-secret view of device identity, lifecycle,
resources, storage, networking, protocols, relays, counters, and active faults.
`DiagnosticsService` owns the canonical snapshot; adapters only project it into
their protocol representation.

## Access Surfaces

| Surface | Authorization | Use |
|---|---|---|
| Serial `version` | Read-only | Compatibility labels and build profile |
| Serial `status` | Read-only | Lifecycle, uptime, relays, resources, and persistent counters |
| Serial `service diagnostics` | Physical service session | Factory/service health inspection |
| `GET /api/v1/diagnostics` | Authenticated `diagnostics:read` | Complete bounded JSON snapshot |
| `GET /api/v1/status` | Authenticated `diagnostics:read` | Alias of the diagnostic snapshot |

Service mode has no network entry path. Web diagnostics require HTTPS,
authentication, exact management identity checks, and normal authorization.

## Snapshot Contents

- product model, hardware revision, firmware version, and build ID;
- uptime, boot count, reset category, and lifecycle state;
- current, minimum, and largest free heap; PSRAM totals; CPU frequency/cores;
- network lifecycle, active transport, connectivity, recovery AP state, RSSI,
  and IP address;
- LittleFS availability, NVS availability/health, configuration validity, and
  configuration generation;
- Modbus and KNX availability plus bounded request/error counters;
- relay requested/applied state, lockout, fault, source, and transition sequence;
- active fault code, severity, occurrence count, and aggregate fault state;
- WebSocket client/capacity and request-queue pressure indicators.

## Persistent Counters

The persistent counters are:

```text
bootCount
watchdogCount
brownoutCount
configErrorCount
otaFailureCount
networkFailureCount
modbusErrorCount
knxErrorCount
storageErrorCount
```

Reset counters are updated during boot. Runtime counters are updated in memory
and checkpointed no more than once per minute while dirty, plus during a
controlled restart. Factory reset preserves these counters because they are
service evidence, not user configuration.

## Fault Interpretation

| Fault family | First checks |
|---|---|
| Relay output | Disconnect loads, inspect supply/GPIO mapping, do not retry through a protocol bypass |
| Configuration/settings | Compare generation and load result; validate NVS and the complete LittleFS bundle |
| Filesystem | Confirm mount and complete seven-file bundle; do not auto-format a field unit |
| Network | Inspect lifecycle, active transport, recovery AP state, RSSI, and profile validity |
| Modbus/KNX | Verify adapter availability, physical bus, addressing, timing, and error counters |
| Watchdog/resource | Inspect reset category, heap low-water marks, queue pressure, and repeated operations |
| Security policy | Verify production secure-boot and flash-encryption eFuse state using approved station tooling |

## Secret Exclusions

Diagnostics and logs must never contain passwords, password verifiers, Wi-Fi
passphrases, JWTs, cookies, CSRF values, authorization headers, private/signing
keys, certificates containing private material, security tokens, raw request
bodies, or raw configuration documents. Network diagnostics deliberately omit
SSID and credential values.

For protocol field definitions, see [Web API v1](../protocols/web-api.md),
[Modbus RTU](../protocols/modbus.md), and [KNX/IP](../protocols/knx-ip.md).
