# KNX/IP Application Model v1.0

Status: Frozen baseline product interface  
Product class: Six-channel KNX/IP switching actuator  
Medium: KNXnet/IP routing over UDP multicast `224.0.23.12:3671`

This document is the normative product contract for the KNX/IP application
implemented by the firmware. The typed object catalog is defined in
`src/adapters/knx/knx_application_model.h`.

The current product uses configuration files and the authenticated management
interfaces for commissioning. It is not yet an ETS-downloadable or
KNX-certified product. The ETS strategy below defines the work required before
a `.knxprod` package may be distributed.

## Compatibility Policy

The application model version is `1.0`. Object numbers, DPTs, command meanings,
and channel block allocation are stable within major version 1. Optional
objects may be enabled without renumbering existing objects. Reserved object
numbers must not be reused for an incompatible purpose.

A firmware release must not expose an object unless its decoding, policy,
persistence where applicable, status behavior, diagnostics, and tests are
complete. The aspirational commercial feature set in the design requirements
does not form part of this product interface.

## Application Model

```text
KNX/IP Application Model v1.0
        |
        +-- General parameters
        +-- Per-channel parameters (A..F)
        +-- Device communication objects
        +-- Channel communication objects
        +-- Group-address bindings
        +-- DPT definitions
        +-- Startup and recovery behavior
        +-- Applied-state status behavior
        +-- Central switch/off objects
        +-- Scene object reservations
        +-- Diagnostics
```

## Parameters

### General Parameters

| Parameter | Type and range | Default | Persistence | Effect |
|---|---|---|---|---|
| KNX enabled | Boolean | Disabled | Configuration | Starts or disables the KNX/IP adapter |
| Individual address | KNX individual address | Unprogrammed (`0`) | Configuration | Required and nonzero when KNX is enabled |
| Startup transmit delay | `0..60000 ms` | `3000 ms` | Configuration | Delays spontaneous status, fault, and heartbeat publication; never delays relay safety initialization |
| Minimum telegram interval | `20..1000 ms` | `100 ms` | Configuration | Spaces spontaneous outbound telegrams; at most one is emitted per adapter poll |
| Cyclic status interval | Disabled (`0`) or `10000..86400000 ms` | Disabled | Configuration | Republishes applied relay status by channel |
| Heartbeat interval | Disabled (`0`) or `10000..86400000 ms` | Disabled | Configuration | Publishes DPT 1.002 value `1` to the heartbeat binding |
| Read switch object | Boolean | Disabled | Configuration | Adds the read flag to channel switch objects and answers reads from applied state |
| Heartbeat group address | One primary binding or unassigned | Unassigned | Configuration | Enables object 0 when assigned and interval is nonzero |
| Central switch group address | One primary binding or unassigned | Unassigned | Configuration | Enables object 1 |
| Central off group address | One primary binding or unassigned | Unassigned | Configuration | Enables object 2 |
| Device fault group address | One primary binding or unassigned | Unassigned | Configuration | Enables object 4 |

Transport mode is fixed to KNXnet/IP routing in application model v1.0.
Tunnelling, KNX Secure, remote programming mode, and ETS download are not
exposed.

### Per-Channel Parameters

The following parameters exist independently for channels A through F.

| Parameter | Type | Default | Effect |
|---|---|---|---|
| Switch group address | One primary binding or unassigned | Unassigned | Enables the channel switch object |
| Status group address | One primary binding or unassigned | Unassigned | Enables applied-state publication |
| Fault group address | One primary binding or unassigned | Unassigned | Enables channel fault publication |
| Command polarity inverted | Boolean | No | DPT `0` requests on and `1` requests off when enabled |
| Status polarity inverted | Boolean | No | Inverts only the encoded status value |
| Send status after startup | Boolean | Yes | Queues initial applied-state publication after startup delay |
| Participate in central switch | Boolean | Yes | Includes the channel in object 1 operations |
| Participate in central off | Boolean | Yes | Includes the channel in object 2 operations |

Relay enablement and restore behavior are protocol-neutral relay parameters.
A disabled or safety-locked channel remains subject to the common switching
policy regardless of its KNX bindings.

