# Relay Safety Policy

## 1. Purpose and Status

This document is the normative relay startup and disturbance policy for the
Switch Actuator. The keywords **MUST**, **MUST NOT**, **SHOULD**, and **MAY** are
requirements. `src/domain/relay_policy.*` is the executable decision table.

Electrical safety has priority over state restoration. Missing, invalid, or
ambiguous information always resolves to OFF.

## 2. Boot States

| Boot state | Required behavior |
|---|---|
| `OFF` | Command the relay OFF regardless of configured or persisted state. |
| `ON` | Command an enabled relay ON. No global reset or failure event currently selects this state. |
| `RESTORE` | Apply the channel `restorePolicy`: `allOff` becomes OFF, `configuredDefault` uses the configured state, and `lastKnown` uses valid persisted state or falls back OFF. |
| `LAST_STATE` | Preserve the authoritative applied state. If restoration is required and no valid state is available, fall back OFF. |
| `SAFE_STATE` | Command OFF. The product safe state is fixed and is not remotely configurable. |
| `CONFIGURED_STATE` | Use `configuredDefault`, independent of `restorePolicy`. |

A disabled channel always resolves OFF, including for `ON` and
`CONFIGURED_STATE`. Invalid enum values, invalid persisted states, correlation
overflow, or output failures reject the restore plan and do not permit an ON
fallback.

## 3. Event Decision Table

| Event | Boot state | Product behavior |
|---|---|---|
| Power-on | `RESTORE` | Outputs first initialize OFF, then each enabled channel applies its configured restore policy. |
| Brownout | `SAFE_STATE` | All channels remain or return OFF; previous state is ignored. |
| Watchdog reset | `SAFE_STATE` | All channels remain or return OFF; previous state is ignored and a diagnostic fault is recorded. |
| Software reboot | `RESTORE` | After controlled response draining, startup applies each channel restore policy. |
| OTA reboot | `CONFIGURED_STATE` | After a verified OTA image is selected, startup uses each channel configured default and never restores last-known state. |
| Factory reset | `OFF` | Safety lockout forces every channel OFF before security or configuration is erased. Restart defaults remain all OFF. |
| Configuration update | `LAST_STATE` | A successful commit does not actuate relays. If the update requires restart, the subsequent controlled software reboot applies `RESTORE`. |
| Network failure | `LAST_STATE` | Connectivity transitions never issue relay commands. Local control and the current applied state remain available. |

Panic, repeated-boot, and unknown reset causes are classified as watchdog-safe
and therefore use `SAFE_STATE`.

## 4. Startup Sequence

1. The BSP writes each relay GPIO inactive before selecting output mode, selects
   output mode, and writes inactive again.
2. `RelayCommandService` initializes every authoritative snapshot and commands
   every channel OFF.
3. Configuration is loaded and fully validated. Invalid configuration cannot
   request an ON state.
4. The reset cause is translated to a `RelaySafetyEvent` and then to one
   `RelayBootState` through the domain decision table.
5. A complete six-channel restore batch is validated before execution. The
   normal command service applies it and owns the resulting authoritative state.

No protocol adapter, network callback, configuration parser, or OTA callback may
write relay GPIO directly.

## 5. Last-State Availability

The current firmware has no durable relay-state persistence port. Consequently,
`lastKnown` safely resolves OFF after every reboot. `LAST_STATE` during a live
configuration update or network failure means no command is issued, so the
in-memory authoritative applied state is retained.

Durable last-state restoration MUST NOT be enabled until writes are bounded,
wear-aware, CRC-protected, generation-ordered, and demonstrably completed before
the reset being recovered from. Invalid or absent records MUST resolve OFF.

## 6. OTA Constraint

OTA is currently unavailable. Before OTA can be advertised, its implementation
MUST persist or retain an authenticated `OtaReboot` intent across restart so an
ESP software reset cannot be mistaken for an ordinary software reboot. Without
that marker, the update MUST fail closed and leave the existing image active.

## 7. Hardware Boundary

Firmware cannot control relay pins while the ESP32-S3 is unpowered, held in
reset, or below its guaranteed operating voltage. The production hardware MUST
therefore bias every relay driver to the de-energized state until GPIO control is
valid. Brownout testing MUST verify that relay drivers do not pulse or chatter
before firmware regains control. Firmware behavior alone is not sufficient
evidence for that electrical requirement.
