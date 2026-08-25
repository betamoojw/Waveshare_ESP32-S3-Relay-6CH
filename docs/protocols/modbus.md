# Modbus Register Map v1.0

Status: Frozen product interface  
Protocol: Modbus RTU  
Addressing: Zero-based Protocol Data Unit (PDU) addresses

This document is the normative public contract for the Switch Actuator Modbus
interface. A client using one-based reference notation must add the appropriate
reference prefix and offset; for example, holding register PDU address `32` is
commonly displayed as `40033`. PDU addresses are authoritative.

## Compatibility Policy

The register map version is `1.0`. Firmware may add registers in currently
reserved ranges in a backward-compatible minor revision. Existing addresses,
encodings, access rights, and meanings must not change without a new major map
version. Clients must access only addresses defined here and must not infer
values from reserved addresses.

All 16-bit registers use standard Modbus big-endian byte order. The 32-bit
uptime value uses the high 16-bit word first. Values are unsigned unless stated
otherwise.

## Transport Configuration

| Setting | Default | Supported values | Persistence | Reset behavior |
|---|---:|---|---|---|
| Role | Server | Server; client is a maintenance-only runtime mode | No | Returns to server after restart |
| Unit ID | `10` | `1..247`; `0` is broadcast only | Yes | Retained after restart; factory reset restores deployment default |
| Baud rate | `115200` bit/s | `9600`, `19200`, `38400`, `57600`, `115200` | Yes | Retained after restart; factory reset restores `115200` |
| Data bits | `8` | `8` only | Yes | Always `8` |
| Parity | None | None, even, odd | Yes | Retained after restart; factory reset restores none |
| Stop bits | `1` | `1`, `2` | Yes | Retained after restart; factory reset restores `1` |

UART and Unit ID changes are validated and atomically persisted before a
controlled restart is requested. The response to the accepted write uses the
old transport settings; the new settings become effective after restart.

## Supported Functions

| Function | Name | Supported address spaces | Contract |
|---:|---|---|---|
| `01` | Read Coils | Coils `0..5` | Reads applied relay state |
| `02` | Read Discrete Inputs | Discrete inputs `0..5` | Reads applied relay state |
| `03` | Read Holding Registers | Defined readable holding registers | One request must remain inside one contiguous defined block |
| `04` | Read Input Registers | Defined input registers | One request must remain inside one contiguous defined block |
| `05` | Write Single Coil | Coils `0..5` | `OFF` requests SetOff; `ON` requests SetOn |
| `06` | Write Single Register | Defined writable holding registers | Entire value is validated before any side effect |
| `0F` | Write Multiple Coils | Coils `0..5` | Entire contiguous request is validated and submitted atomically |
| `10` | Write Multiple Registers | One defined writable holding block | Entire request is validated before any command is submitted |
| `2B/0E` | Read Device Identification | Basic objects `00..02` | Vendor, product code, firmware revision |

### Function 03

Function `03` reads only the holding-register blocks listed below. A request
must not cross a reserved gap. Registers `128`, `130`, and `132` are single-item
blocks and therefore require quantity `1`.

### Function 06

Function `06` writes one writable holding register. Relay command `2` is a
one-shot toggle and is never retained as register state. Writes to UART settings
or Unit ID persist atomically and request a controlled restart. Writes to
read-only or reserved addresses return exception `02`.

### Function 10

Function `10` accepts a contiguous subset of relay registers `32..37` or RGB
registers `48..51`. It also accepts quantity `1` at writable single-register
addresses `56`, `128`, and `130`. The complete request is validated first. If
one address or value is invalid, no command from that request is submitted.
Requests must not span map blocks or reserved gaps.

## Coils

| Address | Name | Data type | R/W | Default | Range | Persistence | Side effect | Reset behavior |
|---:|---|---|:---:|---:|---|---|---|---|
| `0..5` | Relay CH1..CH6 | Bit | R/W | `0` | `0`=off, `1`=on | Runtime state only | Queues SetOff/SetOn through the common switching policy; read returns applied state | Resolved by the relay safety/restore policy; fail-safe default is off |

## Discrete Inputs

| Address | Name | Data type | R/W | Default | Range | Persistence | Side effect | Reset behavior |
|---:|---|---|:---:|---:|---|---|---|---|
| `0..5` | Relay applied state CH1..CH6 | Bit | R | `0` | `0`=off, `1`=on | No | None | Follows resolved relay state after startup safety processing |

## Holding Registers

