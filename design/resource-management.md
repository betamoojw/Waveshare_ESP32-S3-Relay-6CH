# Resource Management And Ownership

## Status

This document is the normative resource ownership contract for production
firmware. Every hardware handle, RTOS object, socket, timer, queue, and mutable
buffer MUST have exactly one owner. A borrower may invoke a typed port while the
owner is alive, but MUST NOT initialize, reconfigure, stop, delete, resize, or
retain the underlying resource.

The composition root in `Application` owns the lifetimes and construction order
of concrete services. Ownership of a service object does not transfer ownership
of resources that the service only borrows through a port or reference.

## Ownership Ledger

| Resource | Sole owner | Borrowers | Budget / capacity | Release rule |
|---|---|---|---:|---|
| Arduino application task | Arduino core; stack budget selected by `src/main.cpp` | `Application` executes on it | 8192 bytes | Framework restart |
| Wi-Fi station/AP, scans, ESP network event tasks | `NetworkManager` through Arduino Wi-Fi | Web and diagnostics use network ports/snapshots | Vendor task configuration; at most one active scan | `NetworkManager::shutdown()` deletes scan results, AP, station connection, and radio state |
| HTTPS server task | `WebServerAdapter` through PsychicHttp | Route handlers | 8192-byte stack | `WebServerAdapter::stop()` |
| HTTPS/TLS and WebSocket sockets | `WebServerAdapter` | Authenticated clients | `maximumWebSocketClients + 3` open sockets | Server stop or peer close; client slots cleared on stop |
| PsychicHttp async workers | PsychicHttp, only when `ENABLE_ASYNC` is defined | None currently | Disabled; enabling would create 8 x 4096-byte tasks, one queue, and one semaphore | Enabling requires an ADR and renewed hardware load evidence |
| Modbus UART1 / RS-485 transport | `Esp32ModbusSerialTransport` | `ModbusRtuAdapter` through `Rs485Hal` | One UART, bounded caller buffers | `shutdown()` / destructor calls `HardwareSerial::end()` |
| USB serial byte stream | `Application` composition | CLI and Improv serial filter | 64 input bytes processed per update; CLI buffer below | Framework restart |
| Task-watchdog registration | `Esp32TaskWatchdog` | Diagnostics reads health only | One registration for the Arduino loop task | Adapter destructor unregisters only its own registration |
| Relay command queue | `Application::RelayCommandQueue` | `SwitchingPolicyService` submits; application loop consumes | 16 batches, including 2 safety-reserved slots | Cleared during initialization and factory reset |
| Web application request queue | `WebRequestQueue` | HTTPS task submits; application loop consumes | 8 requests and 16 retained results | Owner clears arrays; owner mutex protects all queue/result invariants |
| Web command tracker | `WebCommandTracker` | HTTPS task begins/reads; application loop completes/expires | 32 commands, 60-second retention | Owner clears/ages entries; owner mutex protects the array |
| Web security records and sessions | `WebSecurityService` | Web/CLI use typed service and security ports | 4 users and 2 sessions | Owner mutex protects mutable state; factory reset clears users/sessions and preserves security identity |
| Web event journal | `Application::WebEventJournal` | Web publisher reads on the application loop | 64 events | Cleared during initialization; sequence gaps require snapshot resync |
| Relay timers | `RelayTimerService` | Application loop calls `update()` | One pending command per relay channel | Volatile; cancelled during initialization/restart flow |
| Scene storage | `SceneService` | Protocol-neutral callers | Compile-time fixed scene/channel arrays | Volatile unless configuration persistence is explicitly added |
| NVS configuration and diagnostic records | NVS adapters | Application services through typed APIs | Fixed A/B records | Adapter destructors close `Preferences`; factory reset transactionally saves safe configuration and preserves identity/counters |
| LittleFS configuration buffer | `LittleFsConfigurationSource` | Configuration service through ports | 8192-byte fixed array; 256-byte copy buffer | Object lifetime; files use scoped handles |
| CLI parser buffer | `CliAdapter` | Embedded CLI library | 4096-byte fixed array; 256-byte RX and command regions | Object lifetime |
| Web request/frame buffers | `WebServerAdapter` and PsychicHttp request scope | Route implementation | 2048-byte body/frame limit; bounded local serialization buffers | Returned with request stack/frame ownership |
| Diagnostic snapshots/counters | `DiagnosticsService` | CLI, web, protocols read snapshots | Fixed arrays and saturating counters | Application lifetime; selected counters checkpoint to NVS |

