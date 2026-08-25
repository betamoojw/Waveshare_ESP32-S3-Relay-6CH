"""Export a failure-atomic, secret-free production device record."""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Any, Dict, Mapping, Optional, Sequence

try:
    from .factory_common import (RECORD_SCHEMA_VERSION, DeviceIdentity, ReleasePackage,
                                 atomic_write_json, load_identity, load_json, utc_timestamp,
                                 validate_release_package)
except ImportError:
    from factory_common import (RECORD_SCHEMA_VERSION, DeviceIdentity, ReleasePackage,
                                atomic_write_json, load_identity, load_json, utc_timestamp,
                                validate_release_package)


def build_device_record(
    identity: DeviceIdentity,
    package: ReleasePackage,
    evidence: Mapping[str, Any],
    station_id: str,
    operator_id: str,
    production_timestamp: Optional[str] = None,
) -> Dict[str, Any]:
    if not station_id or not operator_id:
        raise ValueError("station and operator identifiers are required")
    return {
        "record_schema_version": RECORD_SCHEMA_VERSION,
        "serial_number": identity.serial_number,
        "device_uuid": identity.device_uuid,
        "firmware_version": package.firmware_version,
        "firmware_sha256": package.firmware_sha256,
        "hardware_revision": identity.hardware_revision,
        "secure_boot_state": evidence["secure_boot_state"],
        "flash_encryption_state": evidence["flash_encryption_state"],
        "device_certificate_sha256": evidence["certificate_sha256"],
        "knx_individual_address": evidence["knx_individual_address"],
        "configuration_schema_version": package.configuration_schema_version,
        "manufacturing_date": identity.manufacturing_date,
        "manufacturing_batch": identity.manufacturing_batch,
        "production_timestamp": production_timestamp or utc_timestamp(),
        "station_id": station_id,
        "operator_id": operator_id,
        "verification_result": "passed",
    }


def export_device_record(path: Path, record: Mapping[str, Any]) -> None:
    if path.exists():
        raise FileExistsError(f"production record already exists: {path}")
    atomic_write_json(path, record)


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--identity", required=True, type=Path)
    parser.add_argument("--release", required=True, type=Path)
    parser.add_argument("--evidence", required=True, type=Path)
    parser.add_argument("--station-id", required=True)
    parser.add_argument("--operator-id", required=True)
    parser.add_argument("--output", required=True, type=Path)
    options = parser.parse_args(arguments)
    identity = load_identity(options.identity)
    package = validate_release_package(options.release, identity.hardware_revision)
    record = build_device_record(
        identity, package, load_json(options.evidence), options.station_id, options.operator_id)
    export_device_record(options.output, record)
    print(f"exported production record for {identity.serial_number}: {options.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())