# CLI Adapter

This adapter exposes a serial console with the prompt `switch-actuator>`. It routes every relay action through `SwitchingPolicyService`; no CLI command writes relay GPIO directly.

Commands emit machine-readable `ok=true` or `ok=false` responses. `version`, `status`, and all `get-*` commands are available without maintenance authorization.

## Local Maintenance Authorization

Mutating commands require all of the following:

- firmware built with `ENABLE_MUTATING_CLI_COMMANDS`;
- active local maintenance authorization, granted by the BOOT-button commissioning gesture;
- a lifecycle state that accepts ordinary commands.

The adapter responds with `error=not-authorized` or `error=maintenance-authorization-required` when this gate is not satisfied. Maintenance authorization is cleared at CLI initialization and during factory reset.

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