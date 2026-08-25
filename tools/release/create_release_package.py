"""Create a complete, signed Switch Actuator release package."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import re
import shutil
import tempfile
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, Optional, Sequence, Tuple

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding, rsa
from cryptography.hazmat.primitives.asymmetric.utils import Prehashed


SEMANTIC_VERSION = re.compile(
    r"^v?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)"
    r"(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?"
    r"(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$"
)
IDENTIFIER = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")
FLASH_ARTIFACTS = ("firmware.bin", "bootloader.bin", "partitions.bin", "filesystem.bin")
SIGNATURE_ALGORITHM = "RSASSA-PSS-SHA256"
PRIVATE_KEY_MARKERS = (
    b"-----BEGIN PRIVATE KEY-----",
    b"-----BEGIN RSA PRIVATE KEY-----",
    b"-----BEGIN EC PRIVATE KEY-----",
)
COMPATIBILITY_VERSIONS = {
    "configuration": "CFG-4",
    "api": "API-v1",
    "modbus": "MODBUS-v1",
    "knx_application": "KNX-APP-v1",
    "filesystem": "FS-v1",
}


@dataclass(frozen=True)
class ReleaseInputs:
    firmware: Path
    bootloader: Path
    partitions: Path
    filesystem: Path
    signing_key: Path
    release_notes: Path
    output: Path
    product: str
    hardware: str
    version: str
    minimum_version: str
    git_commit: str
    generated_at: str


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def artifact_metadata(path: Path) -> Dict[str, object]:
    return {"sha256": sha256_file(path), "size": path.stat().st_size}


def write_json(path: Path, document: object) -> None:
    path.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def normalize_version(value: str, field: str) -> str:
    if not SEMANTIC_VERSION.fullmatch(value):
        raise ValueError(f"{field} must be a semantic version")
    return value[1:] if value.startswith("v") else value


def version_order(value: str) -> Tuple[int, int, int, Tuple[Tuple[int, int, object], ...]]:
    match = SEMANTIC_VERSION.fullmatch(value)
    if match is None:
        raise ValueError("version must be semantic")
    prerelease = match.group(4)
    if prerelease is None:
        prerelease_order = ((1, 0, 0),)
    else:
        prerelease_order = tuple(
            (0, 0, int(identifier)) if identifier.isdigit() else (0, 1, identifier)
            for identifier in prerelease.split(".")
        )
    return int(match.group(1)), int(match.group(2)), int(match.group(3)), prerelease_order


def validate_inputs(inputs: ReleaseInputs) -> None:
    for label, path in (
        ("firmware", inputs.firmware),
        ("bootloader", inputs.bootloader),
        ("partitions", inputs.partitions),
        ("filesystem", inputs.filesystem),
        ("signing key", inputs.signing_key),
        ("release notes", inputs.release_notes),
    ):
        if not path.is_file():
            raise FileNotFoundError(f"{label} not found: {path}")
    if not inputs.firmware.name.endswith("-signed.bin"):
        raise ValueError("firmware input must be the Secure Boot v2 *-signed.bin artifact")
    for field, value in (("product", inputs.product), ("hardware", inputs.hardware)):
        if not IDENTIFIER.fullmatch(value):
            raise ValueError(f"{field} must be a filename-safe identifier")
    if not inputs.hardware.startswith("HW-"):
        raise ValueError("hardware must use the HW-* compatibility format")
    version = normalize_version(inputs.version, "version")
    minimum_version = normalize_version(inputs.minimum_version, "minimum_version")
    if version_order(minimum_version) > version_order(version):
        raise ValueError("minimum_version must not exceed version")
    output = inputs.output.resolve()
    for path in (inputs.firmware, inputs.bootloader, inputs.partitions, inputs.filesystem,
                 inputs.signing_key, inputs.release_notes):
        resolved = path.resolve()
        if output == resolved or output in resolved.parents:
            raise ValueError("release output must not contain an input file")
    for label, path in (
        ("firmware", inputs.firmware),
        ("bootloader", inputs.bootloader),
        ("partition table", inputs.partitions),
        ("filesystem", inputs.filesystem),
    ):
        content = path.read_bytes()
        if any(marker in content for marker in PRIVATE_KEY_MARKERS):
            raise ValueError(f"{label} contains prohibited private key material")


def sign_firmware(firmware: Path, signing_key: Path) -> Tuple[bytes, str]:
    private_key = serialization.load_pem_private_key(signing_key.read_bytes(), password=None)
    if not isinstance(private_key, rsa.RSAPrivateKey) or private_key.key_size < 3072:
        raise ValueError("release signing key must be an unencrypted RSA key of at least 3072 bits")
    firmware_digest = bytes.fromhex(sha256_file(firmware))
    signature = private_key.sign(
        firmware_digest,
        padding.PSS(mgf=padding.MGF1(hashes.SHA256()), salt_length=hashes.SHA256().digest_size),
        Prehashed(hashes.SHA256()),
    )
    public_key_der = private_key.public_key().public_bytes(
        encoding=serialization.Encoding.DER,
        format=serialization.PublicFormat.SubjectPublicKeyInfo,
    )
    return signature, hashlib.sha256(public_key_der).hexdigest()


def create_sbom(inputs: ReleaseInputs, artifacts: Dict[str, Dict[str, object]]) -> Dict[str, object]:
    namespace_version = inputs.version.replace("+", "-")
    files = []
    relationships = []
    for index, name in enumerate(FLASH_ARTIFACTS, start=1):
        file_id = f"SPDXRef-File-{index}"
        files.append({
            "SPDXID": file_id,
            "fileName": name,
            "checksums": [{"algorithm": "SHA256", "checksumValue": artifacts[name]["sha256"]}],
        })
        relationships.append({
            "spdxElementId": "SPDXRef-Package",
            "relationshipType": "CONTAINS",
            "relatedSpdxElement": file_id,
        })
    package_verification_code = hashlib.sha1(
        "".join(sorted(str(artifacts[name]["sha256"]) for name in FLASH_ARTIFACTS)).encode("ascii")
    ).hexdigest()
    return {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": f"{inputs.product}-{inputs.version}",
        "documentNamespace": (
            f"https://switch-actuator.invalid/spdx/{inputs.product}/{namespace_version}/{inputs.git_commit}"
        ),
        "creationInfo": {
            "created": inputs.generated_at,
            "creators": ["Tool: switch-actuator-release-packager-1.0"],
        },
        "packages": [{
            "name": inputs.product,
            "SPDXID": "SPDXRef-Package",
            "versionInfo": inputs.version,
            "downloadLocation": "NOASSERTION",
            "filesAnalyzed": True,
            "packageVerificationCode": {"packageVerificationCodeValue": package_verification_code},
            "licenseConcluded": "NOASSERTION",
            "licenseDeclared": "NOASSERTION",
            "copyrightText": "NOASSERTION",
        }],
        "files": files,
        "relationships": relationships,
    }


def create_release_package(inputs: ReleaseInputs, force: bool = False) -> Path:
    validate_inputs(inputs)
    version = normalize_version(inputs.version, "version")
    minimum_version = normalize_version(inputs.minimum_version, "minimum_version")
    output = inputs.output.resolve()
    if output.exists() and not force:
        raise FileExistsError(f"release output already exists: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=f".{output.name}-", dir=output.parent))
    try:
        source_artifacts = {
            "firmware.bin": inputs.firmware,
            "bootloader.bin": inputs.bootloader,
            "partitions.bin": inputs.partitions,
            "filesystem.bin": inputs.filesystem,
        }
        for name, source in source_artifacts.items():
            shutil.copyfile(source, temporary / name)
        shutil.copyfile(inputs.release_notes, temporary / "release-notes.md")

        signature, signing_key_id = sign_firmware(temporary / "firmware.bin", inputs.signing_key)
        (temporary / "firmware.sig").write_bytes(signature)
        artifacts = {name: artifact_metadata(temporary / name) for name in FLASH_ARTIFACTS}
        signature_metadata = artifact_metadata(temporary / "firmware.sig")
        compatibility = {
            "hardware": inputs.hardware,
            "firmware": f"FW-{version}",
            **COMPATIBILITY_VERSIONS,
        }

        manifest = {
            "schema_version": 1,
            "product": inputs.product,
            "hardware": inputs.hardware,
            "version": version,
            "minimum_version": minimum_version,
            "compatibility": compatibility,
            "sha256": artifacts["firmware.bin"]["sha256"],
            "signature": base64.b64encode(signature).decode("ascii"),
            "signature_algorithm": SIGNATURE_ALGORITHM,
            "signature_file": "firmware.sig",
            "signing_key_id": signing_key_id,
            "artifacts": artifacts,
        }
        write_json(temporary / "manifest.json", manifest)
        write_json(temporary / "version.json", {
            "schema_version": 1,
            "product": inputs.product,
            "hardware": inputs.hardware,
            "version": version,
            "minimum_version": minimum_version,
            "compatibility": compatibility,
            "build_environment": "production",
            "git_commit": inputs.git_commit,
            "generated_at": inputs.generated_at,
        })
        write_json(temporary / "SBOM", create_sbom(inputs, artifacts))

        checksum_names = sorted(path.name for path in temporary.iterdir() if path.name != "SHA256SUMS")
        checksum_lines = [f"{sha256_file(temporary / name)}  {name}" for name in checksum_names]
        (temporary / "SHA256SUMS").write_text("\n".join(checksum_lines) + "\n", encoding="ascii")

        if output.exists():
            shutil.rmtree(output)
        os.replace(temporary, output)
    except BaseException:
        shutil.rmtree(temporary, ignore_errors=True)
        raise
    return output


def timestamp_from_epoch(epoch: Optional[str]) -> str:
    if epoch is None:
        return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")
    return datetime.fromtimestamp(int(epoch), timezone.utc).isoformat().replace("+00:00", "Z")


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware", required=True, type=Path)
    parser.add_argument("--bootloader", required=True, type=Path)
    parser.add_argument("--partitions", required=True, type=Path)
    parser.add_argument("--filesystem", required=True, type=Path)
    parser.add_argument("--signing-key", required=True, type=Path)
    parser.add_argument("--release-notes", type=Path, default=Path("CHANGELOG.md"))
    parser.add_argument("--output", type=Path, default=Path("release"))
    parser.add_argument("--product", default="relay-6ch")
    parser.add_argument("--hardware", default="HW-A01")
    parser.add_argument("--version", required=True)
    parser.add_argument("--minimum-version")
    parser.add_argument("--git-commit", default=os.getenv("GITHUB_SHA", "unknown"))
    parser.add_argument("--source-date-epoch", default=os.getenv("SOURCE_DATE_EPOCH"))
    parser.add_argument("--force", action="store_true")
    options = parser.parse_args(arguments)
    minimum_version = options.minimum_version or options.version
    inputs = ReleaseInputs(
        firmware=options.firmware.resolve(),
        bootloader=options.bootloader.resolve(),
        partitions=options.partitions.resolve(),
        filesystem=options.filesystem.resolve(),
        signing_key=options.signing_key.resolve(),
        release_notes=options.release_notes.resolve(),
        output=options.output.resolve(),
        product=options.product,
        hardware=options.hardware,
        version=options.version,
        minimum_version=minimum_version,
        git_commit=options.git_commit,
        generated_at=timestamp_from_epoch(options.source_date_epoch),
    )
    output = create_release_package(inputs, force=options.force)
    print(f"Release package created: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())