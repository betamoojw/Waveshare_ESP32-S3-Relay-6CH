# ADR-0001: WebSocket Live Transport

- Status: Accepted
- Date: 2026-08-21

## Context

The original frontend design selected HTTP mutations plus Server-Sent Events. The production backend now requires one authenticated bidirectional channel for relay command submission, command completion, state changes, network changes, scan progress, configuration generation changes, diagnostics, and OTA progress. The target has no PSRAM, so connection count, frames, queues, and serialization work must remain bounded.

## Decision

Use same-origin secure WebSocket at `/api/v1/ws` as the primary live transport. REST remains authoritative for initial snapshots, configuration forms, uploads, and recovery after sequence gaps. Bounded polling is the fallback when WebSocket is unavailable.

The WebSocket upgrade uses the same hardened session cookie as REST and requires exact `Origin` and `Host` validation. The protocol uses versioned JSON envelopes, `bootId`, monotonic event sequence, request ID, and typed payloads. Commands enter the same application services as REST; WebSocket handlers never write GPIO, NVS, or radio state directly.

Firmware permits at most two authenticated WebSocket clients and one per session. Text frames are limited to 2 KiB. Binary and fragmented frames are rejected. Each client has a bounded outbound queue; replaceable snapshots are coalesced and persistently slow clients are disconnected. Heartbeat and idle expiry are mandatory.

The server starts only as `PsychicHttpsServer` with a valid certificate/private key and provisioned administrator/signing material loaded from protected NVS. It does not downgrade authenticated management to HTTP. Recovery-AP WebSocket upgrades are rejected.

## Consequences

The former `/api/v1/events` SSE route is removed from the production contract. The React client reconnects with capped exponential backoff and jitter, reconciles by `bootId` and sequence, and refreshes REST snapshots after gaps. Non-idempotent mutations are not automatically retried after ambiguous disconnects.

PsychicHttp 3.1.2 and Arduino-ESP32 3.1.3 export conflicting global middleware names. The build excludes PsychicHttp's unused `PsychicMiddlewares.cpp`; this project implements JWT, CSRF, exact-origin, and security headers locally.

Release requires no-PSRAM hardware measurements under two HTTPS/WebSocket clients plus relay, Modbus, KNX, Wi-Fi reconnect, malformed traffic, and OTA load. If heap or scheduler margins fail, authenticated web management remains disabled.
