"""Run the failure-atomic Switch Actuator production provisioning flow."""

from __future__ import annotations

import argparse
import getpass
import json
import os
import subprocess
import time
from pathlib import Path
from typing import Optional, Sequence

try:
    from .generate_identity import generate_identity
    from .production_manifest import (ProductionManifest, ProvisioningState, lock_manifest,
                                      save_manifest, sha256_file)
    from .verify_device import request_snapshot, verify_snapshot
except ImportError:
    from generate_identity import generate_identity
    from production_manifest import (ProductionManifest, ProvisioningState, lock_manifest,
                                     save_manifest, sha256_file)
    from verify_device import request_snapshot, verify_snapshot


FILESYSTEM_ENVIRONMENT = "engineering"


def _run(command: Sequence[str], repository: Path) -> None:
    subprocess.run(command, cwd=repository, check=True)


def _write_factory_identity(system_path: Path, manifest: ProductionManifest) -> None:
    document = json.loads(system_path.read_text(encoding="utf-8"))
    identity = document["identity"]
    identity["productId"] = manifest.identity.product_id
    identity["hardwareRevision"] = manifest.identity.hardware_revision
    identity["deviceSerial"] = manifest.identity.serial_number
    identity["deviceUuid"] = manifest.identity.device_uuid
    identity["manufacturingDate"] = manifest.identity.manufacturing_date
    identity["manufacturingBatch"] = manifest.identity.manufacturing_batch
    temporary = system_path.with_suffix(".json.provisioning")
    temporary.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    os.replace(temporary, system_path)


def _serial_command(port: str, baud: int, command: str, timeout: float = 30.0) -> str:
    try:
        import serial
    except ImportError as error:
        raise RuntimeError("provisioning requires pyserial: python -m pip install pyserial") from error
    deadline = time.monotonic() + timeout
    with serial.Serial(port, baudrate=baud, timeout=0.25, write_timeout=1) as connection:
        connection.reset_input_buffer()
        connection.write(command.encode("utf-8") + b"\r\n")
        while time.monotonic() < deadline:
            line = connection.readline().decode("utf-8", errors="replace").strip()
            if line.startswith("ok="):
                if not line.startswith("ok=true"):
                    raise RuntimeError(f"device rejected provisioning command: {line}")
                return line
    raise TimeoutError("timed out waiting for provisioning response")


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--hardware-revision", required=True)
    parser.add_argument("--batch", required=True, type=int)
    parser.add_argument("--product-id", default="SA-6CH-S3")
    parser.add_argument("--manufacturing-date")
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--admin-user", default="admin")
    parser.add_argument("--password-env", default="PROVISIONING_ADMIN_PASSWORD")
    parser.add_argument("--manifest-dir", type=Path, default=Path("production-manifests"))
    parser.add_argument("--repository", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--pio", default="platformio")
    parser.add_argument("--firmware", required=True, type=Path,
                        help="preinstalled production *-signed.bin application")
    options = parser.parse_args(arguments)

    password = os.environ.get(options.password_env) or getpass.getpass("Initial administrator password: ")
    if any(character.isspace() for character in password) or not 12 <= len(password) <= 128:
        raise ValueError("administrator password must contain 12-128 non-whitespace characters")
    manifest_path = options.manifest_dir / f"{options.serial}.json"
    if manifest_path.exists():
        raise FileExistsError(f"manifest already exists: {manifest_path}")

    repository = options.repository.resolve()
    firmware = options.firmware.resolve()
    if not firmware.name.endswith("-signed.bin"):
        raise ValueError("production firmware must be an explicit *-signed.bin artifact")
    if not firmware.is_file():
        raise FileNotFoundError(f"production firmware image not found: {firmware}")
    identity = generate_identity(options.serial, options.product_id, options.hardware_revision,
                                 options.batch, options.manufacturing_date)
    manifest = ProductionManifest(identity=identity, firmware_sha256=sha256_file(firmware))
    save_manifest(manifest_path, manifest)
    manifest.advance(ProvisioningState.IDENTITY_GENERATED)
    save_manifest(manifest_path, manifest)

    system_path = repository / "data" / "config" / "system.json"
    backup = system_path.read_bytes()
    try:
        _write_factory_identity(system_path, manifest)
        _run([options.pio, "run", "-e", FILESYSTEM_ENVIRONMENT, "-t", "uploadfs", "--upload-port", options.port], repository)
    finally:
        system_path.write_bytes(backup)
    manifest.advance(ProvisioningState.FACTORY_CONFIGURATION_WRITTEN)
    save_manifest(manifest_path, manifest)

    print("Hold BOOT for at least 3 seconds, release before 10 seconds, then press Enter.")
    input()
    _serial_command(options.port, options.baud, f"provision-web {options.admin_user} {password}")
    password = ""
    manifest.advance(ProvisioningState.SECURITY_PROVISIONED)
    save_manifest(manifest_path, manifest)

    time.sleep(2.0)
    fields = request_snapshot(options.port, options.baud, 30.0)
    verify_snapshot(fields, manifest, require_locked=True)
    manifest.advance(ProvisioningState.VERIFIED)
    save_manifest(manifest_path, manifest)
    lock_manifest(manifest_path, manifest)
    print(f"provisioned and locked {identity.serial_number}; manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())