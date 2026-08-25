# Production Provisioning

## Scope

Provisioning converts approved hardware and signed release artifacts into a unique, locked production device. Application tooling complements the audited ESP32-S3 secure-boot and flash-encryption station; it does not replace irreversible eFuse procedures.

## Inputs

- approved `HW-*` hardware revision;
- complete production release package and exact signed application artifact;
- unique serial number and UUID;
- manufacturing date and nonzero batch;
- protected administrator credential input;
- approved Secure Boot public verification key from the station trust store;
- assigned KNX individual address;
- approved Secure Boot v2 and flash-encryption station material.

Private signing/encryption keys and administrator passwords must not be committed, logged, placed in command-line history, or stored in device configuration files.

## Station Sequence

1. Verify board model/revision and keep relay loads isolated.
2. Install the approved signed bootloader, partition table, and signed application using audited station tooling.
3. Provision and verify secure-boot and flash-encryption eFuses in the approved order.
4. Generate the per-device production manifest and unique identity.
5. Write the factory LittleFS configuration.
6. Enter [Service mode](service-mode.md) with the physical BOOT gesture.
7. Provision identity, KNX individual address, and initial web security/user state. The device generates its TLS private key internally and returns only the certificate SHA-256 fingerprint.
8. Run the serial manufacturing test sequence and external contact, RS-485, RF, current, and BOOT-mode checks.
9. Verify the restarted identity, firmware compatibility set, security state, and safe relay state.
10. Lock and archive the production manifest by serial number.

The canonical station entry point is `tools/factory/provision.py`. It validates
the complete release package, flashes its approved artifacts, derives the
filesystem offset from the packaged partition table, and verifies read-only
eFuse evidence. It never burns eFuses or receives private signing/encryption
keys.

Example:

```text
python tools/factory/provision.py --release release --port COM5 --serial SA2608240001 --hardware-revision HW-A01 --batch 42 --knx-individual-address 1.1.42 --credentials C:/protected/station/SA2608240001-credentials.json --secure-boot-public-key C:/protected/station/production-secure-boot-public.pem --station-id STATION-01 --operator-id OPERATOR-01
```

Use `PROVISIONING_ADMIN_PASSWORD` only in an isolated protected fixture environment to avoid interactive input. Never pass the password as a command-line argument.

## Verification

Recheck a locked unit without mutation:

```text
python tools/factory/verify_device.py --identity factory-work/SA2608240001/identity.json --release release --port COM5
```

The normative fixture commands and acceptance criteria remain in `design/manufacturing-test-interface.md`. Hardware security prerequisites are defined in [Security architecture](../architecture/security.md), and released artifact requirements are defined in [Release process](../release/release-process.md).
