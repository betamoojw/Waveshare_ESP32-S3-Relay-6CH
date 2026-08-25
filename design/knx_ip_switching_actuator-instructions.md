# KNX/IP Switching Actuator Requirements

## 1. Purpose and Status

This document defines the normative parameter and communication-object requirements for the six-channel KNX/IP switching actuator. It complements `software-architecture-instructions.md`; that document remains authoritative for safety, layering, relay ownership, configuration persistence, and concurrency.

The implemented baseline product contract is [KNX/IP](../docs/protocols/knx-ip.md). Sections describing the commercial feature set are roadmap requirements and MUST NOT be interpreted as currently exposed firmware or ETS product-data functionality.

The keywords **MUST**, **MUST NOT**, **SHOULD**, **SHOULD NOT**, and **MAY** are requirements. "Communication object" means a KNX group object. Object numbers in this document are the stable project object numbers and are not copied from another manufacturer's application program.

The profile is informed by the feature sets commonly offered by commercial ABB, MDT, and Theben KNX switch actuators: independent channel switching, applied-state feedback, central functions, startup behavior, delays, staircase timing, scenes, lock/forced operation, logic, counters, and diagnostics. It does not claim compatibility with, or certification as, any referenced product.

## 2. Product Profile and Delivery Tiers

The target has six independent relay channels. Each channel controls one binary load and uses KNX DPT 1.001 for its normal switch command and status.

Requirements are divided into two delivery tiers:

- **Baseline (MUST):** KNX/IP transport, individual address, six switch command objects, six applied-state objects, channel enablement, safe startup, read response, cross-protocol convergence, fault indication, and bounded diagnostics.
- **Commercial feature set (SHOULD):** central switching, lock and forced operation, on/off delays, staircase timer, scenes, logic gates, cyclic status, operating-hour counters, switching-cycle counters, and enhanced device diagnostics.

An implementation MUST NOT expose a communication object until its complete behavior, persistence, validation, diagnostics, and tests are implemented. Unsupported functions MUST remain hidden or disabled rather than behaving partially.

## 3. Architectural Constraints

1. The KNX adapter MUST use a maintained KNX stack for KNXnet/IP framing, routing or tunnelling, sequence handling, duplicate suppression, acknowledgements, retransmission, datapoint encoding, and KNX Secure when enabled.
2. KNX callbacks MUST translate telegrams into typed application commands and enqueue them. They MUST NOT write GPIO or mutate authoritative relay state.
3. The `RelayCommandService` MUST remain the authoritative source for requested and applied relay state.
4. Status objects MUST report applied state after successful output application. They MUST NOT optimistically echo a received command.
5. Central, lock, forced-operation, timer, scene, and logic behavior MUST be implemented as protocol-neutral domain/application policies shared by every command source where applicable. `SwitchingPolicyService` owns typed single-channel and atomic group command construction; KNX callbacks only decode DPTs, resolve configured bindings, and invoke that service. Protocol-specific switching policy in a KNX callback is prohibited.
6. KNX processing MUST be non-blocking and execute at least once every 10 ms while the application is operational.
7. Dynamic allocation after KNX startup SHOULD be avoided. Telegram processing, pending status publication, timers, scenes, and duplicate tracking MUST use bounded storage.
8. Modbus, CLI, web, buttons, and KNX MUST observe one converged applied state and one arbitration policy.
9. A KNX/IP build without supported transport hardware or stack capability MUST use `NullKnxAdapter`, report KNX unavailable, and keep relays safe.
10. A production claim of KNX conformance or certification MUST be supported by the appropriate KNX Association product-registration and interworking process. Use of multicast routing alone is not evidence of certification.

## 4. KNX Address and Datapoint Rules

### 4.1 Individual Address

The individual address MUST use the KNX three-level form `area.line.device` and the packed 16-bit representation:

```text
(area << 12) | (line << 8) | device
```

Valid commissioning ranges are area `0..15`, line `0..15`, and device `0..255`. The unprogrammed address `15.15.255` MAY be used only during commissioning. Packed value `0` MUST NOT be accepted as an enabled production configuration.

