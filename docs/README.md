# Switch Actuator Product Documentation

This directory is the canonical product documentation for the Switch Actuator firmware. It is organized by lifecycle responsibility rather than implementation history.

## Architecture

- [System architecture](architecture/system.md): layering, lifecycle, safety, ownership, and runtime constraints.
- [Network architecture](architecture/network.md): Wi-Fi/Ethernet selection, recovery access point, and network security boundaries.
- [Filesystem architecture](architecture/filesystem.md): NVS/LittleFS ownership, precedence, recovery, and deployment.
- [Security architecture](architecture/security.md): build profiles, trust boundaries, credentials, secure boot, and flash encryption.

## Protocols

- [Modbus RTU](protocols/modbus.md): frozen register map and transport contract.
- [KNX/IP](protocols/knx-ip.md): application model, communication objects, and DPT contract.
- [Web API](protocols/web-api.md): HTTPS and WebSocket API v1 contract.

## Product

- [Device identity](product/device-identity.md): identity fields, ownership, provisioning, and preservation.
- [Configuration](product/configuration.md): configuration sources, validation, persistence, and reset behavior.
- [Update](product/update.md): release artifacts, update policy, rollback requirements, and current limitations.
- [Compatibility](product/compatibility.md): current hardware, firmware, configuration, API, protocol, and filesystem versions.

## Manufacturing

- [Provisioning](manufacturing/provisioning.md): production station sequence, security prerequisites, and verification.
- [Factory reset](manufacturing/factory-reset.md): exact remove/preserve contract.
- [Service mode](manufacturing/service-mode.md): physical entry, authorization, timeout, and serial operations.

## Release

- [Release process](release/release-process.md): complete signed package and CI release flow.
- [Versioning](release/versioning.md): firmware SemVer, compatibility bumps, and release checklist.

Engineering plans, experiments, ADRs, and implementation prompts remain under `design/`. They are supporting records, not product contracts. When they conflict with this directory, the product documentation and implemented tests are authoritative.