PsychicHttp, Arduino Wi-Fi, TLS, ArduinoJson, and the pinned logger perform some
vendor-library dynamic allocation. The adapter that invokes each library owns
the resulting lifetime even where the vendor API hides the underlying handle.
No second adapter may call the corresponding global start/stop API.

## Heap And PSRAM

- Internal heap is shared platform capacity, not a transferable object. Every
  allocation is owned by the component or request scope that creates it.
- Application/domain hot paths MUST use fixed-capacity value storage. New direct
  `malloc`, `calloc`, `realloc`, `free`, `new`, or `delete` calls are prohibited.
- Startup-only vendor allocation and bounded request-scoped parsing require
  hardware measurements. Retained request pointers after callback completion
  are prohibited.
- Diagnostics sample current/minimum internal heap, largest allocatable block,
  and PSRAM. Falling below 64 KiB free internal heap or a 32 KiB largest block
  activates `ResourceExhaustion`; recovery clears the fault. These values match
  the mandatory web hardware load gate.
- PSRAM is optional acceleration only. Relay control, queues, safety state,
  credentials, and protocol correctness MUST work with zero PSRAM. Code MUST NOT
  move required state to PSRAM or branch into weaker behavior when it is absent.
- Increasing a queue, socket, stack, frame, or client limit to hide exhaustion
  is prohibited. Capacity changes require measured internal-heap and stack data.

## Tasks, Stacks, And Synchronization

Application services execute on the Arduino loop task and MUST NOT create a
FreeRTOS task. A new task requires an ADR naming its owner, priority, core
affinity, stack size in bytes, blocking behavior, watchdog policy, shutdown
signal, join/delete path, and measured stack high-water mark.

The only application-owned mutexes are colocated with cross-task mutable state:

- `WebRequestQueue::mutex_`;
- `WebCommandTracker::mutex_`;
- `WebSecurityService::mutex_`.

Callers MUST NOT hold an external lock while invoking these services. Relay
commands, timers, scenes, diagnostics mutation, Modbus polling, KNX polling, and
the event journal are serialized on the application loop and MUST remain free of
unnecessary mutexes. Vendor mutexes and event-loop synchronization remain owned
by the vendor component that creates them.

## Sockets, Timers, Queues, And Buffers

- `NetworkManager` is the only component allowed to call Wi-Fi station/AP/scan
  lifecycle APIs. Other components use `NetworkStatusPort` and
  `NetworkControlPort`.
- `WebServerAdapter` is the only HTTP/TLS/WebSocket listener and socket owner.
  No service may retain PsychicHttp request, response, frame, or client pointers
  beyond the documented callback/client lifetime.
- `Esp32ModbusSerialTransport` is the only UART1 owner. Modbus parsing and
  application gateways borrow `Rs485Hal` and MUST NOT call `Serial1.begin/end`.
- `RelayCommandQueue`, `WebRequestQueue`, and vendor queues MUST reject excess
  work explicitly; overwrite, unbounded growth, and blocking the relay loop are
  prohibited.
- Relay deadlines use `RelayTimerService` fixed state and monotonic timestamps;
  no FreeRTOS software timer is created. Any future RTOS timer must have one
  named owner and an idempotent delete path.
- Large local buffers must be charged to the stack of the task executing them.
  Buffer capacities are compile-time constants, and all parsers/serializers must
  receive explicit destination capacities.

## Shutdown Order

Controlled restart and destructive maintenance use this order:

1. Stop accepting new work and force relay safety policy as required.
2. Flush dirty persistent diagnostic counters once.
3. Stop `WebServerAdapter` to close HTTP/TLS/WebSocket access.
4. Shut down `NetworkManager` to release scans, AP/station state, and Wi-Fi.
5. Shut down `Esp32ModbusSerialTransport` to release UART1.
6. Restart through the ESP platform.

Shutdown methods MUST be idempotent. Destructors may delegate to them, but a
borrower MUST never invoke cleanup on a resource it does not own.

## Release Evidence

Production release MUST satisfy `design/web-hardware-load-gate.md`, including
heap recovery, queue high-water marks, socket cycling, task-watchdog health,
relay scheduling, Modbus scheduling, and the 12-hour soak. A firmware build alone
does not prove resource safety.