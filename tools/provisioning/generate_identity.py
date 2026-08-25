"""Generate and validate unique production device identity."""

from __future__ import annotations

import argparse
import json
import re
import uuid
from dataclasses import asdict
from datetime import date
from pathlib import Path
from typing import Optional, Sequence

try:
    from .production_manifest import ProductionIdentity
except ImportError:
    from production_manifest import ProductionIdentity


_IDENTIFIER = re.compile(r"^[A-Z0-9][A-Z0-9._-]+$")


def generate_identity(
    serial_number: str,
    product_id: str,
    hardware_revision: str,
    manufacturing_batch: int,
    manufacturing_date: Optional[str] = None,
) -> ProductionIdentity:
    for name, value, maximum_length in (
        ("serial number", serial_number, 31),
        ("product ID", product_id, 23),
        ("hardware revision", hardware_revision, 15),
    ):
        if not _IDENTIFIER.fullmatch(value) or len(value) > maximum_length:
            raise ValueError(f"invalid {name}: use 3-{maximum_length} uppercase identifier characters")
    if manufacturing_batch <= 0 or manufacturing_batch > 0xFFFFFFFF:
        raise ValueError("manufacturing batch must be in the range 1..4294967295")
    manufactured = manufacturing_date or date.today().isoformat()
    if date.fromisoformat(manufactured) > date.today():
        raise ValueError("manufacturing date cannot be in the future")
    return ProductionIdentity(
        serial_number=serial_number,
        device_uuid=str(uuid.uuid4()),
        product_id=product_id,
        hardware_revision=hardware_revision,
        manufacturing_date=manufactured,
        manufacturing_batch=manufacturing_batch,
    )


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--product-id", default="SA-6CH-S3")
    parser.add_argument("--hardware-revision", required=True)
    parser.add_argument("--batch", required=True, type=int)
    parser.add_argument("--manufacturing-date")
    parser.add_argument("--output", type=Path)
    options = parser.parse_args(arguments)
    identity = generate_identity(
        options.serial,
        options.product_id,
        options.hardware_revision,
        options.batch,
        options.manufacturing_date,
    )
    encoded = json.dumps(asdict(identity), indent=2, sort_keys=True)
    if options.output:
        options.output.parent.mkdir(parents=True, exist_ok=True)
        options.output.write_text(encoded + "\n", encoding="utf-8")
    else:
        print(encoded)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
