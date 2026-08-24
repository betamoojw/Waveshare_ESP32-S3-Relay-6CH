# Web Adapter

PsychicHttp 3.1.2 and Arduino-ESP32 3.1.3 both define globally named
`LoggingMiddleware`, `AuthenticationMiddleware`, and `CorsMiddleware` symbols.
KNX requires Arduino's built-in `WebServer`, so `extra_script.py` excludes only
PsychicHttp's unused `PsychicMiddlewares.cpp`. This adapter implements JWT,
CSRF, exact-origin, and security-header middleware locally; do not remove that
filter or use PsychicHttp's Basic Authentication/CORS helpers.

Optional HTTP and live-update adapter code belongs in this directory.

The adapter must not expose a generic LittleFS editor, directory listing, or
arbitrary filesystem path. Future static assets are restricted to an explicit
allowlist under `/www/`; `/config/`, `/config/.backup/`, and NVS credentials are
never web-readable. Essential pages should retain a compiled
fallback when filesystem assets are unavailable. See
`design/filesystem-architecture.md`.

## Management API contract

The React client uses same-origin `/api/v1` endpoints. Implementations must keep
HTTP callbacks bounded and enqueue mutations for application-loop processing.
Network callbacks must use `NetworkManager`; relay callbacks must use
`SwitchingPolicyService` and may never write GPIO directly.

| Endpoint | Permission | Behavior |
|---|---|---|
| `POST /session` | Public, rate-limited | Verify a password and issue hardened session and CSRF cookies |
| `GET /session` | Authenticated | Return the redacted current session |
| `DELETE /session` | Authenticated mutation | Revoke the current in-memory session |
| `GET /network/wifi` | `configuration:read` | Redacted profiles, recovery AP state, and bounded scan snapshot |
| `POST /network/wifi/scan` | `configuration:write` | Start one asynchronous scan; never block for scan completion |
| `PUT /network/wifi/profiles/{index}` | `configuration:write` | Stage and persist one of three profiles through `ConfigurationService` |
| `DELETE /network/wifi/profiles/{index}` | `configuration:write` | Remove and compact a profile using expected generation |
| `POST /network/wifi/profiles/{index}/move` | `configuration:write` | Reorder profiles using expected generation |
| `POST /network/wifi/profiles/{index}/connect` | `configuration:write` | Begin an asynchronous connection attempt through `NetworkManager` |
| `PUT /network/wifi/recovery-ap` | `configuration:write` | Persist and apply recovery-AP policy using expected generation |
| `GET /protocols/modbus` | `configuration:read` | Return active runtime role, generation, unit ID, supported baud, parity, fixed data bits, and stop bits |
| `PUT /protocols/modbus` | `configuration:write` | Queue a generation-safe validated RTU server configuration commit and controlled restart |
| `PUT /protocols/modbus/role` | `configuration:write` | Queue an immediate runtime switch between RTU server and client roles |
| `GET /protocols/knx` | `configuration:read` | Return generation, KNX/IP timing, device objects, and six channel bindings |
| `PUT /protocols/knx` | `configuration:write` | Queue a generation-safe validated KNX/IP configuration commit and controlled restart |
| `GET /diagnostics` | `diagnostics:read` | Return bounded runtime, fault, protocol, heap, queue, and WebSocket pressure state |
| `GET /ws` | `relay:read` | Open one versioned live channel per authenticated session |
| `GET /users` | `users:manage` | List username, role, and enabled state only |
| `POST /users` | `users:manage` | Create a bounded administrator or guest profile |
| `PUT /users/{id}` | `users:manage` | Change role, enabled state, or password verifier |
| `POST /maintenance/restart` | `configuration:write` | Queue a lifecycle-controlled restart with a bounded response-drain window |

Passphrases, password verifiers, JWT signing material, and private keys are never
returned by any endpoint or stored under LittleFS `/config/`.

## Authentication requirements

JWT support is not considered secure over plain HTTP. A production adapter must:

- serve HTTPS or remain restricted to an explicitly trusted management network;
- place short-lived JWTs in `HttpOnly`, `Secure`, `SameSite=Strict` cookies;
- require an independent CSRF token and exact `Origin` validation for mutations;
- store salted password verifiers and a hardware-random signing key in a separate
	protected NVS namespace;
- enforce administrator and guest permissions in middleware and again at the
	owning application service;
- rate-limit authentication failures and support signing-generation revocation;
- never place bearer tokens in URLs, JavaScript storage, logs, or WebSocket payloads.

The existing `web.securityProvisioned` flag must remain false until the protected
credential store contains a valid administrator and signing key.

Initial credentials and TLS identity are created only by the BOOT-authorized
serial `provision-web` command documented in `../cli/README.md`. There is no
unauthenticated remote bootstrap route.

## Unavailable OTA

OTA routes are intentionally unregistered and capabilities advertise
`firmwareUpdate: false`. The React client does not query or render update
workflows while that capability is false. OTA may be advertised only after all
of these checks are implemented:

The Waveshare build currently emits `otadata`, `app0`, and `app1`; each app slot
is `0x330000` bytes.

- product identifier, chip family, build environment (`PIOENV`), image length,
	secure version, and cryptographic signature are valid;
- remote metadata and image downloads use HTTPS certificate validation and an
	approved host allowlist;
- the image is written incrementally to the inactive OTA partition;
- activation occurs only after complete-image verification;
- the next boot confirms application health before cancelling rollback;
- update transfer does not change relay output state, and restart follows the
	configured relay restore policy.

Remote factory reset remains prohibited. Factory reset requires the physical
BOOT-button gesture defined by the application layer.

Production release also requires the no-PSRAM HTTPS/WebSocket hardware evidence
defined in `design/web-hardware-load-gate.md`. A successful firmware build does
not substitute for that gate.