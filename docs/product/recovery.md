# Product Recovery

## Recovery Model

Recovery is layered. Use the least destructive operation that restores a valid,
observable state. Configuration fallback, recovery Wi-Fi, user reset, factory
reset, and firmware recovery are distinct mechanisms.

## Decision Table

| Condition | Supported recovery | Preserved state |
|---|---|---|
| Primary NVS configuration invalid | Valid LittleFS, backup, embedded, then safe-default fallback | Identity when present in the selected valid source |
| Primary LittleFS bundle invalid | Complete validated backup bundle | NVS and factory security identity |
| LittleFS unavailable | Embedded configuration or safe defaults; lifecycle remains degraded | NVS and installed firmware |
| Infrastructure Wi-Fi unavailable | Configured recovery access point | Configuration and identity |
| Web administrator unavailable | Physical service entry and identity-preserving user reset | Manufacturing and factory security identity |
| User configuration must be removed | Physical factory reset or authorized service erase | Manufacturing identity, security identity, counters, firmware, LittleFS |
| Firmware image damaged/incompatible | External approved manufacturing/service flashing | Depends on approved station procedure |

## Configuration Recovery

LittleFS is mounted with automatic formatting disabled. Firmware accepts only a
complete, bounded, schema-valid seven-file bundle. A valid primary bundle
refreshes `/config/.backup/`; an invalid primary may be restored only from a
complete valid backup. If both fail, firmware proceeds to the embedded fallback
or safe defaults and reports the underlying storage/configuration fault.

Do not delete NVS, format LittleFS, or merge individual files from different
bundles as a first response. Capture `version`, `status`, generation, load/save
results, and relevant persistent counters before mutation.

## Network Recovery

When enabled and infrastructure connectivity cannot be established, the
network manager may expose the configured recovery access point. Recovery AP
access does not grant service mode, factory operations, secret access, or a
relay-policy bypass. Apply a corrected Wi-Fi profile through authenticated Web
configuration or physically authorized serial maintenance.

## User And Factory Reset

Hold BOOT for at least ten seconds and release to request physical factory
reset. The reset removes users, sessions, Wi-Fi/network preferences, protocol
configuration, relay preferences, scenes, timers, and pending user operations.
It preserves serial/UUID, product and hardware identity, manufacturing data,
MAC/eFuse identity, TLS/signing identity, diagnostic counters, firmware, OTA
metadata, and LittleFS deployment data.

The serial `service erase-user-configuration` operation follows the same
identity-preserving contract and requires an active physical service session.
See [Factory reset](../manufacturing/factory-reset.md).

## Firmware Recovery

Runtime firmware recovery is currently unavailable. Web capabilities advertise
firmware update unavailable, the OTA route rejects the request, and
`service firmware-recovery` returns `operation-unavailable`. This is deliberate:
the product does not yet contain a separately verified recovery image and safe
recovery boot selector.

Use only an approved signed release package and audited external station for
firmware repair. Never add unsigned network or serial flashing to running
production firmware. See [Product update](update.md) and
[Release process](../release/release-process.md).

## Evidence Before Escalation

Record the device serial, all compatibility labels, reset category, lifecycle,
configuration generation, active faults, persistent counters, installed release
package digest, and actions already attempted. Never attach credentials, tokens,
private keys, or raw configuration containing passphrases.
