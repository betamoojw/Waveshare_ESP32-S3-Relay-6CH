"""Shared contracts for audited Switch Actuator factory tooling."""

from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import uuid
from dataclasses import asdict, dataclass
from datetime import date, datetime, timezone
from pathlib import Path
from typing import Any, Dict, Iterable, Mapping, Optional, Sequence, Tuple


RECORD_SCHEMA_VERSION = 1
CONFIGURATION_SCHEMA_VERSION = 4
DEFAULT_BAUD = 115200
FLASH_ARTIFACTS = ("bootloader.bin", "partitions.bin", "firmware.bin", "filesystem.bin")
_IDENTIFIER = re.compile(r"^[A-Z0-9][A-Z0-9._-]{1,62}$")
_FIELD = re.compile(r"(?:^|\s)([a-z_]+)=([^\s]+)")
_SECRET_KEY_PARTS = (
    "password",
    "passwd",
    "privatekey",
    "private_key",
    "secret",
    "token",
    "psk",
    "signing_key",
    "encryption_key",
)


@dataclass(frozen=True)
class DeviceIdentity:
    serial_number: str
    device_uuid: str
    hardware_revision: str
    manufacturing_date: str
    manufacturing_batch: int


@dataclass(frozen=True)
class ReleasePackage:
    directory: Path
    product_id: str
    hardware_revision: str
    firmware_version: str
    firmware_sha256: str
    configuration_schema_version: int
    filesystem_offset: int


def utc_timestamp() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def validate_identity(identity: DeviceIdentity) -> None:
    if not _IDENTIFIER.fullmatch(identity.serial_number) or len(identity.serial_number) > 31:
        raise ValueError("serial number must contain 2-31 uppercase identifier characters")
    try:
        parsed_uuid = uuid.UUID(identity.device_uuid)
    except ValueError as error:
        raise ValueError("device UUID must be a canonical RFC 4122 UUID") from error
    if str(parsed_uuid) != identity.device_uuid.lower() or parsed_uuid.int == 0:
        raise ValueError("device UUID must be canonical and nonzero")
    if not _IDENTIFIER.fullmatch(identity.hardware_revision) or not identity.hardware_revision.startswith("HW-"):
        raise ValueError("hardware revision must use the HW-* identifier format")
    try:
        manufactured = date.fromisoformat(identity.manufacturing_date)
    except ValueError as error:
        raise ValueError("manufacturing date must use YYYY-MM-DD") from error
    if manufactured > date.today():
        raise ValueError("manufacturing date cannot be in the future")
    if not 1 <= identity.manufacturing_batch <= 0xFFFFFFFF:
        raise ValueError("manufacturing batch must be in the range 1..4294967295")


def load_identity(path: Path) -> DeviceIdentity:
    document = load_json(path)
    try:
        identity = DeviceIdentity(
            serial_number=str(document["serial_number"]),
            device_uuid=str(document["device_uuid"]).lower(),
            hardware_revision=str(document["hardware_revision"]),
            manufacturing_date=str(document["manufacturing_date"]),
            manufacturing_batch=int(document["manufacturing_batch"]),
        )
    except (KeyError, TypeError, ValueError) as error:
        raise ValueError(f"invalid device identity file: {path}") from error
    validate_identity(identity)
    return identity


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_json(path: Path) -> Dict[str, Any]:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read JSON file {path}: {error}") from error
    if not isinstance(document, dict):
        raise ValueError(f"JSON root must be an object: {path}")
    return document


def atomic_write_json(
    path: Path,
    document: Mapping[str, Any],
    private: bool = False,
    allow_secrets: bool = False,
) -> None:
    if not allow_secrets:
        reject_secret_fields(document)
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=str(path.parent), text=True)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as output:
            json.dump(document, output, indent=2, sort_keys=True)
            output.write("\n")
            output.flush()
            os.fsync(output.fileno())
        if private:
            os.chmod(temporary_name, 0o600)
        os.replace(temporary_name, path)
    finally:
        if os.path.exists(temporary_name):
            os.unlink(temporary_name)


def reject_secret_fields(value: Any, path: str = "root") -> None:
    if isinstance(value, Mapping):
        for key, child in value.items():
            normalized = str(key).lower().replace("-", "").replace("_", "")
            if any(part.replace("_", "") in normalized for part in _SECRET_KEY_PARTS):
                raise ValueError(f"secret-bearing field is prohibited in production output: {path}.{key}")
            reject_secret_fields(child, f"{path}.{key}")
    elif isinstance(value, (list, tuple)):
        for index, child in enumerate(value):
            reject_secret_fields(child, f"{path}[{index}]")


