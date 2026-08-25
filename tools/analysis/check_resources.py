"""Enforce ESP32-S3 firmware, partition, memory, and stack budgets."""

from __future__ import annotations

import argparse
import json
import os
import re
import struct
import subprocess
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Dict, Iterable, Mapping, Optional, Sequence, Tuple


PARTITION_ENTRY_SIZE = 32
PARTITION_MAGIC = 0x50AA
PARTITION_END_MAGIC = 0xFFFF


@dataclass(frozen=True)
class Limits:
    flash_capacity_bytes: int
    internal_dram_capacity_bytes: int
    max_firmware_binary_bytes: int
    max_firmware_partition_percent: int
    max_filesystem_image_bytes: int
    max_filesystem_partition_percent: int
    max_total_flash_image_bytes: int
    max_iram_bytes: int
    max_static_dram_bytes: int
    min_estimated_heap_headroom_bytes: int
    max_stack_frame_bytes: int


@dataclass(frozen=True)
class Partition:
    label: str
    offset: int
    size: int


def load_limits(path: Path) -> Limits:
    document = json.loads(path.read_text(encoding="utf-8"))
    if document.get("schema_version") != 1:
        raise ValueError("unsupported resource-limit schema")
    values = {key: value for key, value in document.items() if key != "schema_version"}
    if any(not isinstance(value, int) or value <= 0 for value in values.values()):
        raise ValueError("resource limits must be positive integers")
    return Limits(**values)


def parse_partitions(path: Path) -> Tuple[Partition, ...]:
    content = path.read_bytes()
    partitions = []
    for start in range(0, len(content) - PARTITION_ENTRY_SIZE + 1, PARTITION_ENTRY_SIZE):
        magic, _partition_type, _subtype, offset, size, raw_label, _flags = struct.unpack(
            "<HBBII16sI", content[start:start + PARTITION_ENTRY_SIZE])
        if magic == PARTITION_END_MAGIC:
            break
        if magic != PARTITION_MAGIC:
            continue
        label = raw_label.split(b"\0", 1)[0].decode("ascii")
        partitions.append(Partition(label, offset, size))
    if not partitions:
        raise ValueError("partition table contains no entries")
    ordered = sorted(partitions, key=lambda partition: partition.offset)
    for previous, current in zip(ordered, ordered[1:]):
        if previous.offset + previous.size > current.offset:
            raise ValueError(f"partitions overlap: {previous.label} and {current.label}")
    return tuple(partitions)


def parse_size_output(output: str) -> Dict[str, int]:
    sections: Dict[str, int] = {}
    for line in output.splitlines():
        match = re.match(r"^\s*(\.[^\s]+)\s+(\d+)\s+(?:0x[0-9a-fA-F]+|\d+)\s*$", line)
        if match is not None:
            sections[match.group(1)] = sections.get(match.group(1), 0) + int(match.group(2))
    if not sections:
        raise ValueError("ELF size tool returned no sections")
    return sections


def parse_stack_usage(paths: Iterable[Path]) -> Tuple[int, str, int]:
    maximum = 0
    maximum_function = ""
    count = 0
    for path in paths:
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            fields = line.rsplit("\t", 2)
            if len(fields) != 3 or not fields[1].isdigit():
                continue
            function, size_text, qualifier = fields
            count += 1
            if qualifier.startswith("dynamic") and "bounded" not in qualifier:
                raise ValueError(f"unbounded dynamic stack usage: {function}")
            size = int(size_text)
            if size > maximum:
                maximum = size
                maximum_function = function
    if count == 0:
        raise ValueError("no GCC stack-usage records were found")
    return maximum, maximum_function, count


def discover_size_tool(explicit: Optional[Path] = None) -> Path:
    if explicit is not None:
        if not explicit.is_file():
            raise FileNotFoundError(f"ELF size tool not found: {explicit}")
        return explicit
    executable = "xtensa-esp32s3-elf-size.exe" if os.name == "nt" else "xtensa-esp32s3-elf-size"
    platformio_home = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
    candidates = sorted((platformio_home / "packages").glob(f"toolchain-xtensa-esp-elf*/bin/{executable}"))
    if not candidates:
        raise FileNotFoundError("xtensa-esp32s3-elf-size was not found in PlatformIO packages")
    return candidates[-1]


def single_artifact(build_directory: Path, pattern: str) -> Path:
    matches = sorted(build_directory.glob(pattern))
    if len(matches) != 1:
        raise ValueError(f"expected exactly one {pattern} artifact, found {len(matches)}")
    return matches[0]


def firmware_binary(build_directory: Path) -> Path:
    signed = sorted(build_directory.glob("firmware_*-signed.bin"))
    if signed:
        if len(signed) != 1:
            raise ValueError(f"expected exactly one signed firmware artifact, found {len(signed)}")
        return signed[0]
    unsigned = sorted(path for path in build_directory.glob("firmware_*.bin")
                      if not path.name.endswith("-signed.bin"))
    if len(unsigned) != 1:
        raise ValueError(f"expected exactly one firmware artifact, found {len(unsigned)}")
    return unsigned[0]