### 4.2 Group Addresses

Group addresses MUST support the KNX three-level range `0/0/0` through `31/7/255`. The persistent representation MUST distinguish "unassigned" from the valid group address `0/0/0`; using numeric zero as both values is prohibited in the next configuration schema.

An eventual ETS application program SHOULD allow multiple group-address associations per object according to KNX rules. The embedded JSON configuration MAY initially bind one primary group address per object, but this is a product limitation that MUST be documented.

### 4.3 Flags

The object tables use these KNX flag abbreviations:

| Flag | Meaning |
|---|---|
| C | Communication enabled |
| R | Value may be read from the actuator |
| W | Object accepts group writes |
| T | Object may transmit a group value |
| U | Received group response updates the object value |

Command objects normally use `C,W`. Status and diagnostic objects normally use `C,R,T`. A parameter MAY add `R` to a switch command object; a read response MUST then contain the current applied state.

## 5. General Parameters

| Parameter | Values and default | Requirement |
|---|---|---|
| KNX function | `disabled`, `enabled`; default `disabled` | Enabling MUST require valid transport and address configuration. |
| Product mode | `baseline`, `commercial`; default `baseline` | Controls which implemented parameter pages and objects are exposed. |
| Individual address | KNX individual address; no production default | MUST be uniquely commissioned per device. |
| Transport mode | `routing`, `tunnelling`; default `routing` for the current stack | Only modes fully supported by the selected stack MAY be offered. |
| Routing multicast address | default `224.0.23.12` | A non-default address MAY be allowed only where supported by KNXnet/IP and the stack. |
| IP assignment | `DHCP`, `static`; default `DHCP` | Static mode MUST validate IPv4 address, prefix/netmask, gateway, and DNS fields. |
| Startup transmit delay | `0..60 s`; default `3 s` | Random or deterministic jitter of up to 500 ms SHOULD be added to avoid telegram bursts. Safety initialization MUST not wait for this delay. |
| Minimum inter-telegram interval | `20..1000 ms`; default `100 ms` | Applies to spontaneous status and diagnostic transmissions, not required protocol responses. |
| Status after startup | `none`, `changed only`, `all enabled channels`; default `all enabled channels` | Publication occurs only after startup delay and configuration validation. |
| Cyclic status interval | `disabled`, `10 s..24 h`; default `disabled` | Cyclic traffic MUST be rate-limited and staggered by channel. |
| Read switch object | `disabled`, `enabled`; default `disabled` | When enabled, the switch command object gains `R` and answers with applied state. |
| Device-in-operation heartbeat | `disabled`, `10 s..24 h`; default `disabled` | Object 0 sends `1`; failure to transmit MUST be diagnosed without blocking relay control. |
| Programming mode | `local action only`; default required | Remote telegrams MUST NOT enter programming mode. The status indicator MUST visibly identify this mode. |
| Programming timeout | `1..60 min`; default `10 min` | Programming mode MUST leave automatically at timeout or successful commissioning. |
| Bus recovery behavior | `keep safe/current state`, `send status`; default both | Bus recovery MUST NOT replay stale switch commands. |
| Telegram rate limit | `10..200 writes/s`; default `50 writes/s` per device | Excess traffic MUST be rejected or coalesced safely and counted. Safety-off capacity remains reserved. |
| KNX Secure | `disabled`, `enabled when supported`; default `disabled` | Keys MUST use protected storage and MUST never be logged or returned by CLI/web diagnostics. |

## 6. Per-Channel Parameters

The following parameters apply independently to channels A through F. A disabled channel MUST expose no writable channel objects, reject all commands from every source, remain off, and report its disabled state through diagnostics.

### 6.1 Identity and Basic Switching

