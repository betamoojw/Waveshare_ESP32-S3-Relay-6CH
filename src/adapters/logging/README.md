# Logging Adapter

The firmware uses `betamoojw/arduino-logger` v1.0.1, installed by PlatformIO as
`Logger`. `LoggerAdapter` is the only application-facing logging API. It keeps
the library's timestamping, serial sink, bounded formatting, runtime threshold,
and optional file capability while presenting the product severities:

```text
LOG_DEBUG
LOG_INFO
LOG_WARNING
LOG_ERROR
LOG_FATAL
```

The file sink is disabled. Logs are transient serial diagnostics and must not be
written to LittleFS unless a future design defines retention, access control,
rotation, and secure erase requirements. `LOG_FATAL` uses the library's error
sink with a `FATAL` marker; it does not restart or halt the device. The owning
application remains responsible for fail-safe lifecycle transitions.

## Build And Runtime Levels

| Profile | Build-time availability | Startup threshold |
|---|---|---|
| Production | FATAL, ERROR, WARNING, INFO | INFO |
| Engineering | FATAL, ERROR, WARNING, INFO, DEBUG | DEBUG |
| Development | FATAL, ERROR, WARNING, INFO, DEBUG | DEBUG |

Production compiles `LOG_DEBUG(...)` to a no-op, so runtime configuration cannot
recover its arguments or enable debug output. `LoggerAdapter::setLevel()` changes
the volatile threshold; production rejects `Debug`. The maintenance-authorized
serial command `set-log-level [debug|info|warning|error|fatal]` exposes this API.
The selected level is intentionally not persisted and returns to the profile
default after restart.

## Prohibited Data

The following values must never appear in logs at any severity, including DEBUG:

- passwords or password verifiers;
- private keys, signing keys, or key material;
- JWTs or JWT claims copied from an untrusted token;
- session tokens, cookies, CSRF tokens, or authorization headers;
- Wi-Fi credentials, including all SSIDs and passphrases;
- OTA credentials, authorization headers, signed download URLs, or private
  update metadata.

Do not log raw HTTP headers or bodies, CLI command buffers, provisioning input,
configuration documents, credential-bearing objects, or binary dumps that may
contain these values. Log fixed event identifiers and explicitly non-secret
facts such as lifecycle state, numeric error code, channel number, bounded
counter, build profile, and protocol availability. Redaction after formatting is
not an acceptable control because unlabeled secret values cannot be detected
reliably.