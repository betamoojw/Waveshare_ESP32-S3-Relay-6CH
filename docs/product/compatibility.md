# Version Compatibility Contract

## Current Versions

| Surface | Canonical version | Source of truth | Compatibility boundary |
|---|---|---|---|
| Hardware | `HW-A01` | Selected board descriptor | PCB, pin assignment, polarity, and fitted peripherals |
| Firmware | `FW-1.4.0` | Release `FIRMWARE_VERSION`, normalized by `extra_script.py` | Executable feature and fix release |
| Configuration | `CFG-4` | `domain::compatibility::configuration` | Persisted NVS and JSON configuration schema |
| Web API | `API-v1` | `domain::compatibility::api` | HTTPS request/response and WebSocket wire contract |
| Modbus | `MODBUS-v1` | `domain::compatibility::modbus` | Register addresses, types, encoding, and side effects |
| KNX application | `KNX-APP-v1` | `domain::compatibility::knxApplication` | Parameters, communication objects, DPTs, and behavior |
| Filesystem | `FS-v1` | `domain::compatibility::filesystem` | LittleFS paths, required files, backup layout, and ownership |

The C++ source of truth for non-hardware contracts is `src/domain/version_compatibility.h`. Numeric constants used by parsers and protocols alias the same manifest instead of repeating literals.

Development/native builds without an injected release version report `FW-1.4.0+development`. PlatformIO release builds normalize `1.4.0`, `v1.4.0`, or `FW-1.4.0` to `FW-1.4.0` and compile that label into the image. Production rejects non-SemVer firmware versions.

## Discovery

The serial `version` command reports all seven versions in one machine-readable line:

```text
ok=true hardware=HW-A01 firmware=FW-1.4.0 config=CFG-4 api=API-v1 modbus=MODBUS-v1 knx=KNX-APP-v1 filesystem=FS-v1 cli=1 profile=production
```

Authenticated `GET /api/v1/capabilities` reports the same values in `versions`:

```json
{
  "apiVersion": "1.0",
  "versions": {
    "hardware": "HW-A01",
    "firmware": "FW-1.4.0",
    "configuration": "CFG-4",
    "api": "API-v1",
    "modbus": "MODBUS-v1",
    "knxApplication": "KNX-APP-v1",
    "filesystem": "FS-v1"
  }
}
```

Modbus holding register 132 contains the running firmware major/minor encoding (`FW-1.4.0` becomes `104`). Full labels remain available through serial and HTTPS because a 16-bit register cannot represent every contract.

## Bump Rules

- Increment `FW-MAJOR` for incompatible device behavior, `FW-MINOR` for compatible features, and `FW-PATCH` for compatible fixes.
- Increment `CFG-*` when persisted or deployment configuration structure changes. Readers must explicitly migrate every supported older schema or reject it safely.
- Increment `API-v*` only for incompatible wire changes and introduce a matching base path. Additive v1 fields do not require a major bump.
- Increment `MODBUS-v*` for incompatible address, type, encoding, exception, or side-effect changes. Additions in reserved space may remain v1 when existing clients are unaffected.
- Increment `KNX-APP-v*` for incompatible parameter, communication-object, DPT, ETS product-data, or runtime semantic changes.
- Increment `FS-v*` for incompatible path, required-file set, backup, atomicity, or ownership changes. Configuration content changes alone use `CFG-*`.
- Increment `HW-*` when firmware compatibility depends on a PCB/BOM/pin/polarity/peripheral revision. New board families may start at `HW-A01` independently.

Every release records all seven values in the `compatibility` objects of both
`manifest.json` and `version.json`. A firmware version bump must not silently
change another compatibility surface; any such change requires both labels to
be updated and migration/release notes.