def percent(used: int, capacity: int) -> float:
    return round(used * 100.0 / capacity, 2)


def enforce(name: str, actual: int, maximum: int) -> None:
    if actual > maximum:
        raise ValueError(f"{name} exceeds budget: {actual} > {maximum} bytes")


def analyze(
    build_directory: Path,
    limits: Limits,
    size_tool: Optional[Path] = None,
    require_filesystem: bool = True,
) -> Dict[str, object]:
    build = build_directory.resolve()
    elf = single_artifact(build, "firmware_*.elf")
    firmware = firmware_binary(build)
    partition_table = build / "partitions.bin"
    if not partition_table.is_file():
        raise FileNotFoundError(f"partition table not found: {partition_table}")
    map_file = elf.with_suffix(".map")
    if not map_file.is_file():
        raise FileNotFoundError(f"linker map not found: {map_file}")
    filesystem = build / "littlefs.bin"
    if require_filesystem and not filesystem.is_file():
        raise FileNotFoundError(f"filesystem image not found: {filesystem}")

    partitions = parse_partitions(partition_table)
    partition_by_label = {partition.label: partition for partition in partitions}
    app = partition_by_label.get("app0")
    filesystem_partition = partition_by_label.get("spiffs") or partition_by_label.get("littlefs")
    if app is None or filesystem_partition is None:
        raise ValueError("partition table must contain app0 and filesystem partitions")
    partition_end = max(partition.offset + partition.size for partition in partitions)
    enforce("partition table end", partition_end, limits.flash_capacity_bytes)

    result = subprocess.run(
        [str(discover_size_tool(size_tool)), "-A", "-d", str(elf)],
        check=True,
        text=True,
        capture_output=True,
    )
    sections = parse_size_output(result.stdout)
    iram_bytes = sum(size for name, size in sections.items() if name.startswith(".iram"))
    static_dram_bytes = sum(
        size for name, size in sections.items()
        if name.startswith(".dram") or name in (".data", ".bss", ".noinit")
    )
    estimated_heap_headroom = limits.internal_dram_capacity_bytes - static_dram_bytes
    maximum_stack, maximum_stack_function, stack_record_count = parse_stack_usage(build.rglob("*.su"))
    firmware_bytes = firmware.stat().st_size
    filesystem_bytes = filesystem.stat().st_size if filesystem.is_file() else 0
    auxiliary_flash_bytes = sum(
        path.stat().st_size for path in (build / "bootloader.bin", partition_table) if path.is_file())
    total_flash_bytes = firmware_bytes + filesystem_bytes + auxiliary_flash_bytes

    enforce("firmware binary", firmware_bytes, limits.max_firmware_binary_bytes)
    enforce("firmware partition usage", firmware_bytes,
            app.size * limits.max_firmware_partition_percent // 100)
    enforce("filesystem image", filesystem_bytes, limits.max_filesystem_image_bytes)
    enforce("filesystem partition usage", filesystem_bytes,
            filesystem_partition.size * limits.max_filesystem_partition_percent // 100)
    enforce("total flash image", total_flash_bytes, limits.max_total_flash_image_bytes)
    enforce("IRAM", iram_bytes, limits.max_iram_bytes)
    enforce("static DRAM", static_dram_bytes, limits.max_static_dram_bytes)
    if estimated_heap_headroom < limits.min_estimated_heap_headroom_bytes:
        raise ValueError(
            "estimated heap headroom below budget: "
            f"{estimated_heap_headroom} < {limits.min_estimated_heap_headroom_bytes} bytes")
    enforce("stack frame", maximum_stack, limits.max_stack_frame_bytes)

    return {
        "schema_version": 1,
        "build_directory": str(build),
        "limits": asdict(limits),
        "measurements": {
            "firmware_binary_bytes": firmware_bytes,
            "firmware_partition_bytes": app.size,
            "firmware_partition_percent": percent(firmware_bytes, app.size),
            "filesystem_image_bytes": filesystem_bytes,
            "filesystem_partition_bytes": filesystem_partition.size,
            "filesystem_partition_percent": percent(filesystem_bytes, filesystem_partition.size),
            "total_flash_image_bytes": total_flash_bytes,
            "partition_table_end_bytes": partition_end,
            "iram_bytes": iram_bytes,
            "static_dram_bytes": static_dram_bytes,
            "estimated_heap_headroom_bytes": estimated_heap_headroom,
            "maximum_stack_frame_bytes": maximum_stack,
            "maximum_stack_function": maximum_stack_function,
            "stack_record_count": stack_record_count,
            "linker_map_bytes": map_file.stat().st_size,
        },
    }


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--limits", type=Path,
                        default=Path(__file__).with_name("resource_limits.json"))
    parser.add_argument("--size-tool", type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--allow-missing-filesystem", action="store_true")
    options = parser.parse_args(arguments)
    report = analyze(
        options.build_dir,
        load_limits(options.limits),
        options.size_tool,
        require_filesystem=not options.allow_missing_filesystem,
    )
    encoded = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if options.report:
        options.report.parent.mkdir(parents=True, exist_ok=True)
        options.report.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())