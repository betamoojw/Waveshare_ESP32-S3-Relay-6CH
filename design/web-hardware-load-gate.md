# Web Hardware Load Gate

## Status

This document defines the mandatory hardware-in-the-loop release gate for the
HTTPS and WebSocket management adapter on the Waveshare ESP32-S3-Relay-6CH.
Passing desktop tests or compiling firmware does not satisfy this gate.

Target profile:

- Waveshare ESP32-S3-Relay-6CH, 8 MiB flash, no PSRAM;
- Arduino-ESP32 3.1.3 and PsychicHttp 3.1.2;
- release build with the production partition table and LittleFS image;
- six representative relay loads or an electrically equivalent safe fixture;
- active Modbus RTU and KNX/IP traffic generators.

## Release Thresholds

All thresholds must pass in one uninterrupted run. A threshold may be changed
only through an approved ADR containing replacement measurements.

| Measurement | Required result |
|---|---:|
| Minimum free internal heap | at least 64 KiB |
| Largest free internal heap block | at least 32 KiB |
| Heap loss after disconnect and five-minute recovery | no more than 4 KiB from the matching pre-connection baseline |
| Authenticated WebSocket clients | 2 total, at most 1 per session |
| Web request queue high-water mark | no more than 8; no silent overwrite |
| WebSocket send failures during steady load | 0 |
| WebSocket sequence gaps during steady load | 0 |
| Relay scheduler maximum observed gap | no more than 20 ms |
| Modbus poll maximum observed gap | no more than 10 ms |
| Local authenticated snapshot latency | p95 no more than 250 ms; p99 no more than 500 ms |
| Relay command completion latency | p95 no more than 250 ms; p99 no more than 500 ms |
| Task watchdog | healthy throughout; no watchdog reset |
| Relay correctness | no missed, duplicated, or unintended transition |
| Fieldbus correctness | no load-induced malformed request or unexplained timeout |
| Soak duration | at least 12 hours |

A deliberate journal-overflow test is separate from steady load. It passes only
when each affected client receives `resync.required`, refetches authoritative
REST snapshots, and converges without replaying a mutation.

## Instrumentation

Capture `GET /api/v1/diagnostics` at baseline and every 60 seconds. Preserve at
least these fields:

- `heapLowWaterMarkBytes`;
- `largestFreeHeapBlockBytes`;
- `taskWatchdogHealthy`;
- `commandCounters`;
- `web.activeClients`;
- `web.requestQueueDepth` and `web.requestQueueHighWaterMark`;
- `web.sendFailures`, `web.sequenceGaps`, and `web.coalescedEvents`;
- active faults and Modbus/KNX error counters.

A hardware timing probe or firmware benchmark build must record maximum and
percentile scheduler gaps. Diagnostic HTTP sampling alone cannot prove the 2 ms
Modbus and 10 ms relay scheduling requirements.

Record free heap before TLS connection, after each TLS handshake, at two steady
connections, after forced reconnect, and five minutes after all clients close.
Connection RAM is the difference from the matching zero-client baseline.

## Procedure

1. Build the React assets, firmware, and LittleFS image from a clean checkout.
2. Record toolchain revisions, Git commit, firmware size, partition utilization,
   and generated asset manifest hashes.
3. Flash firmware and LittleFS, factory reset, perform BOOT-authorized
   `provision-web`, trust the generated device certificate on test clients, and
   confirm unauthenticated requests fail.
4. Warm the device for five minutes with Modbus and KNX traffic active. Capture
   the zero-client baseline.
5. Run the workload with zero, one, and two authenticated browser clients for
   15 minutes each. Each client keeps its WebSocket alive and requests relay,
   network, and diagnostics snapshots every five seconds.
6. During each stage, command all six relays at no more than five commands per
   second, run Wi-Fi scans, force one station reconnect, and maintain maximum
   representative Modbus and KNX traffic.
7. At two clients, repeatedly close and reconnect one client for 100 TLS and
   WebSocket cycles. Verify session, origin, CSRF, one-client-per-session, and
   connection limits remain enforced.
8. Saturate the eight-entry web request queue using non-safety mutations. Verify
   excess work receives an explicit bounded error, relay and fieldbus work
   continues, and queued operations retain session ownership.
9. Force one WebSocket journal overflow without retrying mutations. Verify
   `resync.required` and authoritative REST convergence.
10. Continue the representative two-client workload for at least 12 hours, then
    close both clients and capture the five-minute recovery measurement.
11. Power-cycle once and perform a controlled remote restart once. Verify relay
    restore policy, security persistence, and boot/session reconciliation.

## Evidence Record

Store completed records under `doc/hil/` using a filename containing date,
board serial suffix, and Git commit. Do not commit private keys, passwords,
cookies, CSRF tokens, Wi-Fi passphrases, or full device serial numbers.

| Field | Result |
|---|---|
| Date / operator | Not run |
| Git commit / build environment | Not run |
| Board revision / anonymized serial suffix | Not run |
| Firmware flash usage | Not run |
| LittleFS usage | Not run |
| UI JS / CSS gzip sizes | Not run |
| Minimum free heap, 0 / 1 / 2 clients | Not run |
| Largest block, 0 / 1 / 2 clients | Not run |
| TLS connection RAM, client 1 / client 2 | Not run |
| Relay scheduler maximum / p99 gap | Not run |
| Modbus scheduler maximum / p99 gap | Not run |
| Snapshot latency p50 / p95 / p99 | Not run |
| Command completion latency p50 / p95 / p99 | Not run |
| Queue high-water / rejected operations | Not run |
| WebSocket failures / gaps / recovery result | Not run |
| Watchdog / reset result | Not run |
| 12-hour soak result | Not run |
| Final decision and approver | Not run |

## Failure Policy

Any missed safety/relay transition, watchdog reset, credential disclosure,
cross-session result access, unexplained fieldbus error, heap threshold breach,
or failure to converge after a sequence gap blocks production release. Do not
raise capacities or disable checks to obtain a pass. Reduce web concurrency or
replace the transport only through an approved architecture decision with a
repeat of this gate.
