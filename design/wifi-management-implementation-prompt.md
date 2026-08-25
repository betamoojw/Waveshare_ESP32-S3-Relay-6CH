# Production Wi-Fi Management Implementation Prompt

Use this prompt when implementing or reviewing the remaining production Wi-Fi management work in this repository.

---

You are a senior embedded, security, API, and frontend engineer working in the `Waveshare_ESP32-S3-Relay-6CH` repository. Implement production-grade Wi-Fi management for the ESP32-S3 relay actuator. Follow `AGENTS.md`, `docs/architecture/system.md`, `docs/architecture/network.md`, `design/fontend-plan.md`, and `design/fontend-instructions.md`. Use C++17 only.

## Objective

Deliver one coherent, secure workflow that manages all Wi-Fi needs:

- bring up a protected recovery access point for first provisioning and fallback;
- scan asynchronously for nearby networks;
- create, update, enable/disable, delete, and reorder up to three station profiles;
- connect a selected enabled profile immediately without rebooting relay control;
- support DHCP and validated static IPv4 per profile;
- atomically persist configuration with generation conflict protection;
- preserve omitted secrets, replace explicitly supplied secrets, and clear only by explicit request;
- expose redacted authoritative state to the React dashboard;
- keep relay, Modbus RTU, KNX, watchdog, and local controls responsive during every network operation.

## Mandatory Architecture

1. `ConfigurationService` owns validation, staging, generation assignment, atomic persistence, and activation.
2. `WifiManagementService` owns redacted snapshots and typed profile/recovery-AP mutations. HTTP, CLI, and Improv must use this application boundary rather than editing domain configuration independently.
3. `NetworkManager` exclusively owns `WiFi`, station/AP modes, scan, connection attempts, retry/backoff, active profile, IP application, and controlled reconfiguration after a successful commit.
4. HTTP handlers only parse bounded DTOs, authorize, call application services, map typed results, and serialize redacted responses. They must not perform NVS writes or radio loops directly.
5. Frontend state is a cache or unsaved draft. Firmware state and generation are authoritative.

## Domain and Persistence Rules

- Keep exactly three fixed-capacity ordered profile slots; do not introduce unbounded containers.
- Validate SSID and WPA2 passphrase lengths, duplicate enabled SSIDs, channel `1..13`, and bounded recovery-AP prefix.
- Static IPv4 requires nonzero address, contiguous nonzero subnet mask, nonzero gateway and DNS, and gateway in the configured subnet. Reject before persistence.
- Every mutation carries the expected active configuration generation. Return conflict without staging or saving when stale.
- On save failure, discard staged state and keep the active configuration and current radio operation unchanged.
- Profile deletion compacts priority order and clears the final slot, including its secret.
- Reordering preserves the complete profile value, including its secret, but never exposes that secret.
- An omitted passphrase preserves the stored value. A supplied passphrase replaces it. `clearPassphrase=true` clears it. Never use empty or missing values ambiguously.
- NVS remains authoritative. LittleFS configuration is deployment defaults only.

## Radio Behavior

- Scan with the ESP32 asynchronous API; permit one scan at a time, retain at most 16 bounded results, and publish state plus sequence.
- Attempt enabled profiles in priority order with a 20-second association/IP timeout per profile.
- A connect-now request starts the selected enabled profile and reports progress through authoritative status; it does not claim success synchronously.
- Continue bounded station retries while the recovery AP is active.
- Use wrap-safe elapsed-time comparisons and capped backoff. Add randomized jitter before production release.
- A recovery AP must use a device-unique SSID suffix and per-device WPA2 passphrase. Open AP mode and repository-wide passwords are prohibited.
- `remainActiveWhileOffline=true` keeps the AP available until infrastructure succeeds. Otherwise, a nonzero `timeoutMs` closes it after the commissioning window while station retries continue.
- Applying committed Wi-Fi configuration must not reboot, block the scheduler, reset relays, or bypass lifecycle/diagnostic reporting.

## HTTPS API

Implement only after the approved `PsychicHttpsServer` security foundation is present. Register routes only when the complete capability is operational.

- `GET /api/v1/network/wifi`: redacted generation, active profile, profile summaries, recovery-AP policy/state, and scan state/results.
- `POST /api/v1/network/wifi/scan`: start or return the one active asynchronous scan.
- `PUT /api/v1/network/wifi/profiles/{index}`: generation-safe profile update with optional write-only passphrase and explicit clear.
- `DELETE /api/v1/network/wifi/profiles/{index}`: generation-safe delete and compaction.
- `POST /api/v1/network/wifi/profiles/{index}/move`: generation-safe priority move.
- `POST /api/v1/network/wifi/profiles/{index}/connect`: begin connection to one enabled stored profile.
- `PUT /api/v1/network/wifi/recovery-ap`: generation-safe recovery policy update.

All mutation routes require Administrator permission, hardened-cookie authentication, exact Origin/Host validation, CSRF, bounded JSON, no-store responses, and rate limits. Map invalid index to `404`, validation to `422`, stale generation to `409`, scan/operation conflict to `409`, persistence failure to `503`, and unavailable lifecycle/radio to `503`. Never serialize passphrases, JWTs, keys, or password-derived values.

The recovery AP exposes only authenticated provisioning and minimal non-sensitive health. Relay control, normal configuration, users, detailed diagnostics, KNX/IP, and OTA remain unavailable there unless a later security ADR explicitly permits them.

## Frontend

- Render active profile, priority order, enabled state, SSID, secret-present flag, DHCP/static settings, and recovery-AP status.
- Provide scan/select, save, explicit clear-secret, move up/down, connect now, delete confirmation, and recovery policy controls.
- Use generation from the last authoritative response in each mutation. On `409`, retain the draft, show a conflict message, refetch, and require deliberate retry.
- Poll only while scan/connection state is changing; stop polling when stable.
- Never place credentials in logs, URLs, query keys, browser storage, diagnostics, or returned API models.
- Authorize controls from permission strings, not role names. Hide or disable unsupported capabilities.
- Preserve the existing quiet operational visual language and responsive layout.

## Verification

Add native fakes and tests for secret preserve/replace/clear, stale generation, invalid index, duplicate SSID, static IPv4 validation, delete compaction, reorder, recovery policy, persistence failure atomicity, and generation wrap policy. Add frontend tests for redaction, conflict handling, ordering, delete, recovery policy, and no optimistic connection success.

Run and report:

```text
platformio test -e native
platformio run -e development
platformio run -e development -t buildfs
cd web && npm run lint && npm test && npm run build
```

If the host compiler required by PlatformIO native is unavailable, report that as a blocker; do not claim native tests passed. Hardware acceptance must exercise AP provisioning, all-profile failure, reconnect storms, connect-now, DHCP/static networking, `millis()` wrap, concurrent relay/Modbus/KNX load, credential non-disclosure, and recovery after persistence/radio failures.

Do not expose unfinished routes or capability flags, do not add blocking radio waits, do not weaken physical-only factory reset, and do not claim production completion until HTTPS/security and hardware acceptance gates pass.
