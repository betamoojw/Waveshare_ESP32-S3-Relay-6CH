# Production Provisioning

The provisioning tools require Python 3.9 or newer, PlatformIO, and `pyserial`:

```text
python -m pip install pyserial
```

Run the complete flow from the repository root with a blank device connected:

```text
python tools/provisioning/provision.py --port COM5 --serial SA2608240001 --hardware-revision REV-1 --batch 42 --firmware .pio/build/production/firmware_production_v1.0.0-signed.bin
```

The tool uses the `production` environment. A production station must first provision
the approved Secure Boot v2 bootloader, secure-boot digest, and flash-encryption eFuses,
then flash the generated `*-signed.bin` application with the audited station tooling.
Pass that exact `*-signed.bin` artifact to this tool after it is installed. The helper
never uploads an application image; it hashes the supplied artifact for the manifest,
generates the identity, writes and uploads the factory filesystem, pauses for
the BOOT-button authorization gesture, provisions device-generated web security
material, verifies the restarted device, and locks its manifest. Set
`PROVISIONING_ADMIN_PASSWORD` to avoid the interactive password prompt in an
isolated production fixture. Do not place that variable in source control or
command-line arguments.

`verify_device.py` can recheck a connected unit without changing it:

```text
python tools/provisioning/verify_device.py --manifest production-manifests/SA2608240001.json --port COM5
```

The firmware exposes explicit `development`, `engineering`, and `production`
profiles. Production serial mutations are permitted only during physically
authorized factory provisioning and become locked after security provisioning.
The application-level provisioning lock complements, but does not replace, the
ESP32-S3 hardware controls. See [Security architecture](../../docs/architecture/security.md).