# Switch Actuator

Switch Actuator is production firmware for the Waveshare ESP32-S3 Relay 6CH.
It provides deterministic relay control through KNX/IP, Modbus RTU, physical
buttons, a local serial service interface, and an authenticated HTTPS/WebSocket
management interface.

The firmware is safety-oriented: relay outputs start inactive, all commands
pass through application policy and arbitration, configuration changes are
validated and persisted transactionally, and production startup fails closed
when required hardware security controls are absent.

## Supported Product

| Item | Current contract |
|---|---|
| Board | Waveshare ESP32-S3 Relay 6CH |
| Hardware compatibility | `HW-A01` |
| Firmware compatibility | `FW-1.4.0` |
| Configuration schema | `CFG-4` |
| Web API | `API-v1` |
| Modbus contract | `MODBUS-v1` |
| KNX application | `KNX-APP-v1` |
| Filesystem contract | `FS-v1` |

Board descriptors also exist for custom 6-channel and 12-channel hardware.
The current domain, protocol maps, configuration, and Web UI remain compiled
for six channels; selecting a different relay count is rejected at startup
until those shared product contracts are widened.

## Architecture

The firmware follows ports and adapters:

```text
Domain -> Application -> HAL/Ports -> ESP32-S3 adapters
```

Domain and application services own relay state, policy, lifecycle,
configuration, diagnostics, and reset behavior. ESP32, Arduino, GPIO, NVS,
network, protocol-library, and filesystem dependencies remain in adapters.
See [System architecture](docs/architecture/system.md),
[Hardware interfaces](src/hal/README.md), and
[Boot flow](docs/architecture/boot-flow.md).

## Build Profiles

PlatformIO defines three explicit environments:

| Environment | Intended use | Security behavior |
|---|---|---|
| `development` | Local implementation and diagnosis | Debug interfaces and development credentials allowed |
| `engineering` | Release-like hardware validation | Debug interfaces allowed; development credentials rejected |
| `production` | Manufacturing and field release | Secure Boot v2, flash encryption, signed firmware, and protected inputs required |

Build development or engineering firmware with:

```powershell
pio run -e development
pio run -e engineering
```

Do not treat an unsigned PlatformIO application image as a production release.
Production artifacts are created through the protected workflow documented in
[Release process](docs/release/release-process.md).

The React management interface is maintained under `web/`:

```powershell
Set-Location web
npm install
npm run lint
npm test
npm run build
```

## Configuration And Provisioning

Mutable configuration is stored transactionally in NVS. LittleFS contains the
seven-file deployment bundle under `data/config/` and embedded Web assets under
`data/www/`. The embedded `config/default_configuration.json` is the final
recoverable configuration source before safe domain defaults.

Boot precedence is NVS, primary LittleFS bundle, backup LittleFS bundle,
embedded configuration, then safe defaults. A normal firmware upload does not
replace LittleFS. See [Product configuration](docs/product/configuration.md)
and [Filesystem architecture](docs/architecture/filesystem.md).

Production identity, security state, and initial administrator credentials are
created by an audited station using the signed release package. Start with
[Manufacturing overview](docs/manufacturing/overview.md) and
[Provisioning](docs/manufacturing/provisioning.md). Never commit production
passwords, private keys, signing keys, tokens, or per-device secrets.

## Product Interfaces

- [Modbus RTU](docs/protocols/modbus.md)
- [KNX/IP application](docs/protocols/knx-ip.md)
- [Web API v1](docs/protocols/web-api.md)
- [Device identity](docs/product/device-identity.md)
- [Compatibility and versioning](docs/product/compatibility.md)

## Operations

- [Diagnostics](docs/product/diagnostics.md)
- [Recovery](docs/product/recovery.md)
- [Troubleshooting](docs/product/troubleshooting.md)
- [Service mode](docs/manufacturing/service-mode.md)
- [Factory reset](docs/manufacturing/factory-reset.md)

The serial `version` and `status` commands are safe read-only starting points.
Mutating service operations require physical BOOT-button authorization and are
never exposed as unauthenticated network operations.

## Documentation

[Product documentation index](docs/README.md) is the canonical navigation
entry for maintainers. Normative product contracts live under `docs/`.
Engineering plans, experiments, ADRs, and implementation prompts live under
`design/` and do not override product contracts.

Update [CHANGELOG.md](CHANGELOG.md) for release-visible changes. A release is
complete only when its compatibility decisions, signed artifacts, checksums,
SBOM, release notes, manufacturing evidence, and recovery implications have
been reviewed together.
