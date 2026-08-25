# Static Analysis And Resource Budgets

CI applies five complementary checks to repository-owned firmware code:

- `clang-format` checks changed C/C++ files against the repository style;
- PlatformIO Check runs the focused `.clang-tidy` correctness and security
  policy against `src/` while excluding framework and library packages;
- `cppcheck` independently applies warning and portability checks to owned C
  and C++ sources;
- GCC compiles all sources with `-Wall -Wextra` and generates `.su` stack
  reports and a linker map for resource analysis;
- The static analyzers enforce the equivalent of `-Werror` on owned code only,
  since third-party libraries (e.g. `nanomodbus`) under `src/` share the same
  compiler flags.

Run the local analyzer gates after installing the CI tool versions:

```text
python tools/analysis/run_clang_tidy.py --report .pio/reports/clang-tidy.json
cppcheck --enable=warning,portability --std=c++17 --error-exitcode=1 \
  --inline-suppr --suppress=missingIncludeSystem --suppress=unknownMacro \
  --suppress=unmatchedSuppression \
  --suppress=subtractPointers:src/adapters/configuration/json_configuration_source.cpp \
  --quiet -Isrc -Iinclude src
```

The cppcheck suppression covers subtraction of linker-provided binary start and
end symbols. The linker guarantees that relationship, but cppcheck models the
symbols as unrelated C++ arrays. No third-party source is analyzed by this
command.

## Resource Gate

`resource_limits.json` is the reviewed ESP32-S3 budget contract. After firmware
and LittleFS builds, run:

```text
python tools/analysis/check_resources.py \
  --build-dir .pio/build/engineering \
  --report .pio/reports/engineering-resources.json
```

The command fails when:

- the firmware binary or LittleFS image exceeds its absolute or partition
  percentage limit;
- combined flashable build artifacts exceed the total flash-image limit;
- the binary partition table overlaps or extends beyond the 8 MB device;
- ELF IRAM or static DRAM exceeds its limit;
- estimated heap headroom falls below its minimum;
- any owned function exceeds the stack-frame limit or has unbounded dynamic
  stack usage;
- required ELF, binary, filesystem, partition, linker-map, or `.su` evidence is
  missing.

Estimated heap headroom is the board's internal DRAM capacity minus ELF static
DRAM sections. It is a build-time budget, not a replacement for runtime
`minimum_free_heap_bytes` evidence from hardware qualification. Release
approval should review both.

Run parser tests without building firmware:

```text
python -B -m unittest discover -s tools/analysis -p "test_*.py" -v
```

Budget changes require review with measured engineering and production JSON
reports. Do not raise a limit merely to make CI pass.