## Address Representation

Individual addresses use the packed representation:

```text
(area << 12) | (line << 8) | device
```

Area and line are each `0..15`; device is `0..255`. Packed value `0` is not
accepted while KNX is enabled.

Three-level group addresses use:

```text
(main << 11) | (middle << 8) | sub
```

Main is `0..31`, middle is `0..7`, and sub is `0..255`.

The v1 configuration schema uses packed value `0` as the unassigned sentinel.
Consequently, valid KNX group address `0/0/0` cannot be assigned in this
version. Each object supports one primary group address. Command bindings must
be unique, and output bindings must not collide with a command binding. These
are explicit v1 product limitations, not KNX protocol limitations.

## DPT Definitions

| DPT | Width | Values | Use in v1.0 |
|---|---:|---|---|
| DPT 1.001 Switch | 1 bit | `0`=off, `1`=on | Channel switch, applied status, central switch |
| DPT 1.002 Boolean | 1 bit | `0`=false, `1`=true | Device-in-operation heartbeat |
| DPT 1.003 Enable | 1 bit | `0`=disable/no action, `1`=enable/execute | Central off |
| DPT 1.005 Alarm | 1 bit | `0`=no alarm, `1`=alarm | Channel and device fault |
| DPT 17.001 Scene number | 1 byte | Wire values `0..63` represent scenes `1..64` | Reserved; not exposed in v1.0 |
| DPT 18.001 Scene control | 1 byte | Recall/learn plus scene `1..64` | Reserved; not exposed in v1.0 |

Only one-bit payloads are accepted by exposed v1 objects. Command polarity is
applied after DPT decoding; BSP relay electrical polarity is unrelated.

## Communication Objects

Flags use `C` communication, `R` read, `W` write, `T` transmit, and `U` update.
Object numbers are stable ETS/product-data identifiers. The current runtime
adapter binds configured group addresses directly because ETS download is not
yet implemented.

### Device Objects

| No. | Name | DPT | Flags | Exposure | Behavior |
|---:|---|---|---|---|---|
| `0` | Device in operation | 1.002 | C,T | Conditional | Sends `1` at the configured heartbeat interval after startup delay |
| `1` | Central switch | 1.001 | C,W | Conditional | `0` requests off and `1` requests on for all participating channels |
| `2` | Central off | 1.003 | C,W | Conditional | `1` requests off for all participating channels; `0` has no effect |
| `3` | Aggregate status | - | - | Reserved | Not implemented or exposed in v1.0 |
| `4` | Device fault | 1.005 | C,T | Conditional | Sends `1` while any reportable device fault is active |
| `5..15` | Reserved | - | - | Hidden | Reserved for compatible model evolution |

### Channel Object Numbering

Each channel reserves 16 consecutive object numbers:

```text
B = 16 + channelIndex * 16
```

| Channel | Index | Object range |
|---|---:|---:|
| A | `0` | `16..31` |
| B | `1` | `32..47` |
| C | `2` | `48..63` |
| D | `3` | `64..79` |
| E | `4` | `80..95` |
| F | `5` | `96..111` |

### Per-Channel Objects

| Offset | Name | DPT | Flags | Exposure | Behavior |
|---:|---|---|---|---|---|
| `B+0` | Channel X switch | 1.001 | C,W; optional R | Conditional | Writes enqueue SetOff/SetOn through `SwitchingPolicyService`; optional read returns applied state using command polarity |
| `B+1` | Channel X applied state | 1.001 | C,T | Conditional | Publishes only authoritative applied state, with optional status inversion |
| `B+4` | Channel X scene recall | 17.001 | - | Reserved, hidden | Object number and DPT reserved; no callback or group-address parameter exists |
| `B+5` | Channel X scene control | 18.001 | - | Reserved, hidden | Object number and DPT reserved; learning and persistence are not connected to KNX |
| `B+9` | Channel X fault | 1.005 | C,T | Conditional | Sends `1` for output fault or safety lockout |
| Other offsets | Reserved | - | - | Hidden | Must not be emitted in v1.0 product data |

