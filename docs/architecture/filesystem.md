# Filesystem Architecture

## Purpose

LittleFS provides recoverable deployment defaults and a reserved location for
future web assets. NVS remains the authoritative transactional store for mutable
runtime configuration and relay state.

This design adapts the useful filesystem mechanisms in WLED without importing
WLED's global filesystem ownership or generic file-serving surface:

- mount once during boot and report mount failure;
- validate JSON before use;
- keep a known-good backup and restore only a validated backup;
- use a temporary file for replacement;
- keep secrets and configuration outside static web routes;
- allow compiled resources to remain a boot-safe fallback.

The reference surfaces reviewed were WLED's filesystem mount in
[`wled.cpp`](https://github.com/wled/WLED/blob/main/wled00/wled.cpp), JSON backup
and validation helpers in
[`file.cpp`](https://github.com/wled/WLED/blob/main/wled00/file.cpp), configuration
persistence in [`cfg.cpp`](https://github.com/wled/WLED/blob/main/wled00/cfg.cpp),
and generated compressed web assets in
[`tools/cdata.js`](https://github.com/wled/WLED/blob/main/tools/cdata.js).

## Ownership and Precedence

The application resolves configuration in this order:

1. newest valid NVS A/B generation;
2. valid LittleFS `/config/*.json` bundle;
3. valid LittleFS `/config/.backup/*.json` bundle;
4. embedded `config/default_configuration.json`;
5. safe domain defaults with relay restoration disabled.

`ConfigurationService` remains the only owner of active and staged
configuration. The LittleFS adapter implements `ConfigurationSource`; it does
not apply settings or access relay services.

## Files

| Path | Configuration ownership |
|---|---|
| `/config/system.json` | Schema version, device identity, relay policies, indicators |
| `/config/network.json` | Network enablement, host name, recovery access point |
| `/config/wifi.json` | Fixed-capacity Wi-Fi profiles |
| `/config/ethernet.json` | Ethernet enablement; must be disabled until BSP/domain support exists |
| `/config/knx.json` | KNX device and channel bindings |
| `/config/modbus.json` | Modbus RTU serial and unit settings |
| `/config/ui.json` | Web UI enablement and security provisioning state |
| `/config/.backup/` | Last completely validated seven-file bundle |
| `/www/` | Future web adapter assets; not currently served |

Each section file is limited to 8192 bytes. The adapter parses each file as a
JSON object, assembles the canonical configuration document, and delegates final
schema and domain validation to `JsonConfigurationSource`. Missing, malformed,
empty, oversized, truncated, or domain-invalid sections reject the entire
bundle failure-atomically. Unknown Ethernet enablement is rejected rather than
silently ignored.

## Boot and Recovery

LittleFS is mounted once with auto-format disabled. A mount failure records
`FileSystemFailure`, uses the embedded fallback, and keeps the lifecycle
degraded so the storage problem remains observable.

When the complete primary bundle is valid, it refreshes
`/config/.backup/` using per-file temporary replacements. When any primary
section is missing or invalid and the complete backup bundle is valid, the
backup becomes the active default and is restored to `/config/`. If neither
bundle is valid, firmware uses the embedded fallback. A normal firmware flash
does not overwrite LittleFS, and the adapter never manufactures a partial split
bundle from the embedded monolithic fallback.

Factory reset transactionally replaces mutable user configuration in NVS with
validated safe defaults while preserving manufacturing identity and diagnostic
counters. It does not erase LittleFS deployment defaults. Because the reset
configuration remains the newest valid NVS generation, LittleFS does not
override it on the next boot. The complete remove/preserve contract is defined
in [Factory reset](../manufacturing/factory-reset.md). A separate authenticated maintenance
operation is required before filesystem formatting can be introduced.

## Web and Security Boundary

The web adapter MUST NOT expose a generic file editor, directory listing, or
arbitrary LittleFS path. Static serving, when implemented, MUST use an explicit
allowlist rooted at `/www/`, reject traversal and configuration paths, prefer
pre-compressed assets where available, and fall back to compiled essential
pages when the filesystem is unavailable.

Tokens, private keys, and administrative credentials MUST NOT be placed in
`/config/` or `/www/`; they belong in protected NVS namespaces. Wi-Fi
passphrases are part of the data-driven Wi-Fi profile and are written to
`wifi.json` only by the explicit maintenance `store-config` operation. LittleFS
is not encrypted, so physical flash access can recover them. The web adapter
MUST never serve configuration paths, and support exports or downloads must
redact secrets before serialization.

CLI configuration setters persist transactionally to NVS and do not
automatically increase LittleFS wear. `store-config` explicitly snapshots the
active configuration to the split bundle. `load-config` reads only the primary
bundle, validates it completely, then stages and commits it through
`ConfigurationService`; boot fallback selection is not used for this explicit
operator command.

## Deployment

PlatformIO selects `littlefs` through `board_build.filesystem` and builds the
image from `data/`. Build and upload the data-driven bundle with:

```text
pio run -e development -t buildfs
pio run -e development -t uploadfs
```

An ordinary firmware flash remains bootable through the embedded monolithic
fallback but does not deploy or replace the split bundle. Treat `uploadfs` as an
explicit provisioning operation and validate device identity before energizing
loads. Uploading a filesystem image can replace existing LittleFS data; preserve
field configuration before provisioning an installed device.