# CLI Adapter

This adapter exposes a serial console with the prompt `switch-actuator>`. It routes every relay action through `SwitchingPolicyService`; no CLI command writes relay GPIO directly.

Commands emit machine-readable `ok=true` or `ok=false` responses. `version`, `status`, and all `get-*` commands are available without maintenance authorization.

## Local Maintenance Authorization

Mutating commands require all of the following:

- firmware built with `ENABLE_MUTATING_CLI_COMMANDS`;
- active local maintenance authorization, granted by the BOOT-button commissioning gesture;
- a lifecycle state that accepts ordinary commands.

The adapter responds with `error=not-authorized` or `error=maintenance-authorization-required` when this gate is not satisfied. Maintenance authorization is cleared at CLI initialization and during factory reset.

## Initial Web Provisioning

Web security is provisioned only over the local serial console after physical
BOOT-button authorization. Hold BOOT for at least 3 seconds and release before
the 10-second factory-reset threshold, then run:

```text
provision-web [username] [password]
```

The password must contain 12 through 128 non-whitespace characters. CLI command
history is disabled, the parsed command buffer is cleared after execution, and
the password is never echoed. The device generates its P-256 private key,
self-signed SHA-256 certificate, JWT signing key, salt, and password verifier
locally from the ESP32 hardware RNG. Private material is persisted only in the
protected `web_security` NVS namespace.

Provisioning writes protected security state first and enables
`web.securityProvisioned` only after the validated configuration commit
succeeds. A failed configuration commit erases the new security record, so the
web server remains fail-closed. Success requests a controlled restart and
revokes maintenance authorization. The generated certificate contains the
configured `<hostname>.local` DNS SAN; an administrator must explicitly trust
that self-signed certificate on management clients.

## Configuration Files

```text
load-config
store-config
```

Both commands are maintenance-only. `load-config` reads only the primary
seven-file `/config/` bundle, validates the complete assembled configuration,
and commits it through `ConfigurationService` and the NVS A/B store. It does not
silently load the filesystem backup or embedded fallback. The response reports
whether a controlled restart is required.

`store-config` serializes the current validated active configuration into
`/config/.staging/`, reloads and validates that bundle, preserves the current
valid primary as `/config/.backup/`, and then promotes the staged files. Normal
CLI setters continue to commit NVS only; run `store-config` explicitly when the
new active values must become filesystem deployment defaults.

The Wi-Fi file contains configured SSIDs and passphrases in plaintext because
LittleFS does not provide encryption. The web adapter must never expose
`/config/`, and physical flash access remains able to recover these values.

## Device And Relay Commands

```text
version
status
get-relay [all|0..5]
set-relay [all|0..5] [on|off]
toggle-relay [0..5]
reboot
```

`status` returns lifecycle, uptime, maintenance authorization, and applied relay states. Relay mutations are queued through the same application command path used by the other command sources.

## Indicators And Button

```text
get-indicator
set-rgb [red_0..255] [green_0..255] [blue_0..255] [brightness_0..255]
buzzer [0..7]
get-button
```

`set-rgb` and `buzzer` are maintenance-only commands. The configured brightness and buzzer-duty limits remain enforced.

## Manufacturing Test Interface

Production fixtures use `mfg-test snapshot`, `button`, `relay`, `rgb`, `buzzer`, and `safe`. Snapshot and button operations are read-only. Mutating operations require local maintenance authorization; relay requests use the production command path and `safe` queues all channels off and clears indicator overrides. See `design/manufacturing-test-interface.md` for the normative sequence and external fixture requirements.

## Modbus RTU

All connected Modbus RTU devices must use the same serial framing: supported baud rate, 8 data bits, matching parity, and matching stop bits. Each device must be assigned a unique slave ID from `1` through `247`; the firmware validates the local ID range, while uniqueness across multiple physical devices must be maintained by the installer or network commissioning process.

```text
get-modbus-role
set-modbus-role [server|client]
get-modbus-config
set-modbus-config [slave_id_1..247] [baud] [none|even|odd] [stop_bits_1|2]
modbus-read-holding [unit_1..247] [address] [count_1..20]
modbus-write-register [unit_1..247] [address] [value]
```

`get-modbus-config` reports the active `slave_id`, baud rate, data bits, parity, and stop bits. `set-modbus-config` always applies `data_bits=8`; the data-bit width is deliberately not configurable because the connected-device requirement is 8 data bits. Changing the role, serial configuration, or issuing Modbus client operations requires maintenance authorization. Serial configuration changes are validated and persisted atomically, and normally return `restart_required=true`.

All connected Modbus RTU devices must use the same baud rate, 8 data bits, parity, and stop bits. Every physical device must have a unique slave ID in the range `1..247`; this firmware validates the local ID but cannot discover duplicate IDs on other devices without a bus-wide commissioning protocol.

## Wi-Fi Provisioning

```text
set-wifi [profile_0..2] [ssid] [passphrase]
```

This maintenance-only command stores one of the three ordered Wi-Fi profiles through the validated, dual-slot configuration service, then immediately asks the network manager to reconfigure without rebooting the relay application. The CLI response includes the profile index and reconfiguration state but never echoes the SSID or passphrase. SSIDs must fit the firmware's 32-character limit; passphrases must fit its 63-character limit. Whitespace is not supported in either argument by the tokenized CLI.

## KNX/IP Configuration

Reading KNX configuration is always available:

```text
get-knx
get-knx general
get-knx channel 0
```

Mutating commands require a build with mutating CLI commands enabled, active local maintenance authorization, and an operational lifecycle. Every accepted change follows `copy active -> validate -> stage -> persist -> commit` and reports whether a controlled restart is required.

General parameters and device-wide communication objects:

```text
set-knx enabled [true|false]
set-knx individual-address [area.line.device]
set-knx startup-delay-ms [0..60000]
set-knx telegram-interval-ms [20..1000]
set-knx cyclic-status-ms [0|10000..86400000]
set-knx heartbeat-interval-ms [0|10000..86400000]
set-knx read-switch [true|false]
set-knx heartbeat-ga [main/middle/sub|none]
set-knx central-switch-ga [main/middle/sub|none]
set-knx central-off-ga [main/middle/sub|none]
set-knx device-fault-ga [main/middle/sub|none]
```

Per-channel parameters and communication objects:

```text
set-knx-channel [0..5] switch-ga [main/middle/sub|none]
set-knx-channel [0..5] status-ga [main/middle/sub|none]
set-knx-channel [0..5] fault-ga [main/middle/sub|none]
set-knx-channel [0..5] command-inverted [true|false]
set-knx-channel [0..5] status-inverted [true|false]
set-knx-channel [0..5] startup-status [true|false]
set-knx-channel [0..5] central-switch [true|false]
set-knx-channel [0..5] central-off [true|false]
```

Boolean setters also accept `1/0`, `yes/no`, and `enabled/disabled`. Group address `0/0/0` is rejected because schema v3 uses packed zero for the explicit `none` state. Configure a heartbeat group address before enabling its interval, and disable the interval before clearing its group address.

The configuration service rejects duplicate writable addresses, collisions between writable and outbound objects, invalid timing ranges, and enabling KNX without a valid individual address. A successful KNX change normally returns `restart_required=true`; use `reboot` to request the controlled restart after completing all changes.