| Parameter | Values and default | Requirement |
|---|---|---|
| Channel enabled | `no`, `yes`; default `yes` | Disabled channels MUST remain at the board-defined inactive GPIO level. |
| Channel name | UTF-8, `0..30` display characters; default `Channel A` ... `Channel F` | Storage MUST be bounded and safely truncated or rejected before persistence. |
| Operating mode | `normal switching`, `staircase`; default `normal switching` | Other modes MUST not appear until implemented as domain policies. |
| Command polarity | `0=off/1=on`, `0=on/1=off`; default `0=off/1=on` | This changes telegram interpretation only; it MUST NOT alter BSP electrical polarity or safe GPIO initialization. |
| Status polarity | `normal`, `inverted`; default `normal` | Applied-state storage remains non-inverted. Inversion occurs only during datapoint encoding. |
| Status transmission | `on change`, `on change and cyclic`; default `on change` | An idempotent command MUST NOT generate duplicate change status. |
| Send status after startup | `inherit general`, `no`, `yes`; default `inherit general` | Status MUST reflect applied state after restore policy completes. |
| Restore after normal restart | `all off`, `configured default`, `last known`; default `all off` | Must use `RelayCommandService`. `last known` requires valid wear-aware persistence. |
| Behavior after abnormal reset | `all off`, optionally `configured default`; default `all off` | `last known` MUST NOT be offered unless an approved safety ADR allows it. |

### 6.2 Delay and Contact Protection

| Parameter | Values and default | Requirement |
|---|---|---|
| On delay | `0..86,400 s`; default `0 s` | A new off command cancels a pending on command unless explicitly configured otherwise. |
| Off delay | `0..86,400 s`; default `0 s` | Safety-off bypasses this delay. |
| Minimum off time | `0..86,400 s`; default `0 s` | An on request during this interval remains pending or is rejected according to the next parameter. |
| Minimum on time | `0..86,400 s`; default `0 s` | Safety-off and fault lockout bypass the minimum-on restriction. |
| Minimum-time conflict action | `defer latest`, `reject`; default `defer latest` | At most one bounded pending target state is stored per channel. Latest accepted state replaces older pending state. |
| Maximum on time | `disabled`, `1 s..7 d`; default `disabled` | Expiry generates a safety/policy off command and a diagnostic event. |
| Delay status | `final state only`, `include pending diagnostic`; default `final state only` | Switch status always remains the actual applied state. |

All deadlines MUST use wrap-safe monotonic arithmetic. Restart MUST cancel volatile pending delays unless persisted timer restoration is explicitly implemented and validated.

### 6.3 Staircase Function

| Parameter | Values and default | Requirement |
|---|---|---|
| Staircase duration | `1 s..24 h`; default `120 s` | An on telegram starts the timer and applies on through the normal command service. |
| Retrigger behavior | `restart`, `extend`, `ignore`; default `restart` | `extend` MUST have a configured maximum total duration. |
| Off telegram behavior | `switch off`, `ignore`; default `switch off` | Safety-off is never ignored. |
| Warning enabled | `no`, `yes`; default `no` | Warning MUST use a status/indicator event; repeated relay flashing is prohibited by default. |
| Warning time | `1..60 s`; default `30 s` | Must be less than staircase duration. |
| Remaining-time publication | `disabled`, `on change/request`, `cyclic`; default `disabled` | Uses object `B+12`; cyclic publication obeys the global rate limit. |

### 6.4 Lock and Forced Operation

| Parameter | Values and default | Requirement |
|---|---|---|
| Lock function | `disabled`, `enabled`; default `disabled` | Enables object `B+2`. |
| Lock polarity | `1=locked`, `0=locked`; default `1=locked` | Invalid or missing telegram data MUST NOT change lock state. |
| State on lock | `unchanged`, `off`, `on`; default `off` | The transition is applied through a typed policy command. |
| State on unlock | `unchanged`, `off`, `on`, `restore latest deferred`; default `unchanged` | Deferred-command storage is bounded to one target state per channel. |
| Forced operation | `disabled`, `enabled`; default `disabled` | Enables DPT 2.001 object `B+3`. |
| State after forced operation | `unchanged`, `restore latest deferred`; default `unchanged` | Safety lockout always has higher priority than forced on. |

Priority MUST be deterministic: electrical/fault safety off, safety lockout, forced operation, channel lock, then normal accepted commands in serialized receive order. A forced-on telegram MUST be rejected while safety lockout is active.

### 6.5 Scenes

