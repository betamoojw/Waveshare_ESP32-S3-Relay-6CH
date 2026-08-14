# CLI Adapter

CLI command parsing and application-service integration belong in this directory.

## KNX/IP configuration

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

Boolean setters also accept `1/0`, `yes/no`, and `enabled/disabled`. Group address `0/0/0` is rejected because schema v2 uses packed zero for the explicit `none` state. Configure a heartbeat group address before enabling its interval, and disable the interval before clearing its group address.

The configuration service rejects duplicate writable addresses, collisions between writable and outbound objects, invalid timing ranges, and enabling KNX without a valid individual address. A successful KNX change normally returns `restart_required=true`; use `reboot` to request the controlled restart after completing all changes.