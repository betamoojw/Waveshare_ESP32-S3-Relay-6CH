# Security Architecture

## Purpose

Security controls span release, manufacturing, local service, network access,
credential storage, and reset. No single authenticated interface bypasses relay
safety, configuration validation, secure boot, or another lifecycle boundary.

## Trust Boundaries

| Boundary | Authority | Explicit limits |
|---|---|---|
| Ordinary network user | Authenticated HTTPS session and assigned permissions | Cannot enter service mode, factory reset remotely, access secrets, or bypass relay policy |
| Local service operator | Physical BOOT gesture plus expiring serial service session | Cannot bypass production identity lock, secure boot, flash encryption, or unsupported recovery policy |
| Manufacturing station | Audited physical fixture and unlocked device lifecycle | Must use signed artifacts, unique identity, protected credentials, and recorded verification |
| Release system | Protected CI environment and signing keys | Produces immutable signed packages; does not provision device eFuses or user credentials |

## Runtime Access

The web interface requires HTTPS, authenticated sessions, bounded role
permissions, exact Origin/Host checks, CSRF protection, rate limits, and fixed
request limits. Password verifiers, token-signing keys, certificates, and
private keys remain in the protected web-security store and are never returned
by diagnostics or configuration APIs.

Service mode is serial-only, requires physical presence, expires automatically,
and has no HTTP, WebSocket, KNX, Modbus, or Wi-Fi entry operation. See
[Service mode](../manufacturing/service-mode.md).

Remote factory reset is prohibited. Physical factory reset removes users and
field configuration while preserving manufacturing and factory security
identity. See [Factory reset](../manufacturing/factory-reset.md).

## Build profiles

PlatformIO exposes exactly three firmware profiles:

| Environment | Purpose | Optimization | Logging and debug access |
|---|---|---|---|
| `development` | Local development and diagnosis | `-Og`, debug symbols | Verbose logging; debug interfaces allowed |
| `engineering` | Release-like hardware validation | `-Og`, debug symbols | Warning/debug logging; debug interfaces allowed |
| `production` | Released field firmware | `-O2`, `NDEBUG` | Errors only; debug interfaces disabled |

`development` is the default environment. CI names every environment explicitly, so adding or reordering environments cannot silently alter release behavior.

## Production inputs

The production build aborts before compilation unless all release inputs are present:

```text
FIRMWARE_VERSION=<immutable release version>
PRODUCTION_SIGNING_KEY=<external Secure Boot v2 PEM key path>
PRODUCTION_SIGNING_KEY_DIGEST=<approved 64-hex Secure Boot v2 public-key digest>
SECURE_BOOT_PROVISIONING_APPROVED=1
FLASH_ENCRYPTION_PROVISIONING_APPROVED=1
```

The approvals mean that the release is paired with the audited manufacturing workflow. They do not burn eFuses or prove a particular device state. The private key must live in a restricted CI secret store, signing service, or HSM-backed release station; it must never be committed or copied into a firmware image. The build derives the Secure Boot v2 public-key digest from the supplied key and rejects it unless it exactly matches the separately approved digest. Development keys therefore cannot silently sign production firmware.

The build scans the embedded recovery configuration and every LittleFS JSON
source and rejects nonempty password, PSK, token, secret, or private-key fields.
Release packaging also rejects PEM private-key material in every flashable
artifact. Device identity and administrator credentials remain per-device
provisioning data.

## Signing and artifacts

The production post-build action signs the application with Espressif `espsecure` using Secure Boot v2. The unsigned compiler output remains an intermediate. Only this application artifact may enter a release package:

```text
.pio/build/production/firmware_production_<version>-signed.bin
```

Do not release or flash `.pio/build/production/firmware_production_<version>.bin`. The tagged-release workflow renames the signed application to `firmware.bin` inside the complete package and never archives the unsigned intermediate.

The post-build hook verifies the newly signed application before reporting it.
CI independently verifies both the approved bootloader and application. Factory
flashing requires the approved public key from the station trust store and
verifies both images again before writing flash; a public key supplied inside
the release package is not a trust anchor.