A conditional object is present only when its corresponding group-address or
feature parameter is configured. Scene services exist at the protocol-neutral
application layer, but scenes are not part of KNX application model v1.0.

## Startup And Recovery Behavior

1. Relay GPIOs are initialized to their inactive level before KNX starts.
2. The complete configuration is validated before KNX callbacks are registered.
3. Protocol-neutral restore policy resolves relay state before initial status is
   eligible for publication.
4. If KNX is disabled, the adapter remains initialized but does not start the
   transport.
5. If the IP network is offline, relay state is unchanged and KNX is reported
   offline. Local, Modbus, and web operation continue according to lifecycle
   policy.
6. On network availability, the routing transport starts and the startup
   transmit delay begins.
7. Initial configured channel status, channel faults, and device fault are
   published after the delay and paced by the minimum telegram interval.
8. Recovery never replays an old switch command. It republishes current state.

A restart clears volatile publication history and counters but retains validated
KNX configuration. Factory reset removes mutable bindings and restarts with KNX
disabled and relays governed by the factory-reset safety policy.

## Status Behavior

Applied state from `RelayCommandService` is authoritative. The adapter never
echoes an inbound command optimistically.

- A successful state transition schedules one status publication.
- An idempotent command does not create a new transition sequence.
- Changes originating from Modbus, web, CLI, button, restore, or safety logic
  are published exactly like KNX-originated changes.
- Cyclic status is optional and channel paced.
- Only one spontaneous KNX telegram is emitted per adapter poll, and all
  spontaneous telegrams obey the minimum interval.
- A GPIO application failure retains the previous applied status and causes the
  fault object to assert.
- Read requests are supported only on configured switch objects when the global
  read-switch parameter is enabled. Status and diagnostic objects are
  transmit-only in v1.0.

## Central Objects

Central switch and central off use fixed-size participant masks. The full group
is validated before any channel command is enqueued.

- Central switch `0`: SetOff for participating channels.
- Central switch `1`: SetOn for participating channels.
- Central off `0`: no action and treated as valid.
- Central off `1`: SetOff for participating channels.
- No participants: valid no-op.
- Any policy rejection prevents partial enqueueing.
- Central objects do not bypass disabled-channel, safety-lockout, queue, or
  lifecycle rules.

## Scene Objects

Object offsets `B+4` and `B+5` and their DPTs are reserved for compatibility,
but they are not exposed in application model v1.0. This is deliberate:

- KNX scene bindings are absent from the persisted schema.
- DPT 17.001 and 18.001 telegram decoding is not connected to `SceneService`.
- Learned-scene persistence is not connected to the KNX adapter.
- The selected stack's callback-assignment capacity is already bounded by the
  baseline command objects.

Scene objects may become conditional objects only in a later compatible model
revision after all four points are implemented and tested. Until then, ETS
product data must hide them.

## Diagnostics

Runtime diagnostics expose:

| Diagnostic | Meaning | Reset behavior |
|---|---|---|
| Available | KNX adapter/library initialized | Recomputed at initialization |
| Bus online | KNX enabled, IP online, and routing transport started | Recomputed on network/transport transitions |
| Valid telegrams | Accepted writes and successful read responses | Volatile; clears on restart |
| Telegram errors | Unknown binding, unsupported service, malformed input, or rejected command | Volatile; clears on restart |
| KNX unavailable fault | Callback or transport initialization failed | Cleared after successful initialization |
| KNX bus-off fault | Network/transport became unavailable | Cleared after successful transport recovery |
| Channel fault object | Output fault or safety lockout for one channel | Recomputed from authoritative relay snapshots |
| Device fault object | At least one active reportable fault | Recomputed from diagnostics snapshot |

Diagnostics must not expose Wi-Fi credentials, security keys, or private
commissioning material. Repeated transport failures must not block relay,
watchdog, Modbus, or local-control processing.

## ETS And Product-Data Strategy

### Current Boundary

