#!/usr/bin/env python3
"""Run PlatformIO clang-tidy and reject analyzer failures or reported defects."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--environment", default="development", help="PlatformIO environment")
    parser.add_argument("--report", type=Path, help="Optional path for the raw JSON report")
    return parser.parse_args()


def print_defect(defect: dict[str, Any]) -> None:
    location = defect.get("file", "<unknown>")
    if defect.get("line") is not None:
        location += f":{defect['line']}"
    defect_id = defect.get("id", "clang-tidy")
    message = defect.get("message", "unspecified defect")
    print(f"{location}: {defect_id}: {message}", file=sys.stderr)


def find_platformio() -> str | None:
    executable = shutil.which("platformio")
    if executable is not None:
        return executable
    scripts_directory = Path.home() / ".platformio" / "penv" / "Scripts"
    for name in ("platformio.exe", "platformio"):
        candidate = scripts_directory / name
        if candidate.is_file():
            return str(candidate)
    return None


def main() -> int:
    arguments = parse_arguments()
    platformio = find_platformio()
    if platformio is None:
        print("platformio executable not found", file=sys.stderr)
        return 2

    command = [
        platformio,
        "check",
        "-e",
        arguments.environment,
        "--skip-packages",
        "--json-output",
    ]
    process = subprocess.run(command, capture_output=True, text=True, check=False)
    if process.stderr:
        print(process.stderr, end="", file=sys.stderr)

    if arguments.report is not None:
        arguments.report.parent.mkdir(parents=True, exist_ok=True)
        arguments.report.write_text(process.stdout, encoding="utf-8")

    try:
        records = json.loads(process.stdout)
    except json.JSONDecodeError as error:
        print(f"invalid clang-tidy JSON report: {error}", file=sys.stderr)
        return 2

    if process.returncode != 0:
        print(f"PlatformIO clang-tidy exited with status {process.returncode}", file=sys.stderr)
        return process.returncode
    if not isinstance(records, list) or not records:
        print("clang-tidy JSON report contains no analyzer records", file=sys.stderr)
        return 2

    failed = False
    for record in records:
        defects = record.get("defects", [])
        succeeded = record.get("succeeded") is True
        print(f"{record.get('tool', 'unknown')}: succeeded={succeeded} defects={len(defects)}")
        if not succeeded or defects:
            failed = True
        for defect in defects:
            print_defect(defect)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())