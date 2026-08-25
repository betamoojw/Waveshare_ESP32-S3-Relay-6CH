"""Verify production identity, firmware, security state, and safe outputs."""

from __future__ import annotations

import argparse
import re
from pathlib import Path
from typing import Any, Dict, Mapping, Optional, Sequence


_SHA256 = re.compile(r"^[0-9a-f]{64}$")
_KNX_ADDRESS = re.compile(r"^(?:[0-9]|1[0-5])\.(?:[0-9]|1[0-5])\.(?:[0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$")

try:
    from .factory_common import (DEFAULT_BAUD, DeviceIdentity, ReleasePackage, atomic_write_json,
                                 load_identity, load_json, parse_response, parse_security_report,
                                 read_security_state, serial_command, validate_release_package)
except ImportError:
    from factory_common import (DEFAULT_BAUD, DeviceIdentity, ReleasePackage, atomic_write_json,
                                load_identity, load_json, parse_response, parse_security_report,
                                read_security_state, serial_command, validate_release_package)


def _require(fields: Mapping[str, Any], expected: Mapping[str, str], surface: str) -> None:
    mismatches = [
        f"{name}: expected {value}, got {fields.get(name)}"
        for name, value in expected.items()
        if str(fields.get(name)) != value
    ]
    if mismatches:
        raise ValueError(f"{surface} verification failed: " + "; ".join(mismatches))


def verify_device_evidence(
    identity: DeviceIdentity,
    package: ReleasePackage,
    service_identity: Mapping[str, Any],
    manufacturing_snapshot: Mapping[str, Any],
    security_state: Mapping[str, str],
) -> Dict[str, Any]:
    _require(service_identity, {
        "device_serial": identity.serial_number,
        "device_uuid": identity.device_uuid,
        "hardware_revision": identity.hardware_revision,
        "firmware": package.firmware_version,
    }, "service identity")
    _require(manufacturing_snapshot, {
        "interface": "manufacturing",
        "version": "1",
        "profile": "production",
        "configuration_locked": "true",
        "serial": identity.serial_number,
        "uuid": identity.device_uuid,
        "hardware_revision": identity.hardware_revision,
        "manufacturing_date": identity.manufacturing_date,
        "manufacturing_batch": str(identity.manufacturing_batch),
        "relay_initialized": "true",
    }, "manufacturing snapshot")
    if manufacturing_snapshot.get("relays") != "[0,0,0,0,0,0]":
        raise ValueError("manufacturing snapshot verification failed: relay outputs are not safe")
    for field in ("secure_boot_state", "flash_encryption_state"):
        if security_state.get(field) != "enabled":
            raise ValueError(f"production security verification failed: {field} is not enabled")
    certificate_sha256 = service_identity.get("certificate_sha256")
    if not isinstance(certificate_sha256, str) or _SHA256.fullmatch(certificate_sha256) is None:
        raise ValueError("service identity verification failed: certificate fingerprint is unavailable")
    knx_individual_address = service_identity.get("knx_individual_address")
    if (not isinstance(knx_individual_address, str)
            or _KNX_ADDRESS.fullmatch(knx_individual_address) is None
            or knx_individual_address == "0.0.0"):
        raise ValueError("service identity verification failed: KNX individual address is unavailable")
    return {
        "device_identity": dict(service_identity),
        "manufacturing_snapshot": dict(manufacturing_snapshot),
        "secure_boot_state": security_state["secure_boot_state"],
        "flash_encryption_state": security_state["flash_encryption_state"],
        "certificate_sha256": certificate_sha256,
        "knx_individual_address": knx_individual_address,
        "firmware_sha256": package.firmware_sha256,
        "configuration_schema_version": package.configuration_schema_version,
    }


def collect_device_evidence(
    port: str,
    identity: DeviceIdentity,
    package: ReleasePackage,
    baud: int = DEFAULT_BAUD,
    espefuse: str = "espefuse",
    security_report: Optional[Path] = None,
) -> Dict[str, Any]:
    service_identity = serial_command(port, "service identity", baud)
    snapshot = serial_command(port, "mfg-test snapshot", baud)
    security_state = (parse_security_report(load_json(security_report)) if security_report
                      else read_security_state(port, espefuse))
    return verify_device_evidence(identity, package, service_identity, snapshot, security_state)


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--identity", required=True, type=Path)
    parser.add_argument("--release", required=True, type=Path)
    parser.add_argument("--port")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("--espefuse", default="espefuse", help="espefuse executable or espefuse.py path")
    parser.add_argument("--security-report", type=Path, help="audited station eFuse JSON instead of live read")
    parser.add_argument("--identity-response", help="captured service identity response for offline verification")
    parser.add_argument("--snapshot-response", help="captured manufacturing snapshot for offline verification")
    parser.add_argument("--evidence-output", type=Path)
    options = parser.parse_args(arguments)
    identity = load_identity(options.identity)
    package = validate_release_package(options.release, identity.hardware_revision)
    offline = options.identity_response is not None or options.snapshot_response is not None
    if offline:
        if options.identity_response is None or options.snapshot_response is None or options.security_report is None:
            parser.error("offline verification requires both captured responses and --security-report")
        evidence = verify_device_evidence(
            identity,
            package,
            parse_response(options.identity_response),
            parse_response(options.snapshot_response),
            parse_security_report(load_json(options.security_report)),
        )
    else:
        if options.port is None:
            parser.error("live verification requires --port")
        evidence = collect_device_evidence(
            options.port, identity, package, options.baud, options.espefuse, options.security_report)
    if options.evidence_output:
        atomic_write_json(options.evidence_output, evidence)
    print(f"verified production device {identity.serial_number}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())