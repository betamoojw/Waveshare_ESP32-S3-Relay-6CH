"""Generate a validated, unique factory identity document."""

from __future__ import annotations

import argparse
import uuid
from datetime import date
from pathlib import Path
from typing import Optional, Sequence

try:
    from .factory_common import DeviceIdentity, atomic_write_json, identity_document, validate_identity
except ImportError:
    from factory_common import DeviceIdentity, atomic_write_json, identity_document, validate_identity


def generate_device_identity(
    serial_number: str,
    hardware_revision: str,
    manufacturing_batch: int,
    manufacturing_date: Optional[str] = None,
    device_uuid: Optional[str] = None,
) -> DeviceIdentity:
    identity = DeviceIdentity(
        serial_number=serial_number,
        device_uuid=(device_uuid or str(uuid.uuid4())).lower(),
        hardware_revision=hardware_revision,
        manufacturing_date=manufacturing_date or date.today().isoformat(),
        manufacturing_batch=manufacturing_batch,
    )
    validate_identity(identity)
    return identity


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--hardware-revision", required=True)
    parser.add_argument("--batch", required=True, type=int)
    parser.add_argument("--manufacturing-date")
    parser.add_argument("--uuid", help="station-assigned UUID; generated securely when omitted")
    parser.add_argument("--output", required=True, type=Path)
    options = parser.parse_args(arguments)
    identity = generate_device_identity(
        options.serial,
        options.hardware_revision,
        options.batch,
        options.manufacturing_date,
        options.uuid,
    )
    atomic_write_json(options.output, identity_document(identity))
    print(f"generated identity for {identity.serial_number}: {options.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())