| Address | Name | Data type | R/W | Default | Range | Persistence | Side effect | Reset behavior |
|---:|---|---|:---:|---:|---|---|---|---|
| `32..37` | Relay command/state CH1..CH6 | `uint16` | R/W | `0` | Write: `0`=off, `1`=on, `2`=toggle; read: `0..1` | Runtime state only | Queues a typed relay command through arbitration; toggle executes once | Readback follows resolved applied relay state; fail-safe default is off |
| `48` | Maintenance RGB red | `uint16` | R/W | `0` | `0..255` | No | Updates transient maintenance color | Clears on restart or after 5 seconds |
| `49` | Maintenance RGB green | `uint16` | R/W | `0` | `0..255` | No | Updates transient maintenance color | Clears on restart or after 5 seconds |
| `50` | Maintenance RGB blue | `uint16` | R/W | `0` | `0..255` | No | Updates transient maintenance color | Clears on restart or after 5 seconds |
| `51` | Maintenance RGB brightness | `uint16` | R/W | `0` | Accepted `0..255`; output is capped by configured maximum brightness | No | Updates transient maintenance brightness | Clears on restart or after 5 seconds |
| `56` | Maintenance buzzer tone | `uint16` | W | N/A | `0..7`; `0`=silent | No | Plays the selected bounded tone for up to 100 ms using configured maximum duty | Stops on timeout or restart |
| `128` | UART settings | Bit field in `uint16` | R/W | `0x0004` (`115200`, none, 1 stop bit) | Encoding defined below | Yes | Atomically persists serial settings and requests controlled restart | Retained; factory reset restores `0x0004` |
| `130` | Unit ID | `uint16` | R/W | `10` | `1..247` | Yes | Atomically persists Unit ID and requests controlled restart | Retained; factory reset restores `10` |
| `132` | Software version | `uint16` | R | `104` | Firmware version encoding, major x 100 + minor (`FW-1.4.0` = `104`) | Firmware image | None | Value is derived from the running firmware compatibility label |

RGB values describe the active maintenance override, not the normal status LED
pattern. Critical-fault indication has priority over this override. Partial
writes preserve the other RGB components. Register `56` is write-only.

### UART Settings Encoding

| Bits | Field | Encoding |
|---|---|---|
| `2..0` | Baud index | `0`=9600, `1`=19200, `2`=38400, `3`=57600, `4`=115200 |
| `4..3` | Parity | `0`=none, `1`=even, `2`=odd; `3` is invalid |
| `5` | Stop bits | `0`=1 stop bit, `1`=2 stop bits |
| `15..6` | Reserved | Must be zero |

Data bits are fixed at 8 and are not encoded. Any unsupported baud index,
parity value, or nonzero reserved bit returns `Illegal Data Value` and leaves
configuration unchanged.

## Input Registers

| Address | Name | Data type | R/W | Default | Range | Persistence | Side effect | Reset behavior |
|---:|---|---|:---:|---:|---|---|---|---|
| `0..5` | Relay fault CH1..CH6 | Enum in `uint16` | R | `0` | `0`=none, `1`=output failure | No | None | Recomputed by relay hardware/application state |
| `8` | Lifecycle state | Enum in `uint16` | R | `0` | `0`=booting, `1`=configuring, `2`=operational, `3`=degraded, `4`=fault, `5`=restarting | No | None | Starts at booting |
| `9` | Uptime seconds high word | `uint16` | R | `0` | `0..65535` | No | None | Clears on restart |
| `10` | Uptime seconds low word | `uint16` | R | `0` | `0..65535` | No | None | Clears on restart |
| `11` | Accepted command count | Saturating `uint16` | R | `0` | `0..65535` | No | None | Clears on restart |
| `12` | Rejected command count | Saturating `uint16` | R | `0` | `0..65535` | No | None | Clears on restart |

Read uptime registers `9..10` in one Function `04` request for a coherent
32-bit snapshot. Counters saturate at `65535` rather than wrapping.

## Exceptions

| Code | Name | Returned when |
|---:|---|---|
| `01` | Illegal Function | Function is unsupported or unavailable in the active role |
| `02` | Illegal Data Address | Any address is reserved, a range crosses a block boundary, a read targets a write-only register, a write targets a read-only register, or a protected configuration register is broadcast |
| `03` | Illegal Data Value | Quantity/encoding/value is invalid, a relay action is rejected by policy, or a multi-register combination is invalid |
| `04` | Server Device Failure | Snapshot creation, persistence, hardware application, lifecycle transition, or command queue submission fails |

CRC-invalid and malformed frames are discarded according to Modbus RTU rules;
they do not receive an exception response. Requests for another Unit ID are
ignored.

## Broadcast Behavior

Unit ID `0` is the RTU broadcast address and never produces a response.

- FC05, FC0F, FC06, and FC10 operational writes to relay, RGB, and buzzer
  addresses are validated and may execute on broadcast.
- Broadcast writes to UART settings (`128`) and Unit ID (`130`) are rejected
  without changing persistent configuration. No exception frame is emitted for
  a broadcast.
- Read broadcasts do not mutate state and produce no response. Clients must not
  use broadcast with FC01, FC02, FC03, FC04, or FC2B/0E.

Broadcast acceptance means transport success cannot be confirmed. Safety and
arbitration can still reject an operational command locally, so broadcasts
must not be used where positive actuation confirmation is required.

## Reset And Failure Rules

Runtime relay, indicator, diagnostic, and uptime values are not configuration
storage. Relay outputs follow the separately defined relay safety policy during
power-on, brownout, watchdog reset, software restart, OTA, and factory reset.
Persistent Modbus configuration survives ordinary restart. Factory reset erases
mutable Modbus settings and restores safe domain defaults while preserving the
device identity defined in [Factory reset](../manufacturing/factory-reset.md).

Network loss does not change this RTU map or relay state. If a configuration
write cannot be persisted, the old active configuration remains authoritative
and the request returns Server Device Failure.

## Reserved Addresses

Every coil, discrete input, holding register, and input register not explicitly
listed in this document is reserved. Reserved reads and writes return Illegal
Data Address. Clients must not poll or write reserved locations.
