from __future__ import annotations

import hashlib
import json
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from export_device_record import build_device_record
from flash_firmware import verify_secure_boot_images
from factory_common import (DeviceIdentity, ReleasePackage, atomic_write_json,
                            parse_security_report, validate_release_package)
from generate_credentials import generate_credentials
from generate_device_id import generate_device_identity
from verify_device import verify_device_evidence


class FactoryToolingTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.identity = generate_device_identity(
            "SA2608240001", "HW-A01", 42, "2026-08-24",
            "12345678-1234-4abc-8def-1234567890ab",
        )

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def create_release(self) -> ReleasePackage:
        artifacts = {
            "bootloader.bin": b"bootloader",
            "firmware.bin": b"signed firmware",
            "filesystem.bin": b"filesystem",
        }
        partition = b"".join((
            struct.pack("<HBBII16sI", 0x50AA, 0x01, 0x02, 0x9000, 0x5000,
                        b"nvs\0\0\0\0\0\0\0\0\0\0\0\0\0", 1),
            struct.pack("<HBBII16sI", 0x50AA, 0x01, 0x82, 0x310000, 0x0F0000,
                        b"spiffs\0\0\0\0\0\0\0\0\0\0", 1),
            struct.pack("<HBBII16sI", 0x50AA, 0x01, 0x03, 0x400000, 0x10000,
                        b"coredump\0\0\0\0\0\0\0\0", 1),
        ))
        artifacts["partitions.bin"] = partition
        metadata = {}
        for name, content in artifacts.items():
            (self.root / name).write_bytes(content)
            metadata[name] = {"sha256": hashlib.sha256(content).hexdigest(), "size": len(content)}
        firmware_digest = metadata["firmware.bin"]["sha256"]
        (self.root / "manifest.json").write_text(json.dumps({
            "schema_version": 1,
            "product": "relay-6ch",
            "hardware": "HW-A01",
            "sha256": firmware_digest,
            "compatibility": {"firmware": "FW-1.4.0", "configuration": "CFG-4"},
            "artifacts": metadata,
        }), encoding="utf-8")
        return validate_release_package(self.root, "HW-A01")

    def test_validates_release_and_derives_filesystem_offset(self) -> None:
        package = self.create_release()
        self.assertEqual(0x310000, package.filesystem_offset)
        self.assertEqual("FW-1.4.0", package.firmware_version)
        self.assertEqual(4, package.configuration_schema_version)

    def test_generates_complex_credentials(self) -> None:
        credentials = generate_credentials(length=24)
        password = credentials["password"]
        self.assertEqual(24, len(password))
        self.assertTrue(any(character.islower() for character in password))
        self.assertTrue(any(character.isupper() for character in password))
        self.assertTrue(any(character.isdigit() for character in password))

    def test_rejects_unencrypted_production_partition(self) -> None:
        package = self.create_release()
        partition_path = package.directory / "partitions.bin"
        content = bytearray(partition_path.read_bytes())
        content[28:32] = (0).to_bytes(4, "little")
        partition_path.write_bytes(content)
        manifest_path = package.directory / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["artifacts"]["partitions.bin"] = {
            "sha256": hashlib.sha256(content).hexdigest(),
            "size": len(content),
        }
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")

        with self.assertRaisesRegex(ValueError, "nvs must be encrypted"):
            validate_release_package(package.directory, "HW-A01")

    def test_verifies_both_secure_boot_images_with_external_trust_key(self) -> None:
        package = self.create_release()
        with tempfile.TemporaryDirectory() as trust_directory:
            public_key = Path(trust_directory) / "production-public.pem"
            public_key.write_text("public key fixture", encoding="ascii")
            with patch("flash_firmware.tool_command", return_value=("espsecure",)), \
                    patch("flash_firmware.run_tool") as run_tool:
                verify_secure_boot_images(package, public_key)

        self.assertEqual(2, run_tool.call_count)
        verified_images = {Path(call.args[0][-1]).name for call in run_tool.call_args_list}
        self.assertEqual({"bootloader.bin", "firmware.bin"}, verified_images)

    def test_parses_enabled_security_controls(self) -> None:
        state = parse_security_report({
            "efuses": {
                "SECURE_BOOT_EN": {"value": 1},
                "SPI_BOOT_CRYPT_CNT": {"value": "0x7"},
            },
        })
        self.assertEqual("enabled", state["secure_boot_state"])
        self.assertEqual("enabled", state["flash_encryption_state"])

    def test_verifies_device_and_exports_secret_free_record(self) -> None:
        package = self.create_release()
        service = {
            "ok": True,
            "device_serial": self.identity.serial_number,
            "device_uuid": self.identity.device_uuid,
            "hardware_revision": self.identity.hardware_revision,
            "firmware": package.firmware_version,
            "certificate_sha256": "ab" * 32,
            "knx_individual_address": "1.1.42",
        }
        snapshot = {
            "ok": "true",
            "interface": "manufacturing",
            "version": "1",
            "profile": "production",
            "configuration_locked": "true",
            "serial": self.identity.serial_number,
            "uuid": self.identity.device_uuid,
            "hardware_revision": self.identity.hardware_revision,
            "manufacturing_date": self.identity.manufacturing_date,
            "manufacturing_batch": str(self.identity.manufacturing_batch),
            "relay_initialized": "true",
            "relays": "[0,0,0,0,0,0]",
        }
        security = {"secure_boot_state": "enabled", "flash_encryption_state": "enabled"}
        evidence = verify_device_evidence(self.identity, package, service, snapshot, security)
        record = build_device_record(
            self.identity, package, evidence, "STATION-01", "OPERATOR-01", "2026-08-24T12:00:00Z")
        self.assertEqual("SA2608240001", record["serial_number"])
        self.assertEqual("enabled", record["secure_boot_state"])
        self.assertEqual("ab" * 32, record["device_certificate_sha256"])
        self.assertEqual("1.1.42", record["knx_individual_address"])
        self.assertNotIn("password", json.dumps(record).lower())
        self.assertNotIn("private_key", json.dumps(record).lower())

    def test_rejects_secret_fields_in_production_output(self) -> None:
        with self.assertRaisesRegex(ValueError, "secret-bearing field"):
            atomic_write_json(self.root / "record.json", {"private_key": "forbidden"})


if __name__ == "__main__":
    unittest.main()