| Parameter | Values and default | Requirement |
|---|---|---|
| Scene function | `disabled`, `enabled`; default `disabled` | Enables objects `B+4` and optionally `B+5`. |
| Scene slots | `1..8`; default `8` when enabled | Each slot maps one KNX scene number to an off/on state. |
| Scene number | `1..64`, unique per channel slot | Duplicate scene numbers within one channel MUST fail validation. |
| Scene state | `off`, `on`; default `off` | Scene invocation uses the same arbitration and relay service as switch commands. |
| Scene delay | `0..86,400 s`; default `0 s` | Delayed scenes obey cancellation, lock, and safety rules. |
| Scene learning | `disabled`, `enabled`; default `disabled` | Learning stores current applied state, never requested or pending state. |
| Persist learned scenes | `no`, `yes`; default `yes` | Writes MUST be coalesced and failure-atomic; flash MUST NOT be written in the callback. |

Scene number payloads use the KNX zero-based wire encoding defined by DPT 17.001/18.001 while the installer-facing parameter uses scene numbers `1..64`. The adapter MUST perform this conversion explicitly.

### 6.6 Logic

| Parameter | Values and default | Requirement |
|---|---|---|
| Logic function | `disabled`, `AND`, `OR`, `XOR`; default `disabled` | Enables objects `B+7` and `B+8`. |
| Switch input role | `not used`, `logic input`; default `not used` | When used, normal switch commands feed the selected logic expression rather than bypassing it. |
| Input inversion | independently `normal`, `inverted`; default `normal` | Inversion is applied before evaluation. |
| Missing-input startup value | `0`, `1`, `unknown`; default `unknown` | `unknown` MUST not produce an on transition until all required inputs are valid. |
| Logic result action | `follow result`, `on pulse`, `off pulse`; default `follow result` | Pulse modes require bounded timer behavior and MUST remain hidden until implemented. |

Logic processing MUST be deterministic, non-recursive, and bounded. A channel output status MUST NOT be fed back into its own logic inputs through internal shortcuts. KNX group-address loops remain an installation concern but repeated identical telegrams MUST not retrigger idempotent state changes.

### 6.7 Counters and Maintenance

| Parameter | Values and default | Requirement |
|---|---|---|
| Operating-hours counter | `disabled`, `enabled`; default `disabled` | Counts accumulated applied-on time, not requested-on time. |
| Switching-cycle counter | `disabled`, `enabled`; default `disabled` | Increments only for successful physical off-to-on transitions. |
| Counter transmit mode | `on request`, `on change threshold`, `cyclic`; default `on request` | Change threshold and interval MUST be configurable and rate-limited. |
| Counter persistence interval | `1..24 h`; default `6 h` | Values MUST also flush on controlled shutdown where available. |
| Counter reset | `disabled`, `maintenance authorized`; default `disabled` | Reset MUST require explicit authorization and MUST be recorded in diagnostics. |

Counters MUST saturate at `0xFFFF'FFFF` and set a diagnostic flag. They MUST NOT silently wrap.

## 7. Central Parameters

| Parameter | Values and default | Requirement |
|---|---|---|
| Central switch | `disabled`, `enabled`; default `disabled` | Enables object 1. Each channel separately opts in. |
| Central off | `disabled`, `enabled`; default `enabled` in commercial mode | Enables object 2. Value `1` requests off for all participating channels; value `0` has no effect. |
| Channel participates in central switch | `no`, `yes`; default `yes` | Evaluated during full batch validation. |
| Channel participates in central off | `no`, `yes`; default `yes` | Safety policies may still force off regardless of participation. |
| Central operation atomicity | `all-or-none`; fixed | Validate every participating channel before enqueueing any command. |
| Aggregate status | `disabled`, `all off/all on/mixed`, `any on`; default `disabled` | Enables object 3 using the selected encoding. |

Central commands are KNX commands, not privileged bypasses. Except for a separately modeled safety central-off policy, they use normal arbitration and return no optimistic status.

## 8. Communication Object Map

### 8.1 Numbering Rule

Object numbers `0..15` are device-wide. Each channel reserves a block of 16 objects:

```text
B = 16 + (channelIndex * 16), channelIndex = 0..5
```

