"""Run the controlled production manufacturing and provisioning workflow."""

from __future__ import annotations

import argparse
import re
import time
from pathlib import Path
from typing import Any, Dict, Optional, Sequence

try:
    from .export_device_record import build_device_record, export_device_record
    from .factory_common import (DEFAULT_BAUD, atomic_write_json, load_json, parse_security_report,
                                 read_security_state, serial_command, validate_release_package)
    from .flash_firmware import flash_release, verify_secure_boot_images
    from .generate_device_id import generate_device_identity
    from .verify_device import verify_device_evidence
except ImportError:
    from export_device_record import build_device_record, export_device_record
    from factory_common import (DEFAULT_BAUD, atomic_write_json, load_json, parse_security_report,
                                read_security_state, serial_command, validate_release_package)
    from flash_firmware import flash_release, verify_secure_boot_images
    from generate_device_id import generate_device_identity
    from verify_device import verify_device_evidence


def load_credentials(path: Path) -> Dict[str, str]:
    document = load_json(path)
    username = document.get("username")
    password = document.get("password")
    if not isinstance(username, str) or not username or len(username) > 31 or any(c.isspace() for c in username):
        raise ValueError("credential file has an invalid username")
    if not isinstance(password, str) or not 12 <= len(password) <= 128 or any(c.isspace() for c in password):
        raise ValueError("credential file has an invalid password")
    return {"username": username, "password": password}


def wait_for_snapshot(port: str, baud: int, timeout: float) -> Dict[str, Any]:
    deadline = time.monotonic() + timeout
    last_error: Optional[Exception] = None
    while time.monotonic() < deadline:
        try:
            return serial_command(port, "mfg-test snapshot", baud, min(5.0, timeout))
        except (OSError, RuntimeError, TimeoutError) as error:
            last_error = error
    raise TimeoutError("device did not return after credential provisioning") from last_error


def confirm_physical_step(message: str, unattended: bool) -> None:
    if unattended:
        return
    input(message + " Press Enter to continue: ")


def validate_knx_individual_address(value: str) -> str:
    match = re.fullmatch(r"([0-9]|1[0-5])\.([0-9]|1[0-5])\.([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])", value)
    if match is None or value == "0.0.0":
        raise ValueError("KNX individual address must be a nonzero area.line.device address")
    return value


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release", required=True, type=Path)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--hardware-revision", required=True)
    parser.add_argument("--batch", required=True, type=int)
    parser.add_argument("--manufacturing-date")
    parser.add_argument("--uuid", help="station-assigned UUID; generated securely when omitted")
    parser.add_argument("--knx-individual-address", required=True,
                        help="factory-assigned KNX area.line.device address")
    parser.add_argument("--credentials", required=True, type=Path,
                        help="protected file produced by generate_credentials.py")
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("--flash-baud", type=int, default=921600)
    parser.add_argument("--esptool", default="esptool")
    parser.add_argument("--espsecure", default="espsecure")
    parser.add_argument("--espefuse", default="espefuse")
    parser.add_argument("--secure-boot-public-key", required=True, type=Path,
                        help="approved public key from the station trust store")
    parser.add_argument("--security-report", type=Path,
                        help="audited station eFuse JSON instead of a live read")
    parser.add_argument("--firmware-already-flashed", action="store_true",
                        help="skip flashing only when the station has already installed this exact release")
    parser.add_argument("--station-id", required=True)
    parser.add_argument("--operator-id", required=True)
    parser.add_argument("--record-dir", type=Path, default=Path("production-records"))
    parser.add_argument("--work-dir", type=Path, default=Path("factory-work"))
    parser.add_argument("--restart-timeout", type=float, default=45.0)
    parser.add_argument("--unattended", action="store_true",
                        help="fixture controls physical BOOT and secure-station handoffs")
    options = parser.parse_args(arguments)

    identity = generate_device_identity(
        options.serial,
        options.hardware_revision,
        options.batch,
        options.manufacturing_date,
        options.uuid,
    )
    knx_individual_address = validate_knx_individual_address(options.knx_individual_address)
    package = validate_release_package(options.release, identity.hardware_revision)
    credentials = load_credentials(options.credentials)
    record_path = options.record_dir / f"{identity.serial_number}.json"
    if record_path.exists():
        raise FileExistsError(f"production record already exists: {record_path}")

    work_path = options.work_dir / identity.serial_number
    identity_path = work_path / "identity.json"
    if identity_path.exists():
        raise FileExistsError(f"factory work already exists for serial: {identity.serial_number}")
    atomic_write_json(identity_path, {
        "serial_number": identity.serial_number,
        "device_uuid": identity.device_uuid,
        "hardware_revision": identity.hardware_revision,
        "manufacturing_date": identity.manufacturing_date,
        "manufacturing_batch": identity.manufacturing_batch,
    })

    if not options.firmware_already_flashed:
        flash_release(package, options.port, options.secure_boot_public_key,
                      options.flash_baud, options.esptool, options.espsecure)
    else:
        verify_secure_boot_images(package, options.secure_boot_public_key, options.espsecure)

    confirm_physical_step(
        "Complete the approved Secure Boot v2 and flash-encryption eFuse procedure, then verify it at the station.",
        options.unattended,
    )
    security_state = (parse_security_report(load_json(options.security_report))
                      if options.security_report else read_security_state(options.port, options.espefuse))
    if security_state != {"secure_boot_state": "enabled", "flash_encryption_state": "enabled"}:
        raise ValueError("production security controls are not both enabled")

    confirm_physical_step(
        "Boot the production firmware and enter service mode by holding BOOT for 3-10 seconds.",
        options.unattended,
    )
    serial_command(
        options.port,
        "service provision-identity "
        f"{identity.serial_number} {identity.device_uuid} {identity.manufacturing_date} "
        f"{identity.manufacturing_batch}",
        options.baud,
    )
    serial_command(options.port, f"set-knx individual-address {knx_individual_address}", options.baud)
    service_identity = serial_command(options.port, "service identity", options.baud)
    provision_result = serial_command(
        options.port,
        f"provision-web {credentials['username']} {credentials['password']}",
        options.baud,
    )
    credentials = {}

    certificate_sha256 = provision_result.get("certificate_sha256")
    if not isinstance(certificate_sha256, str) or re.fullmatch(r"[0-9a-f]{64}", certificate_sha256) is None:
        raise ValueError("device did not return a valid certificate fingerprint")
    service_identity["certificate_sha256"] = certificate_sha256

    snapshot = wait_for_snapshot(options.port, options.baud, options.restart_timeout)
    evidence = verify_device_evidence(identity, package, service_identity, snapshot, security_state)
    evidence_path = work_path / "verification.json"
    atomic_write_json(evidence_path, evidence)
    record = build_device_record(
        identity, package, evidence, options.station_id, options.operator_id)
    export_device_record(record_path, record)
    print(f"provisioned and verified {identity.serial_number}")
    print(f"production record: {record_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())