The tagged workflow combines the signed application with the approved bootloader, partition table, filesystem, detached OTA signature, checksums, version metadata, SPDX SBOM, and release notes. The complete package contract is defined in [Release process](../release/release-process.md).

## Irreversible device provisioning

Secure boot and flash encryption are hardware lifecycle controls, not compiler flags. An audited station must follow Espressif's ESP32-S3 procedure to:

1. Protect and authorize access to production signing and encryption keys.
2. Install a Secure Boot v2-compatible signed bootloader and partition table.
3. Flash the signed application at the application partition offset.
4. Provision and verify flash-encryption and secure-boot eFuses in the approved order.
5. Read back and record eFuse security state without exporting private key material.
6. Run device identity, relay-safe-state, update, and recovery verification before locking the manufacturing manifest.

Never automate irreversible eFuse burning in a general developer build script. The production firmware independently checks `esp_secure_boot_enabled()` and `esp_flash_encryption_enabled()` after relay outputs enter their safe state; it refuses normal initialization if either control is absent.

The repository provisioning helper handles application-level identity, filesystem configuration, web credentials, and verification. Pass it the exact `*-signed.bin` artifact after the audited station has installed that application. The helper never uploads application firmware and is not the authority for bootloader signing, flash encryption, or eFuse programming.

## Flash Encryption Modes

Development and engineering profiles leave flash encryption disabled so units
remain recoverable with development tools. They must never contain production
credentials. The production profile requires flash encryption at runtime and
uses `partitions/production_8MB.csv`, which marks NVS, LittleFS, and coredump
partitions encrypted while preserving the established OTA layout.

The approved production bootloader and station procedure own first encryption,
eFuse key generation/provisioning, and disabling unauthorized plaintext access.
The station must verify encryption before application provisioning. NVS then
protects device identity, Wi-Fi profiles, administrator verifiers, JWT signing
material, the TLS certificate/private key, and mutable configuration at rest.
LittleFS is encrypted because it can later hold explicitly stored Wi-Fi
configuration. Coredumps are encrypted because they may contain transient
credentials in memory.

Factory calibration and hardware identity held in eFuses remain eFuse-owned and
are not copied into JSON. Device-specific application metadata is written only
after encryption is verified. A production record may contain public identity,
certificate fingerprints, versions, and security state, but never encryption
keys, signing keys, TLS private keys, passwords, verifiers, or session material.

## Key Lifecycle

- Development signing keys are disposable and are never accepted by the
	production digest pin.
- Production Secure Boot private keys remain in protected CI/signing
	infrastructure. Factory stations receive only approved public verification
	keys and key digests.
- OTA/release metadata uses a separate RSA signing key and key identifier; it
	is not the Secure Boot key.
- Rotation is a controlled release and manufacturing operation. A replacement
	digest may be enrolled only through the ESP32-S3 supported secure-boot key
	slot and revocation procedure, while firmware is still signed by a currently
	trusted key. Revocation is irreversible and requires dual approval plus an
	archived device/batch association.
- If no trusted key slot remains, or all trusted keys are revoked, recovery is
	board replacement or an explicitly approved hardware lifecycle procedure;
	unsigned recovery is prohibited.

Recovery firmware, when implemented, must be signed by a still-trusted Secure
Boot key, match hardware and anti-rollback policy, and preserve the same
encrypted-data and physical-authorization boundaries. The current running
firmware correctly exposes no recovery command because no separately verified
recovery image exists.

## Factory Credential Model

The physically authorized factory flow provisions serial number, UUID,
manufacturing metadata, KNX individual address, and the initial Web
administrator. The device generates its P-256 TLS private key, self-signed
certificate, password salt/verifier, and JWT signing key internally from the
hardware RNG and persists them only in encrypted NVS. The private key is never
returned over serial.

Successful provisioning returns only the certificate's DER SHA-256 fingerprint.
That fingerprint and the KNX address are verified and archived in the
secret-free production record. Factory reset preserves the TLS/signing identity
while removing users and field configuration, as defined by the reset contract.