Thus channel A uses `16..31`, B uses `32..47`, C uses `48..63`, D uses `64..79`, E uses `80..95`, and F uses `96..111`. Reserved numbers MUST remain reserved to preserve future ETS compatibility.

### 8.2 Device-Wide Objects

| No. | Object | DPT | Flags | Exposure and behavior |
|---:|---|---|---|---|
| 0 | Device in operation | 1.002 Boolean | C,R,T | Optional heartbeat. Sends `1`; read returns `1` only while operational. |
| 1 | Central switch | 1.001 Switch | C,W | Optional. `0` requests off and `1` requests on for participating channels. |
| 2 | Central off | 1.003 Enable | C,W | Optional. `1` requests atomic off; `0` has no effect. |
| 3 | Aggregate relay status | 1.001 or 5.010 | C,R,T | Optional. DPT is fixed by selected aggregate mode and MUST not change at runtime. |
| 4 | Device fault | 1.005 Alarm | C,R,T | Baseline. `1` when any reportable device/channel fault is active. |
| 5 | KNX/IP transport status | 1.005 Alarm | C,R,T | Optional. `1` denotes degraded/unavailable transport when publication is possible. |
| 6 | Reset diagnostic counters | 1.015 Reset | C,W | Optional and maintenance-authorized. `1` performs reset; `0` has no effect. |
| 7..15 | Reserved | - | - | MUST NOT be assigned in this profile revision. |

### 8.3 Per-Channel Objects

In the following table, `B` is the channel block base.

| Offset | Object | DPT | Flags | Exposure and behavior |
|---:|---|---|---|---|
| B+0 | Channel X switch | 1.001 Switch | C,W; optional R | Baseline. Writes enqueue `SetOff`/`SetOn`. Optional reads return applied state. |
| B+1 | Channel X switch status | 1.001 Switch | C,R,T | Baseline. Reports applied state on successful change and according to status parameters. |
| B+2 | Channel X lock | 1.003 Enable | C,W | Optional. Lock polarity and lock/unlock action are parameterized. |
| B+3 | Channel X forced operation | 2.001 Switch control | C,W | Optional. Control bit `0` ends forcing; control bit `1` forces the value bit. |
| B+4 | Channel X scene | 17.001 Scene number | C,W | Optional. Recalls configured scene `1..64`. |
| B+5 | Channel X scene control | 18.001 Scene control | C,W | Optional. Recalls or learns a scene when learning is enabled. |
| B+6 | Channel X staircase trigger | 1.010 Start/stop | C,W | Optional. Start and stop semantics follow staircase parameters. |
| B+7 | Channel X logic input 1 | 1.002 Boolean | C,W,U | Optional. First external logic operand. |
| B+8 | Channel X logic input 2 | 1.002 Boolean | C,W,U | Optional. Second external logic operand. |
| B+9 | Channel X fault | 1.005 Alarm | C,R,T | Baseline. `1` while disabled by fault, output failure, or safety lockout. |
| B+10 | Channel X operating hours | 12.001 Counter pulses | C,R,T | Optional. Unsigned 32-bit accumulated applied-on hours. |
| B+11 | Channel X switching cycles | 12.001 Counter pulses | C,R,T | Optional. Unsigned 32-bit successful off-to-on transitions. |
| B+12 | Channel X remaining time | 7.005 Time period seconds | C,R,T | Optional. Remaining delay/staircase seconds, saturated to DPT range. |
| B+13 | Channel X pending command | 1.002 Boolean | C,R,T | Optional diagnostic. `1` while a validated delayed command is pending. |
| B+14..B+15 | Reserved | - | - | MUST NOT be assigned in this profile revision. |

### 8.4 Object Exposure Rules

1. Objects `B+0`, `B+1`, and `B+9` MUST exist for every enabled baseline channel.
2. Disabled channels SHOULD hide their entire block in ETS. Non-ETS configurations MUST reject writes to disabled objects.
3. Optional objects MUST appear only when their feature parameter is enabled.
4. DPT and payload length MUST be validated before any command or state mutation.
5. Group reads to readable objects MUST return the current snapshot value and MUST NOT enqueue commands.
6. A read to an unreadable or unknown object MUST be ignored or handled by the KNX stack according to KNX rules; it MUST NOT produce an application fault.
7. A malformed write MUST increment the invalid-telegram counter and leave all state unchanged.

