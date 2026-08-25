# Release Versioning

## Firmware Version

Production firmware uses SemVer and reports the canonical `FW-MAJOR.MINOR.PATCH` label.

- `MAJOR`: incompatible product behavior or support-policy change;
- `MINOR`: backward-compatible product capability;
- `PATCH`: backward-compatible fix.

Pre-release and build metadata follow SemVer. Development and main-branch images include `+development` or `+main` and must not be treated as immutable releases.

Release tags such as `v1.4.0` are normalized and compiled into the image as `FW-1.4.0`. Production builds reject non-SemVer version inputs.

## Independent Compatibility Versions

Firmware SemVer does not replace hardware, configuration, API, Modbus, KNX application, or filesystem compatibility versions. Each surface changes independently according to [Compatibility](../product/compatibility.md).

A firmware release that changes another compatibility boundary must update that boundary in the same change and provide migration and release notes. A firmware-only fix must not silently bump protocol or storage contracts.

## Release Checklist

1. Select an immutable SemVer tag.
2. Confirm all seven compatibility labels against the intended hardware and artifacts.
3. Update `CHANGELOG.md` with operator-visible behavior and migration requirements.
4. Run protocol, configuration migration, release-package, web, and hardware release gates applicable to the changed surfaces.
5. Build the named production environment with protected approvals and keys.
6. Verify Secure Boot v2 signatures on bootloader and application.
7. Generate the complete package and verify every checksum/signature.
8. Inspect `manifest.json` and `version.json`; both must contain the same compatibility object.
9. Publish the package as one immutable release and retain build provenance.

## Rollback and Minimum Version

The release manifest declares `minimum_version`. It must not exceed the release version. Future OTA logic must combine this value with trusted anti-rollback state and must never allow unsigned metadata to weaken rollback policy.

The complete artifact and CI process is defined in [Release process](release-process.md).