def partition_offset(partition_table: Path, labels: Iterable[str] = ("littlefs", "spiffs")) -> int:
    requested = {label.lower() for label in labels}
    for label, partition_type, subtype, offset, _size, _flags in partition_entries(partition_table):
        if label in requested or (partition_type == 0x01 and subtype == 0x82):
            return offset
    raise ValueError("partition table has no LittleFS/SPIFFS data partition")


def partition_entries(partition_table: Path) -> Sequence[Tuple[str, int, int, int, int, int]]:
    entries = []
    content = partition_table.read_bytes()
    for start in range(0, len(content) - 31, 32):
        entry = content[start:start + 32]
        magic, partition_type, subtype, offset, size, raw_label, flags = struct.unpack("<HBBII16sI", entry)
        if magic == 0xFFFF:
            break
        if magic != 0x50AA:
            continue
        label = raw_label.split(b"\0", 1)[0].decode("ascii", errors="ignore").lower()
        entries.append((label, partition_type, subtype, offset, size, flags))
    return tuple(entries)


def validate_production_partitions(partition_table: Path) -> None:
    entries = {entry[0]: entry for entry in partition_entries(partition_table)}
    required = {
        "nvs": (0x01, 0x02),
        "spiffs": (0x01, 0x82),
        "coredump": (0x01, 0x03),
    }
    for label, (expected_type, expected_subtype) in required.items():
        entry = entries.get(label)
        if entry is None:
            raise ValueError(f"production partition table is missing {label}")
        _name, partition_type, subtype, _offset, _size, flags = entry
        if partition_type != expected_type or subtype != expected_subtype or flags & 0x01 == 0:
            raise ValueError(f"production partition {label} must be encrypted")


def validate_release_package(directory: Path, expected_hardware: Optional[str] = None) -> ReleasePackage:
    root = directory.resolve()
    manifest = load_json(root / "manifest.json")
    if manifest.get("schema_version") != 1:
        raise ValueError("unsupported release manifest schema")
    artifacts = manifest.get("artifacts")
    if not isinstance(artifacts, dict):
        raise ValueError("release manifest has no artifact metadata")
    for name in FLASH_ARTIFACTS:
        path = root / name
        metadata = artifacts.get(name)
        if not path.is_file() or not isinstance(metadata, dict):
            raise ValueError(f"release package is missing {name}")
        expected_digest = metadata.get("sha256")
        if not isinstance(expected_digest, str) or sha256_file(path) != expected_digest.lower():
            raise ValueError(f"release artifact digest mismatch: {name}")
        if metadata.get("size") != path.stat().st_size:
            raise ValueError(f"release artifact size mismatch: {name}")
    hardware = str(manifest.get("hardware", ""))
    if expected_hardware is not None and hardware != expected_hardware:
        raise ValueError(f"release targets {hardware}, not {expected_hardware}")
    compatibility = manifest.get("compatibility")
    if not isinstance(compatibility, dict):
        raise ValueError("release manifest has no compatibility contract")
    configuration = str(compatibility.get("configuration", ""))
    match = re.fullmatch(r"CFG-([1-9][0-9]*)", configuration)
    if match is None:
        raise ValueError("release has an invalid configuration compatibility version")
    firmware_digest = sha256_file(root / "firmware.bin")
    if manifest.get("sha256") != firmware_digest:
        raise ValueError("top-level firmware digest does not match firmware.bin")
    firmware_version = str(compatibility.get("firmware", ""))
    if not re.fullmatch(r"FW-[0-9]+\.[0-9]+\.[0-9]+(?:[-+][0-9A-Za-z.-]+)?", firmware_version):
        raise ValueError("release has an invalid firmware compatibility version")
    validate_production_partitions(root / "partitions.bin")
    return ReleasePackage(
        directory=root,
        product_id=str(manifest.get("product", "")),
        hardware_revision=hardware,
        firmware_version=firmware_version,
        firmware_sha256=firmware_digest,
        configuration_schema_version=int(match.group(1)),
        filesystem_offset=partition_offset(root / "partitions.bin"),
    )


def parse_response(response: str) -> Dict[str, Any]:
    stripped = response.strip()
    if stripped.startswith("{"):
        fields = json.loads(stripped)
        if not isinstance(fields, dict):
            raise ValueError("device response is not an object")
    else:
        fields = dict(_FIELD.findall(stripped))
    if fields.get("ok") not in (True, "true"):
        raise RuntimeError(f"device command failed: {stripped}")
    return fields


