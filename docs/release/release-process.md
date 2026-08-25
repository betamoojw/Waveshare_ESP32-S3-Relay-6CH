# Release Artifact Management

## Purpose

Every tagged production build creates one complete, self-describing release directory. Manufacturing, service tooling, and the future OTA client consume this package instead of locating unrelated PlatformIO outputs.

```text
release/
|-- firmware.bin
|-- bootloader.bin
|-- partitions.bin
|-- filesystem.bin
|-- manifest.json
|-- SHA256SUMS
|-- firmware.sig
|-- version.json
|-- SBOM
`-- release-notes.md
```

The package generator is `tools/release/create_release_package.py`. It validates every input, writes into a temporary directory, and replaces `release/` only after all files are complete. Existing output requires the explicit `--force` option.

## Artifact contract

| File | Contract |
|---|---|
| `firmware.bin` | Secure Boot v2-signed application image; never the unsigned compiler intermediate |
| `bootloader.bin` | Manufacturing-approved Secure Boot v2 bootloader supplied by the protected release environment |
| `partitions.bin` | Partition table emitted by the matching production build |
| `filesystem.bin` | LittleFS image emitted by the matching production `buildfs` target |
| `manifest.json` | Machine-readable product, compatibility, digest, signature, and artifact metadata |
| `SHA256SUMS` | SHA-256 for every other file in the release directory |
| `firmware.sig` | Detached RSA-PSS/SHA-256 signature over the exact `firmware.bin` bytes |
| `version.json` | Build identity, normalized semantic version, all compatibility versions, minimum version, commit, and timestamp |
| `SBOM` | SPDX 2.3 JSON document describing the four flashable release components |
| `release-notes.md` | Release-specific Markdown copied from `CHANGELOG.md` |

`manifest.json` uses schema version 1:

```json
{
  "schema_version": 1,
  "product": "relay-6ch",
  "hardware": "HW-A01",
  "version": "1.0.0",
  "minimum_version": "1.0.0",
  "compatibility": {
    "hardware": "HW-A01",
    "firmware": "FW-1.0.0",
    "configuration": "CFG-4",
    "api": "API-v1",
    "modbus": "MODBUS-v1",
    "knx_application": "KNX-APP-v1",
    "filesystem": "FS-v1"
  },
  "sha256": "<firmware SHA-256>",
  "signature": "<base64 RSA-PSS signature>",
  "signature_algorithm": "RSASSA-PSS-SHA256",
  "signature_file": "firmware.sig",
  "signing_key_id": "<SHA-256 of the OTA public key>",
  "artifacts": {
    "firmware.bin": {"sha256": "<SHA-256>", "size": 123456}
  }
}
```

The top-level digest and signature are the future OTA application contract. `compatibility` records the seven independent compatibility surfaces used by manufacturing, service, and upgrade policy. `signing_key_id` identifies the trusted public key without embedding it and supports controlled key rotation. The artifact map supports manufacturing and recovery validation without hard-coded filenames or sizes.

## Protected release inputs

The GitHub `production` environment must provide:

| Input | Purpose |
|---|---|
| `PRODUCTION_SIGNING_KEY_PEM_BASE64` secret | Secure Boot v2 application signing key |
| `RELEASE_SIGNING_KEY_PEM_BASE64` secret | Separate unencrypted RSA key, at least 3072 bits, for detached OTA signatures |
| `PRODUCTION_BOOTLOADER_BIN_BASE64` secret | Approved signed bootloader image |
| `MINIMUM_FIRMWARE_VERSION` variable | Oldest version allowed by compatibility policy; defaults to the release version |
| `RELEASE_HARDWARE_REVISION` variable | Target hardware compatibility identifier; defaults to `HW-A01` |

The secure-boot and OTA keys are intentionally separate. CI decodes them only into the runner's temporary directory, restricts key permissions, and never archives a private key. Production-environment approval protects access to these inputs.

## Tagged release flow

1. Build the `production` application and produce the Secure Boot v2-signed image.
2. Build the production LittleFS image.
3. Verify the Secure Boot v2 signatures on the application and approved bootloader.
4. Combine the signed application, approved bootloader, generated partitions, and filesystem.
5. Generate the manifest metadata and detached firmware signature.
6. Generate SPDX metadata and checksums.
7. Upload the complete directory as the `release-package` workflow artifact.
8. Attach every package file to the GitHub release.

The workflow fails if an input is absent, a Secure Boot signature is invalid, a version is not semantic, the firmware filename does not identify a signed image, the signing key is not RSA 3072 bits or stronger, or any source artifact cannot be read.

## Future OTA integration

An OTA implementation should fetch `manifest.json` first and apply this order:

1. Require the supported manifest schema, product, and hardware revision.
2. Parse semantic versions and enforce both device anti-rollback state and `minimum_version` policy.
3. Check partition capacity against the declared firmware size.
4. Download `firmware.bin` into the inactive OTA partition while computing SHA-256.
5. Require the computed digest to match `manifest.sha256`.
6. Verify `firmware.sig` with the embedded OTA public key using RSA-PSS/SHA-256.
7. Let the ESP32 secure-boot chain verify the image before activation.
8. Mark the new image pending and require a healthy boot before confirming it.

TLS protects transport, but digest and signature verification remain mandatory. OTA code must never trust an unsigned manifest to weaken hardware compatibility or anti-rollback policy; those constraints must also be bounded by trusted firmware policy or signed metadata in the final OTA design.

## Local invocation

Install the pinned release dependency and invoke the generator after the production firmware and filesystem builds:

```text
python -m pip install -r tools/release/requirements.txt
python tools/release/create_release_package.py \
  --firmware .pio/build/production/firmware_production_v1.0.0-signed.bin \
  --bootloader C:/protected/bootloader.bin \
  --partitions .pio/build/production/partitions.bin \
  --filesystem .pio/build/production/littlefs.bin \
  --signing-key C:/protected/release-signing-key.pem \
  --version v1.0.0 \
  --minimum-version 1.0.0 \
  --output release
```

Run the focused package tests with:

```text
python -m unittest discover -s tools/release -p "test_*.py" -v
```