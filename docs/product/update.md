# Product Update

## Current Product Status

The release pipeline produces signed, self-describing update artifacts, but running firmware does not currently expose a remote firmware-update operation or a service-mode recovery boot target. Web capabilities report firmware update unavailable, and `service firmware-recovery` returns `operation-unavailable`.

This limitation is deliberate: the current product must not claim safe OTA or recovery until partition selection, trust, rollback, and health confirmation are implemented end to end.

## Update Package

Every production release contains the signed application, approved bootloader, partition table, LittleFS image, detached signature, checksums, compatibility metadata, SBOM, and release notes. See [Release process](../release/release-process.md).

Before installation, tooling must verify:

- release manifest schema and product ID;
- hardware and filesystem compatibility;
- firmware minimum-version and anti-rollback policy;
- artifact sizes and SHA-256 digests;
- detached release signature;
- Secure Boot v2 application signature.

## Required Runtime Update Flow

A future firmware updater must:

1. fetch and validate signed release metadata;
2. reject incompatible hardware, configuration, API/protocol, or filesystem requirements;
3. write only to an inactive OTA partition while hashing the stream;
4. verify digest and signature before selecting the image;
5. preserve the previous bootable image;
6. boot the candidate in a pending state;
7. confirm only after lifecycle, storage, network, watchdog, and relay-safety health checks pass;
8. roll back automatically when confirmation fails.

Relay outputs must remain fail-safe throughout update, reboot, rollback, and recovery.

## Filesystem Updates

An ordinary application update does not replace LittleFS. A filesystem image is a separate release artifact governed by `FS-*` compatibility and must be installed only when the release manifest and migration policy explicitly require it.

## Recovery

Recovery must use an independently verified signed image and physical authorization. Unsigned serial/network flashing from running production firmware is prohibited. Factory reset is not firmware recovery and does not alter installed images or OTA metadata.
