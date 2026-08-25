# Device Identity

## Purpose

Device identity binds a physical actuator to manufacturing records, release compatibility, diagnostics, and service operations. Identity is device-owned data and is not ordinary user configuration.

## Identity Fields

| Field | Source | Lifecycle |
|---|---|---|
| Product ID | Manufacturing configuration | Provision once; production-locked |
| Product name | Firmware product definition | Changes only with product firmware |
| Board model | Compiled board descriptor | Provisioned from the selected board |
| Hardware revision | Board descriptor and manufacturing record | Canonical `HW-*`; production-locked |
| Firmware version | Compiled release metadata | Changes with firmware installation |
| Serial number | Manufacturing station | Unique, provision once |
| Device UUID | Manufacturing station | Unique, provision once |
| MAC address | ESP32 eFuse/base MAC | Hardware-owned and read-only |
| Manufacturing date | Manufacturing station | ISO `YYYY-MM-DD`; production-locked |
| Manufacturing batch | Manufacturing station | Nonzero for provisioned production units |

The domain representation is `DeviceIdentity`; construction validates bounded strings, UUID, MAC, date, and manufacturing completeness before diagnostics expose the snapshot.

## Ownership and Mutation

Manufacturing identity is written through the validated configuration service during physically authorized provisioning. The compiled board descriptor supplies product ID, board model, and hardware revision so a fixture cannot substitute incompatible hardware metadata.

Production factory configuration becomes immutable after security or manufacturing provisioning. Ordinary web, KNX, and Modbus users cannot modify identity. Service-mode identity mutation also fails when the production lock is active.

## Preservation

Factory reset and field-service user reset preserve product ID, board model, hardware revision, serial number, UUID, manufacturing date, batch, MAC/eFuse state, and factory security identity. See [Factory reset](../manufacturing/factory-reset.md).

Secure decommissioning or manufacturing reprovisioning is intentionally separate from factory reset and requires a dedicated audited process.

## Discovery

Physically authorized serial service commands expose complete identity. Authenticated diagnostics expose bounded non-secret identity. No interface returns private keys, password verifiers, Wi-Fi passphrases, or session tokens.

Compatibility labels associated with identity are defined in [Compatibility](compatibility.md).
