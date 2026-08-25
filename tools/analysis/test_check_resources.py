from __future__ import annotations

import struct
import tempfile
import unittest
from pathlib import Path

from check_resources import parse_partitions, parse_size_output, parse_stack_usage


class ResourceCheckTest(unittest.TestCase):
    def test_parses_elf_sections(self) -> None:
        sections = parse_size_output("""
firmware.elf  :
section             size         addr
.iram0.vectors      1024   1074266112
.iram0.text        32768   1074267136
.dram0.data        16384   1073479680
.dram0.bss         65536   1073496064
.flash.text       900000   1107427328
Total            1011712
""")
        self.assertEqual(32768, sections[".iram0.text"])
        self.assertEqual(65536, sections[".dram0.bss"])

    def test_rejects_overlapping_partitions(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "partitions.bin"
            path.write_bytes(b"".join((
                self.partition("app0", 0x10000, 0x20000),
                self.partition("app1", 0x20000, 0x20000),
            )))
            with self.assertRaisesRegex(ValueError, "overlap"):
                parse_partitions(path)

    def test_parses_stack_usage_and_rejects_unbounded_dynamic_stack(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            static_path = Path(temporary) / "static.su"
            static_path.write_text("source.cpp:1:1:void safe()\t512\tstatic\n", encoding="utf-8")
            self.assertEqual((512, "source.cpp:1:1:void safe()", 1), parse_stack_usage([static_path]))

            dynamic_path = Path(temporary) / "dynamic.su"
            dynamic_path.write_text("source.cpp:2:1:void unsafe()\t64\tdynamic\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "unbounded dynamic"):
                parse_stack_usage([dynamic_path])

    @staticmethod
    def partition(label: str, offset: int, size: int) -> bytes:
        encoded = label.encode("ascii").ljust(16, b"\0")
        return struct.pack("<HBBII16sI", 0x50AA, 0, 0, offset, size, encoded, 0)


if __name__ == "__main__":
    unittest.main()