## 9. Telegram and State Semantics

### 9.1 Switch Commands

- `0` maps to `RelayAction::SetOff`; `1` maps to `RelayAction::SetOn`, subject to configured command polarity.
- Commands MUST carry `CommandSource::Knx`, a nonzero correlation ID, and monotonic receive time.
- Retransmitted or duplicate telegrams MUST be handled by the KNX stack. An idempotent repeated set command MUST not increment transition sequence or switching-cycle count.
- If the application queue is full, the command MUST be rejected, queue-full diagnostics updated, and no status change published.
- Toggle is intentionally not part of the baseline KNX switch object. If later added, it MUST use a distinct edge-triggered object and duplicate protection.

### 9.2 Status Publication

- Status MUST be sent only after the relay output adapter accepts and applies the transition.
- A failed GPIO application MUST retain the previous applied status and assert the channel fault object.
- A status write received from another device MUST never alter authoritative actuator state.
- On cross-protocol changes, KNX status MUST publish the resulting applied state using the same rules as a KNX-originated change.
- On reconnect, the adapter MUST not burst more telegrams than allowed by the configured inter-telegram interval.

### 9.3 Initialization and Recovery

1. Relays are initialized off before KNX/IP starts.
2. Configuration is validated completely before callbacks and group-address bindings become active.
3. Restore actions run through `RelayCommandService` before initial status publication.
4. KNX/IP disconnection alone MUST NOT change relay state unless an explicit, centrally enforced bus-loss fail-safe policy is configured.
5. Bus recovery MUST restart the stack safely, refresh diagnostic state, and publish configured status without replaying old writes.
6. Reconfiguration of address bindings MUST be failure-atomic and SHOULD require a controlled adapter restart.

## 10. Configuration Model Requirements

The versioned domain configuration MUST eventually represent:

- KNX enablement, individual address, transport mode, IP assignment, startup delay, rate limits, heartbeat, and security state;
- a typed assignment collection for each exposed communication object, with explicit unassigned state;
- per-channel basic switching, timing, staircase, lock, forced-operation, scene, logic, counter, and status policy;
- central-function participation masks;
- feature/version capability bits so older firmware rejects unsupported configuration instead of partially applying it.

Configuration validation MUST reject:

- invalid individual or group addresses;
- duplicate bindings that create ambiguous command ownership inside the actuator;
- a status object bound as a writable command object;
- wrong DPT or payload width for a configured object;
- delays outside range or warning time not shorter than staircase duration;
- duplicate scene numbers within a channel;
- `last known` restore where persistence support is unavailable;
- secure mode without provisioned keys;
- enabled optional objects whose policy implementation is unavailable;
- any configuration exceeding fixed storage, callback, queue, or association capacities.

The current schema containing only `switchGroupAddresses[6]` and `statusGroupAddresses[6]` satisfies the baseline primary bindings but does not satisfy the commercial feature set. Its migration MUST preserve existing addresses and assign every new feature its safe disabled default.

## 11. Diagnostics Requirements

Diagnostics MUST expose at least:

- stack availability, IP link state, KNX/IP online state, individual address, and transport mode;
- valid, invalid, duplicate, rate-limited, and unsupported telegram counters;
- command accepted, rejected, and queue-full counters;
- status sent, status suppressed/coalesced, and transmission-failure counters;
- reconnect count, last online/offline transition time, and last transport error;
- per-channel fault, lock, forced-operation, pending timer, and counter-saturation state;
- configuration validity and whether KNX Secure is enabled, without exposing key material.

Repeated transport errors MUST be rate-limited in logs. Telegram payloads and group addresses MAY be logged at debug level only when they do not disclose protected commissioning or security information.

## 12. Security and Commissioning

