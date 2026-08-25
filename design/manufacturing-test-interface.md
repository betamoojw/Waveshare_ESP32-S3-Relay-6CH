# Manufacturing Test Interface

## 1. Purpose and Status

This document defines the normative production-line test interface for the Switch Actuator. It complements the software architecture and hardware-in-the-loop release procedure. The keywords **MUST**, **MUST NOT**, **SHOULD**, and **MAY** are requirements.

The interface is a machine-readable serial CLI contract. It is available before network or web provisioning and does not require production fixtures to trust a device certificate.

## 2. Safety and Authorization

1. Manufacturing commands MUST NOT access GPIO directly. Relay requests MUST pass through `SwitchingPolicyService`, the bounded command queue, and `RelayCommandService`.
2. Mutating tests MUST require an unlocked deployment profile, active BOOT-button maintenance authorization, and a lifecycle state that accepts ordinary commands.
3. Read-only snapshot and button observations MAY run without authorization over the local serial connection.
4. Indicator tests MUST preserve configured brightness and buzzer-duty limits.
5. The fixture MUST issue `mfg-test safe` before connecting or removing loads and before declaring a completed or aborted test.
6. `safe` MUST request all relay channels off and clear temporary indicator overrides. It MUST NOT erase configuration, credentials, counters, or device identity.
7. Manufacturing mode MUST NOT weaken web authentication, protocol authorization, relay arbitration, safety lockouts, or watchdog behavior.

BOOT-button maintenance authorization is implemented as the time-bounded service session defined in [Service mode](../docs/manufacturing/service-mode.md). The same session protects identity, diagnostics, manufacturing-data, and field-service reset operations; no network transport can create it.

## 3. Transport Contract

- Transport: USB serial CLI at the configured monitor rate, currently 115200 baud.
- Command: `mfg-test [operation] [arguments]`.
- Responses MUST be one machine-readable line beginning with `ok=true` or `ok=false`.
- Invalid syntax, range errors, authorization failures, unavailable hardware, and queue pressure MUST produce stable error identifiers.
- Commands MUST be bounded and non-blocking. The fixture observes queued relay completion through subsequent snapshots.

## 4. Commands

```text
mfg-test snapshot
mfg-test button
mfg-test relay [0..5] [on|off]
mfg-test rgb [red_0..255] [green_0..255] [blue_0..255] [brightness_0..255]
mfg-test buzzer [0..7]
mfg-test safe
```

`snapshot` reports interface version, deployment profile, configuration lock, product ID, board model, hardware revision, device serial, UUID, manufacturing date and batch, configuration generation, authorization, subsystem initialization, network and Modbus state, button state, and all applied relay states. It MUST NOT expose secrets or mutable GPIO numbers.

`button` reports initialized and pressed state. A fixture SHOULD sample released, pressed, and released. GPIO0 boot/download behavior remains part of the hardware release gate.

`relay` queues an explicit idempotent set operation. Queue acceptance does not prove contact continuity; the fixture MUST verify isolated contacts and then command the channel off.

`rgb` and `buzzer` apply bounded maintenance indications. Fixture or operator observation determines pass/fail because firmware has no optical or acoustic feedback sensor.

`safe` queues an atomic six-channel off request and clears RGB/buzzer maintenance overrides. The fixture MUST poll `snapshot` until every applied state is off before disconnecting the unit.

## 5. Recommended Fixture Sequence

1. Boot and wait for `ready=true adapter=cli version=1`.
2. Issue `mfg-test snapshot` and validate model, revision, and initialized flags.
3. Obtain physical maintenance authorization with the BOOT commissioning gesture.
4. Verify button released/pressed/released observations.
5. Exercise RGB primary colors and bounded buzzer tones.
6. For each relay, issue explicit on, verify isolated contact closure, issue explicit off, and verify release.
7. Exercise Modbus using the existing client/server CLI commands and an external RS-485 fixture.
8. Issue `mfg-test safe`, poll until all relays report off, and archive results against the device serial.

## 6. Acceptance Criteria

- Unauthorized mutating commands fail without changing outputs.
- Invalid operations, channels, and values are rejected with stable errors.
- Relay tests use production safety and state-convergence paths.
- Safe cleanup is idempotent and leaves every relay requested off.
- Repeated malformed commands cannot starve watchdog, network, Modbus, KNX, or relay processing.

Electrical contacts, RS-485 loopback, RF performance, current consumption, and programming/download mode require external fixture instrumentation and cannot be self-certified by firmware.