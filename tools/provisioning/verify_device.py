"""Verify production identity and lock state over the manufacturing CLI."""

from __future__ import annotations

import argparse
import re
import time
from pathlib import Path
from typing import Dict, Optional, Sequence

try:
    from .production_manifest import DeploymentProfile, ProductionManifest, load_manifest
except ImportError:
    from production_manifest import DeploymentProfile, ProductionManifest, load_manifest


_FIELD = re.compile(r"(?:^|\s)([a-z_]+)=([^\s]+)")


def parse_response(response: str) -> Dict[str, str]:
    fields = dict(_FIELD.findall(response.strip()))
    if fields.get("ok") != "true":
        raise ValueError(f"device command failed: {response.strip()}")
    return fields


def verify_snapshot(fields: Dict[str, str], manifest: ProductionManifest, require_locked: bool = True) -> None:
    expected = {
        "interface": "manufacturing",
        "profile": DeploymentProfile.PRODUCTION.value,
        "serial": manifest.identity.serial_number,
        "uuid": manifest.identity.device_uuid,
        "product_id": manifest.identity.product_id,
        "hardware_revision": manifest.identity.hardware_revision,
        "manufacturing_date": manifest.identity.manufacturing_date,
        "manufacturing_batch": str(manifest.identity.manufacturing_batch),
        "configuration_locked": "true" if require_locked else "false",
    }
    mismatches = [f"{name}: expected {value}, got {fields.get(name)}" for name, value in expected.items()
                  if fields.get(name) != value]
    if mismatches:
        raise ValueError("device verification failed: " + "; ".join(mismatches))
    relays = fields.get("relays", "")
    if relays != "[0,0,0,0,0,0]":
        raise ValueError(f"device verification failed: relays are not safe: {relays}")


def request_snapshot(port: str, baud: int, timeout: float) -> Dict[str, str]:
    try:
        import serial
    except ImportError as error:
        raise RuntimeError("live verification requires pyserial: python -m pip install pyserial") from error
    deadline = time.monotonic() + timeout
    with serial.Serial(port, baudrate=baud, timeout=0.25, write_timeout=1) as connection:
        connection.reset_input_buffer()
        connection.write(b"mfg-test snapshot\r\n")
        while time.monotonic() < deadline:
            line = connection.readline().decode("utf-8", errors="replace").strip()
            if line.startswith("ok="):
                return parse_response(line)
    raise TimeoutError("timed out waiting for manufacturing snapshot")


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--port")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=20.0)
    parser.add_argument("--response", help="verify a captured response instead of opening serial")
    parser.add_argument("--allow-unlocked", action="store_true")
    options = parser.parse_args(arguments)
    if bool(options.port) == bool(options.response):
        parser.error("provide exactly one of --port or --response")
    manifest = load_manifest(options.manifest)
    fields = parse_response(options.response) if options.response else request_snapshot(options.port, options.baud, options.timeout)
    verify_snapshot(fields, manifest, require_locked=not options.allow_unlocked)
    print(f"verified production device {manifest.identity.serial_number}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
