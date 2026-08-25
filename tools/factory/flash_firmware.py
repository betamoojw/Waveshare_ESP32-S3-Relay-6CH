"""Flash an approved production release package without modifying eFuses."""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Optional, Sequence

try:
    from .factory_common import ReleasePackage, run_tool, tool_command, validate_release_package
except ImportError:
    from factory_common import ReleasePackage, run_tool, tool_command, validate_release_package


def flash_release(
    package: ReleasePackage,
    port: str,
    secure_boot_public_key: Path,
    baud: int = 921600,
    esptool: str = "esptool",
    espsecure: str = "espsecure",
) -> None:
    verify_secure_boot_images(package, secure_boot_public_key, espsecure)
    command = [
        *tool_command(esptool),
        "--chip", "esp32s3",
        "--port", port,
        "--baud", str(baud),
        "--before", "default-reset",
        "--after", "hard-reset",
        "write-flash",
        "0x0", str(package.directory / "bootloader.bin"),
        "0x8000", str(package.directory / "partitions.bin"),
        "0x10000", str(package.directory / "firmware.bin"),
        hex(package.filesystem_offset), str(package.directory / "filesystem.bin"),
    ]
    run_tool(command)


def verify_secure_boot_images(
    package: ReleasePackage,
    secure_boot_public_key: Path,
    espsecure: str = "espsecure",
) -> None:
    trusted_key = secure_boot_public_key.resolve()
    if not trusted_key.is_file():
        raise FileNotFoundError(f"trusted Secure Boot public key not found: {trusted_key}")
    if package.directory == trusted_key.parent or package.directory in trusted_key.parents:
        raise ValueError("trusted Secure Boot public key must come from the station trust store, not the release package")
    for image in ("bootloader.bin", "firmware.bin"):
        run_tool([
            *tool_command(espsecure),
            "verify-signature", "--version", "2",
            "--keyfile", str(trusted_key),
            str(package.directory / image),
        ])


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release", required=True, type=Path)
    parser.add_argument("--hardware-revision", required=True)
    parser.add_argument("--port", required=True)
    parser.add_argument("--secure-boot-public-key", required=True, type=Path,
                        help="approved public key from the station trust store")
    parser.add_argument("--baud", type=int, default=921600)
    parser.add_argument("--esptool", default="esptool", help="esptool executable or esptool.py path")
    parser.add_argument("--espsecure", default="espsecure", help="espsecure executable or espsecure.py path")
    options = parser.parse_args(arguments)
    package = validate_release_package(options.release, options.hardware_revision)
    flash_release(package, options.port, options.secure_boot_public_key,
                  options.baud, options.esptool, options.espsecure)
    print(f"flashed approved {package.firmware_version} release to {options.port}")
    print("security eFuses were not modified; use the approved secure station procedure")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())