Application model v1.0 is configuration-driven and cannot truthfully be sold as
ETS-programmable. The current stack starts KNXnet/IP routing and registers group
callbacks, but it does not implement the complete KNX device-management,
download, association-table, parameter-memory, programming-mode, or KNX Secure
contract required by an ETS product.

No hand-authored or unofficial `.knxprod` file should be shipped for this
firmware.

### Authoritative Sources

Until ETS integration is complete:

1. `knx_application_model.h` is authoritative for object numbers, DPTs, flags,
   channel blocks, and exposure state.
2. The versioned domain configuration is authoritative for implemented
   parameter ranges and defaults.
3. This document is the external behavioral contract.
4. The KNX Manufacturer Tool project will become the product-data source only
   after automated parity checks compare its exported model with the firmware
   catalog.

### Product Identity

Before creating distributable product data, assign and freeze:

- KNX Association manufacturer ID;
- product/order number matching the provisioned `product_id` policy;
- hardware type and revision compatibility list;
- application program number and application version `1.0`;
- medium type KNX IP;
- supported languages and translated parameter/object text;
- firmware compatibility range and migration policy.

Development or placeholder manufacturer identifiers must never appear in a
production `.knxprod` package.

### Parameter Pages

The first ETS parameter tree should contain:

1. General: startup delay, minimum telegram interval, cyclic status, heartbeat,
   and switch-object reads.
2. Central functions: object enablement and per-channel participation.
3. Channel A through F: channel and command/status/fault object enablement,
   polarity, and startup status behavior.
4. Diagnostics: read-only capability and application-version information.
5. Scenes: hidden in v1.0; no placeholder controls visible to installers.

Visibility conditions must suppress communication objects whose feature or
channel is disabled. Object numbers must remain allocated even while hidden.
The individual address and communication-object group-address associations are
commissioning data managed by ETS, not application parameter controls. The
future download bridge must translate ETS association tables into the bounded
runtime binding model without presenting group addresses as parameter fields.

### Required Firmware Evolution

Before ETS download can replace JSON commissioning, the firmware must gain:

- a maintained, certifiable KNX stack with device-management and programming
  mode support;
- a local physical programming-mode gesture and visible indication;
- explicit optional group-address storage so valid address `0/0/0` is distinct
  from unassigned;
- bounded multiple group-address associations per communication object;
- persistent parameter, communication-object, and association tables with
  transactional activation and rollback;
- mapping from ETS parameter memory into validated domain configuration;
- download-complete validation followed by controlled adapter restart;
- masking/version checks that reject product data for incompatible hardware or
  firmware;
- KNX Secure key storage and lifecycle if Secure is advertised.

These changes require a configuration-schema migration. Existing single primary
bindings must migrate into the first association slot, while every new feature
uses a safe disabled default.

### Product-Data Build Pipeline

1. Author the application program in the current KNX Manufacturer Tool using
   the frozen IDs and object catalog.
2. Export machine-readable object and parameter metadata for CI comparison with
   `knx_application_model.h` and domain defaults.
3. Fail CI on object-number, DPT, flag, range, default, or visibility drift.
4. Produce `.knxprod` only through the Manufacturer Tool and approved signing
   process; do not generate or patch signed archives with repository scripts.
5. Archive the Manufacturer Tool source, exported metadata, `.knxprod`, firmware
   digest, hardware mask/version, and release notes together.
6. Version the application program independently from firmware while recording
   the compatible firmware range.

### Release Gates

A `.knxprod` release requires all of the following:

- ETS import and parameter-page validation;
- individual-address and full/partial application download tests;
- association-table tests including multiple addresses and `0/0/0`;
- all six channels tested for write, read where enabled, status, fault, central
  operations, startup publication, and network recovery;
- interrupted-download rollback and incompatible-mask rejection;
- malformed, duplicate, burst, and reconnect traffic tests;
- proof that commissioning and network failure cannot pulse relays or starve
  watchdog/Modbus processing;
- KNX Association registration, interworking/conformance work, and permission
  for every certification claim shown in product literature.

Until those gates pass, release notes and user interfaces must describe the
feature as configuration-driven KNX/IP routing support, not ETS compatibility
or KNX certification.
