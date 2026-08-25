from __future__ import annotations

import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding, rsa
from cryptography.hazmat.primitives.asymmetric.utils import Prehashed

from create_release_package import ReleaseInputs, create_release_package


EXPECTED_FILES = {
    "firmware.bin",
    "bootloader.bin",
    "partitions.bin",
    "filesystem.bin",
    "manifest.json",
    "SHA256SUMS",
    "firmware.sig",
    "version.json",
    "SBOM",
    "release-notes.md",
}


class ReleasePackageTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.private_key = rsa.generate_private_key(public_exponent=65537, key_size=3072)
        cls.private_key_pem = cls.private_key.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.PKCS8,
            encryption_algorithm=serialization.NoEncryption(),
        )

    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.inputs_directory = self.root / "inputs"
        self.inputs_directory.mkdir()
        self.firmware = self.write_input("firmware-production-v1.2.3-signed.bin", b"signed-firmware")
        self.bootloader = self.write_input("bootloader.bin", b"bootloader")
        self.partitions = self.write_input("partitions.bin", b"partitions")
        self.filesystem = self.write_input("littlefs.bin", b"filesystem")
        self.signing_key = self.write_input("release-key.pem", self.private_key_pem)
        self.release_notes = self.write_input("CHANGELOG.md", b"# Release notes\n")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_input(self, name: str, content: bytes) -> Path:
        path = self.inputs_directory / name
        path.write_bytes(content)
        return path

    def release_inputs(self, output: Path | None = None, firmware: Path | None = None) -> ReleaseInputs:
        return ReleaseInputs(
            firmware=firmware or self.firmware,
            bootloader=self.bootloader,
            partitions=self.partitions,
            filesystem=self.filesystem,
            signing_key=self.signing_key,
            release_notes=self.release_notes,
            output=output or self.root / "release",
            product="relay-6ch",
            hardware="HW-A01",
            version="v1.2.3",
            minimum_version="1.0.0",
            git_commit="0123456789abcdef",
            generated_at="2026-08-24T00:00:00Z",
        )

    def test_creates_complete_verifiable_package(self) -> None:
        output = create_release_package(self.release_inputs())

        self.assertEqual(EXPECTED_FILES, {path.name for path in output.iterdir()})
        manifest = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
        self.assertEqual("relay-6ch", manifest["product"])
        self.assertEqual("HW-A01", manifest["hardware"])
        self.assertEqual("1.2.3", manifest["version"])
        self.assertEqual("1.0.0", manifest["minimum_version"])
        self.assertEqual({
            "hardware": "HW-A01",
            "firmware": "FW-1.2.3",
            "configuration": "CFG-4",
            "api": "API-v1",
            "modbus": "MODBUS-v1",
            "knx_application": "KNX-APP-v1",
            "filesystem": "FS-v1",
        }, manifest["compatibility"])
        self.assertEqual(hashlib.sha256(b"signed-firmware").hexdigest(), manifest["sha256"])
        self.assertRegex(manifest["signing_key_id"], r"^[0-9a-f]{64}$")

        firmware_digest = bytes.fromhex(manifest["sha256"])
        self.private_key.public_key().verify(
            (output / "firmware.sig").read_bytes(),
            firmware_digest,
            padding.PSS(mgf=padding.MGF1(hashes.SHA256()), salt_length=hashes.SHA256().digest_size),
            Prehashed(hashes.SHA256()),
        )

        checksum_lines = (output / "SHA256SUMS").read_text(encoding="ascii").splitlines()
        self.assertEqual(len(EXPECTED_FILES) - 1, len(checksum_lines))
        for line in checksum_lines:
            expected, name = line.split("  ", maxsplit=1)
            self.assertEqual(expected, hashlib.sha256((output / name).read_bytes()).hexdigest())

        sbom = json.loads((output / "SBOM").read_text(encoding="utf-8"))
        self.assertEqual("SPDX-2.3", sbom["spdxVersion"])
        self.assertEqual(4, len(sbom["files"]))
        self.assertRegex(
            sbom["packages"][0]["packageVerificationCode"]["packageVerificationCodeValue"],
            r"^[0-9a-f]{40}$",
        )
        version = json.loads((output / "version.json").read_text(encoding="utf-8"))
        self.assertEqual("production", version["build_environment"])
        self.assertEqual("0123456789abcdef", version["git_commit"])
        self.assertEqual(manifest["compatibility"], version["compatibility"])

    def test_rejects_unsigned_firmware(self) -> None:
        unsigned = self.write_input("firmware.bin", b"unsigned")

        with self.assertRaisesRegex(ValueError, "Secure Boot v2"):
            create_release_package(self.release_inputs(firmware=unsigned))

    def test_rejects_output_containing_inputs(self) -> None:
        with self.assertRaisesRegex(ValueError, "must not contain"):
            create_release_package(self.release_inputs(output=self.root), force=True)

    def test_rejects_minimum_version_newer_than_release(self) -> None:
        inputs = self.release_inputs()
        incompatible = ReleaseInputs(**{**inputs.__dict__, "minimum_version": "2.0.0"})

        with self.assertRaisesRegex(ValueError, "must not exceed"):
            create_release_package(incompatible)

    def test_rejects_noncanonical_hardware_version(self) -> None:
        inputs = ReleaseInputs(**{**self.release_inputs().__dict__, "hardware": "revA"})

        with self.assertRaisesRegex(ValueError, "HW-\\*"):
            create_release_package(inputs)

    def test_orders_numeric_prerelease_before_alphanumeric_prerelease(self) -> None:
        inputs = self.release_inputs()
        compatible = ReleaseInputs(**{
            **inputs.__dict__,
            "version": "1.2.3-beta",
            "minimum_version": "1.2.3-1",
        })

        output = create_release_package(compatible)
        manifest = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
        self.assertEqual("1.2.3-1", manifest["minimum_version"])


if __name__ == "__main__":
    unittest.main()