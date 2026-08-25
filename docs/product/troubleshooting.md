# Troubleshooting

## Safety First

Disconnect or isolate mains/load wiring before firmware diagnosis. Do not use a
protocol command, direct GPIO test, or debugger write to bypass relay policy or
a safety lockout. Preserve diagnostics before resetting or rewriting storage.

Start with read-only information:

```text
version
status
service status
```

Use `service diagnostics` only after entering physical service mode. For Web
diagnosis, use authenticated `GET /api/v1/diagnostics` or `/api/v1/status`.

## Symptom Guide

| Symptom | Checks | Corrective path |
|---|---|---|
| Device does not become operational | Lifecycle, reset reason, active faults, board descriptor, secure-boot/encryption state | Follow [Boot flow](../architecture/boot-flow.md); do not bypass a critical fault |
| Device is degraded | Configuration validity/generation, NVS/LittleFS health, adapter availability | Correct the specific dependency; degraded fallback is intentionally observable |
| Relays remain off | Lockout/fault, enabled policy, restore policy, command result, physical supply | Resolve policy/hardware fault; never write GPIO directly |
| Repeated watchdog resets | Watchdog counter, heap minimum/largest block, queue pressure, network/protocol errors | Reproduce in engineering profile and isolate the blocking/hot path |
| LittleFS fault | Mount state and completeness of all seven files plus backup | Restore a complete validated bundle; do not auto-format field storage |
| Configuration rejected | Schema version, required sections, ranges, identity fields, generation conflict | Validate/stage the complete configuration; do not patch active state partially |
| Wi-Fi unavailable | Network state, profile enabled/order, RSSI, recovery AP, DHCP/static fields | Use authenticated recovery AP or physically authorized serial configuration |
| Web login unavailable | HTTPS certificate trust, host/origin, security provisioned state, administrator record | Re-provision only through physical serial authorization; never create a remote bootstrap route |
| Modbus requests fail | Role, unit ID, baud, 8 data bits, parity, stop bits, RS-485 wiring/termination, counters | Match every bus participant and use the frozen register-map contract |
| KNX unavailable | Network online state, individual/group addresses, startup delay, bus state, counters | Correct validated KNX configuration and network/bus prerequisites |
| Factory reset appears ineffective | Verify reset gesture, restart, configuration generation, preserved identity | Compare against the remove/preserve contract; identity and LittleFS should remain |

## Build Failures

Development and engineering builds require their explicit PlatformIO environment:

```powershell
pio run -e development
pio run -e engineering
```

A production build is expected to fail when immutable firmware version,
external signing key, or secure-boot/flash-encryption approvals are absent.
Do not weaken `extra_script.py` or production build flags to make a local build
pass. Use the protected release workflow and approved inputs described in
[Security architecture](../architecture/security.md).

Frontend validation is independent of firmware compilation:

```powershell
Set-Location web
npm run lint
npm test
npm run build
```

Native tests require a host C++17 compiler in addition to PlatformIO.

## Recovery Boundaries

- Normal firmware upload does not replace LittleFS.
- Factory reset does not erase firmware, manufacturing identity, security
  identity, diagnostic counters, or LittleFS deployment data.
- Runtime OTA and firmware recovery are currently unavailable.
- Production firmware repair requires an approved signed package and audited
  external station.

See [Recovery](recovery.md) before destructive action.

## Escalation Package

Provide serial number, hardware/firmware/configuration/API/protocol/filesystem
versions, lifecycle, reset reason, configuration generation, active faults,
persistent counters, build profile, release digest, and a minimal reproduction.
Redact Wi-Fi names/passphrases, passwords, cookies, JWTs, tokens, keys,
certificates with private material, authorization headers, and raw configuration.