def serial_command(port: str, command: str, baud: int = DEFAULT_BAUD, timeout: float = 30.0) -> Dict[str, Any]:
    try:
        import serial
    except ImportError as error:
        raise RuntimeError("factory serial operations require pyserial") from error
    deadline = time.monotonic() + timeout
    with serial.Serial(port, baudrate=baud, timeout=0.25, write_timeout=1) as connection:
        connection.reset_input_buffer()
        connection.write(command.encode("utf-8") + b"\r\n")
        connection.flush()
        while time.monotonic() < deadline:
            line = connection.readline().decode("utf-8", errors="replace").strip()
            if line.startswith("ok=") or line.startswith('{"ok"'):
                return parse_response(line)
    raise TimeoutError(f"timed out waiting for response to {command.split()[0]}")


def tool_command(tool: str) -> Sequence[str]:
    path = Path(tool)
    if path.suffix.lower() == ".py" or path.is_file():
        return (sys.executable, str(path.resolve())) if path.suffix.lower() == ".py" else (str(path.resolve()),)
    discovered = shutil.which(tool)
    if discovered is not None:
        return (discovered,)
    platformio_home = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
    platformio_script = platformio_home / "packages" / "tool-esptoolpy" / f"{tool}.py"
    if platformio_script.is_file():
        interpreter_candidates = (
            platformio_home / "penv" / "Scripts" / "python.exe",
            platformio_home / "penv" / "bin" / "python",
        )
        interpreter = next((candidate for candidate in interpreter_candidates if candidate.is_file()), None)
        if interpreter is None:
            raise FileNotFoundError("PlatformIO's Python interpreter is unavailable")
        return (str(interpreter), str(platformio_script))
    raise FileNotFoundError(
        f"{tool} was not found on PATH or in PlatformIO; pass its executable or Python script path")


def run_tool(command: Sequence[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=True, text=True, capture_output=True)


def _find_security_value(value: Any, names: Tuple[str, ...]) -> Optional[Any]:
    if isinstance(value, Mapping):
        for key, child in value.items():
            normalized = str(key).lower().replace("-", "_").replace(" ", "_")
            if normalized in names:
                if isinstance(child, Mapping):
                    for value_key in ("value", "readable", "raw"):
                        if value_key in child:
                            return child[value_key]
                return child
            found = _find_security_value(child, names)
            if found is not None:
                return found
    elif isinstance(value, list):
        for child in value:
            found = _find_security_value(child, names)
            if found is not None:
                return found
    return None


def _enabled(value: Any, parity: bool = False) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, int):
        return value.bit_count() % 2 == 1 if parity else value != 0
    text = str(value).strip().lower()
    if text in ("true", "enabled", "enable", "yes"):
        return True
    number_match = re.search(r"0x[0-9a-f]+|\b[0-9]+\b", text)
    if number_match is not None:
        number = int(number_match.group(0), 0)
        return number.bit_count() % 2 == 1 if parity else number != 0
    return False


def parse_security_report(document: Mapping[str, Any]) -> Dict[str, str]:
    secure_boot = _find_security_value(document, ("secure_boot_en", "secure_boot_enabled"))
    flash_crypt = _find_security_value(document, ("spi_boot_crypt_cnt", "flash_encryption_enabled"))
    if secure_boot is None or flash_crypt is None:
        raise ValueError("eFuse report does not contain secure boot and flash encryption state")
    return {
        "secure_boot_state": "enabled" if _enabled(secure_boot) else "disabled",
        "flash_encryption_state": "enabled" if _enabled(flash_crypt, parity=True) else "disabled",
    }


def read_security_state(port: str, espefuse: str = "espefuse") -> Dict[str, str]:
    descriptor, report_name = tempfile.mkstemp(prefix="switch-actuator-efuse-", suffix=".json")
    os.close(descriptor)
    try:
        command = [*tool_command(espefuse), "--chip", "esp32s3", "--port", port,
                   "summary", "--format", "json", "--file", report_name]
        run_tool(command)
        report = load_json(Path(report_name))
    finally:
        try:
            os.unlink(report_name)
        except FileNotFoundError:
            pass
    if not isinstance(report, dict):
        raise ValueError("espefuse security report is not an object")
    return parse_security_report(report)


def identity_document(identity: DeviceIdentity) -> Dict[str, Any]:
    return asdict(identity)