1. Programming mode MUST require an explicit local gesture and visible indication.
2. Mutating KNX commands are unauthenticated unless KNX Secure is implemented; installation documentation MUST state this limitation.
3. KNX/IP multicast and maintenance networks MUST be isolated according to the deployment threat model.
4. Secure keys, device certificates, passwords, and provisioning tokens MUST never be present in repository defaults.
5. Factory reset MUST erase KNX address assignments, IP/security provisioning, learned scenes, and counters according to product policy, then restart with all relays off.
6. Commissioning writes MUST pass `validate -> stage -> persist -> apply`; interrupted persistence MUST leave the previous valid configuration usable.

## 13. Verification Requirements

### 13.1 Host Unit Tests

Tests MUST cover:

- packed individual/group address boundary values and explicit unassigned representation;
- DPT 1.001 command decode and applied-state encode, including polarity inversion;
- wrong payload length, unsupported DPT, unknown object, and disabled channel rejection;
- idempotent set behavior and absence of optimistic status;
- status publication after KNX, Modbus, CLI, button, restore, and safety commands;
- lock, forced-operation, safety priority, and deferred-command behavior;
- wrap-safe on/off delay, minimum-time, maximum-on, and staircase deadlines;
- scene recall/learn conversion for installer values `1..64` and wire values `0..63`;
- logic truth tables and unknown startup inputs;
- central all-or-none validation across six channels;
- operating-hour and switching-cycle counting, saturation, and persistence coalescing;
- queue-full, rate-limit, reconnect, duplicate, and status-coalescing behavior;
- migration from the current baseline KNX configuration schema.

### 13.2 Integration Tests

Integration tests MUST use the selected KNX stack or a conforming fake transport to verify:

- group write, group read, group response, repeated telegram, and malformed APDU handling;
- all six channel object blocks and all enabled optional objects;
- startup delay, ordered initial publication, cyclic status, and heartbeat throttling;
- Wi-Fi loss, multicast loss, reconnect, stack restart, and absence of command replay;
- simultaneous Modbus and KNX commands converging on one applied state;
- no direct relay transition from a status, diagnostic, read, or malformed object;
- bounded memory and scheduler latency under maximum configured telegram rate.

### 13.3 Hardware and Commissioning Tests

Before release, hardware tests MUST verify no relay pulse during boot, programming, network reconnect, ETS download, factory reset, or watchdog recovery. Commissioning tests MUST verify the declared KNX/IP routing/tunnelling behavior with at least two independent KNX/IP tools or devices and the final ETS application program when one is supplied.

## 14. Acceptance Criteria

The KNX/IP switching actuator profile is complete when:

- every enabled channel accepts DPT 1.001 commands only through the application queue;
- reads and status telegrams return actual applied state;
- all object numbers, DPTs, flags, parameter ranges, defaults, and enable conditions match this document and product documentation;
- optional commercial functions are either fully implemented and tested or absent/disabled;
- configuration migration is failure-atomic and defaults new behavior to safe/off/disabled;
- bus loss and malformed or high-rate traffic cannot directly change GPIO, starve relay processing, or trigger a reboot;
- Modbus and KNX remain converged after commands from either protocol;
- production commissioning, security, diagnostics, and limitations are documented;
- any KNX certification claim is supported by completed external conformance work.

## 15. Commercial Reference Set

The requirements were informed by publicly available commercial product families and manuals accessed on 2026-08-14:

- MDT Switch Actuator AKS Standard series and MDT technical-manual catalogue: <https://www.mdt-group.com/for-professionals/downloads/technical-manuals.html>
- MDT third-generation Switch Actuator technical manual: <https://www.mdt.de/fileadmin/user_upload/user_upload/download/MDT_TM_Switch_Actuator_03.pdf>
- ABB i-bus KNX switch actuator product family: <https://new.abb.com/low-voltage/products/building-automation/product-range/knx/products/actuators/switch-actuators>
- Theben KNX actuator product family: <https://www.theben.de/en/actuators-1000500-l/>
- KNX Association system and datapoint references: <https://www.knx.org/knx-en/for-professionals/>

Manufacturer manuals are design references only. The implementation MUST use licensed specifications and the selected stack's verified DPT behavior for conformance work.