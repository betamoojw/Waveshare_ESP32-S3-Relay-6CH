# Embedded Web Login Feature Requirements

## 1. Purpose and Status

This document defines the normative authentication and browser-session requirements for the Switch Actuator embedded web interface. It complements `software-architecture-instructions.md` and the web adapter contract. Safety, application ownership, and configuration rules remain authoritative.

The keywords **MUST**, **MUST NOT**, **SHOULD**, **SHOULD NOT**, and **MAY** are requirements.

## 2. Security Objectives

The login feature MUST:

- permit only provisioned, enabled local users to access protected device APIs;
- resist credential guessing, user enumeration, session theft, CSRF, and stale-session use;
- keep passwords, password verifiers, JWTs, signing keys, and private keys out of browser storage, URLs, logs, and API responses;
- fail closed without disabling local relay control when web security is unavailable;
- fit bounded ESP32 memory and execution constraints.

The device is not a cloud identity provider. Password recovery, email login, social login, and remote unauthenticated account creation are out of scope.

## 3. Provisioning and Credentials

1. The first administrator MUST be provisioned through an authenticated local commissioning path. The production web server MUST NOT expose an unauthenticated bootstrap endpoint.
2. Usernames MUST be unique, bounded to the firmware username capacity, and compared without leaking whether an account exists.
3. Passwords MUST be accepted only over HTTPS and MUST be verified against a salted, iterated password verifier stored in protected NVS.
4. Plaintext passwords MUST exist only for the duration of verification and MUST NOT be persisted or logged.
5. Disabled users MUST authenticate indistinguishably from unknown users.
6. Authentication failures MUST return one generic invalid-credentials response. Timing SHOULD be independent of account existence.

## 4. Login API

### 4.1 Create Session

`POST /api/v1/session` MUST accept JSON containing `username` and `password`.

- The request body MUST be bounded before parsing.
- Missing, malformed, empty, or oversized fields MUST be rejected.
- Invalid credentials MUST return `401` with a generic error.
- Rate limiting MUST return `429` and a bounded `Retry-After` value.
- Session-capacity or cryptographic failure MUST fail closed without disclosing account state.
- Success MUST return only the redacted user, permissions, remaining lifetime, and CSRF token.

### 4.2 Inspect and Delete Session

- `GET /api/v1/session` MUST validate the session and return the same redacted view.
- `DELETE /api/v1/session` MUST require the session cookie, exact Origin and Host, and the session CSRF token.
- Logout MUST revoke server-side session state and expire the cookie.

## 5. Session Controls

1. Authentication MUST use a `Secure`, `HttpOnly`, `SameSite=Strict`, host-only cookie with `Path=/`.
2. The browser MUST NOT read or persist the session token.
3. Every mutation MUST include the in-memory CSRF token issued for that session.
4. Sessions MUST have a short access lifetime and a bounded absolute lifetime. Expiry comparisons MUST be wrap-safe.
5. Session identifiers, CSRF tokens, and signing material MUST use the ESP hardware random source.
6. User disablement, password replacement, role changes, security reset, and signing-generation changes MUST revoke affected sessions.
7. Session storage MUST be fixed-capacity. Exhaustion MUST reject new sessions without evicting an active operator silently.

## 6. Browser Experience

1. Protected application routes MUST render only after session inspection succeeds.
2. The login form MUST provide labeled username and password controls, password-manager autocomplete metadata, keyboard submission, pending state, and accessible error announcements.
3. The password MAY be revealed only through an explicit accessible control and MUST default to concealed.
4. Invalid credentials MUST use generic wording. Rate limiting and temporary device unavailability MAY use distinct recovery-oriented wording without revealing account state.
5. A successful login MUST populate the authoritative session query without an avoidable second credential exchange.
6. The browser MUST keep the CSRF token in memory only and clear it on logout, expiry, or authorization failure.
7. When the advertised session lifetime elapses, or any authenticated API request returns `401`, the browser MUST immediately stop live transport, clear protected cached data, and return to login without requiring a reload.
8. Logout MUST return to login even if the network response is lost; the UI MUST NOT continue exposing cached protected data.
9. Development preview mode MAY bypass login only when explicitly compiled for preview and MUST display its existing preview indication.

## 7. Authorization

Authentication does not imply unrestricted access. The server MUST enforce permissions for every REST and WebSocket operation. The browser SHOULD hide or disable unavailable actions for usability, but browser checks MUST NOT be treated as authorization.

## 8. Abuse Resistance and Observability

- Failed logins MUST be rate-limited with fixed bounded state and no dynamic allocation.
- Login responses MUST carry `Cache-Control: no-store` and the standard web security headers.
- Logs and diagnostics MAY report aggregate authentication pressure, but MUST NOT include usernames, passwords, tokens, verifiers, or enough detail to enumerate users.
- Authentication failure MUST NOT block the application loop, relay processing, watchdog servicing, or local control.

## 9. Acceptance Criteria

The feature is acceptable when automated tests demonstrate:

- successful login stores the CSRF token only in memory;
- invalid credentials and rate limiting render the correct non-enumerating messages;
- controls are disabled while a login is pending;
- logout clears session and protected query state;
- an authenticated `401` and local expiry both return the browser to login;
- protected requests send cookies through `same-origin` credentials and mutations send CSRF;
- backend tests cover valid, invalid, disabled, rate-limited, expired, revoked, and capacity-full sessions.

Hardware validation MUST additionally verify HTTPS cookie behavior, repeated failed-login responsiveness, session expiry, logout, and continued relay/CLI/Modbus/KNX operation under authentication load.