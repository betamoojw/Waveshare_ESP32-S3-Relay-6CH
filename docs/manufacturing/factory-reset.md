# Factory Reset Contract

## Purpose

Factory reset removes field-owned configuration and credentials while preserving the device's manufacturing and cryptographic identity. It is a physical-presence operation initiated by holding the BOOT button for at least ten seconds and releasing it. Remote factory reset is prohibited.

The reset is identity-preserving. It must never erase or regenerate manufacturing identity as a side effect of removing user data.

## Remove

Factory reset removes or resets all user-owned state:

- Wi-Fi profiles, SSIDs, passphrases, static IP settings, host name, and recovery access-point preferences;
- web users, password salts/verifiers, active sessions, and web enablement/provisioning state;
- relay enablement, restore policies, and configured default states;
- runtime scenes, including learned scene state;
- pending relay timers;
- KNX enablement, individual address, group addresses, publication intervals, polarity, and channel participation;
- Modbus unit ID, baud rate, parity, data bits, and stop bits;
- indicator brightness and buzzer preferences;
- staged configuration, queued commands, pending web requests, command tracking, and event-journal state.

Configuration fields are replaced with validated safe domain defaults and persisted transactionally. Relays are placed in safety lockout before reset processing begins.

## Preserve

Factory reset preserves device-owned identity and service evidence:

- device serial number;
- device UUID;
- hardware MAC addresses stored by the ESP32 eFuses;
- product ID;
- board model and hardware revision;
- manufacturing date and manufacturing batch;
- TLS certificate and private key;
- web token-signing key and signing generation;
- secure-boot keys, flash-encryption keys, and other eFuse security state;
- persistent diagnostic counters and fault history storage;
- firmware image, partition table, OTA state, and rollback metadata;
- LittleFS deployment configuration, backup configuration, and web assets.

Preserved TLS and signing keys are the factory security identity for the current implementation. Reset revokes all sessions and removes every web user, but subsequent initial-administrator provisioning reuses this identity rather than generating a new one.

## Persistence Semantics

`ConfigurationService::factoryReset()` constructs safe defaults, copies only the manufacturing identity fields, validates the complete result, and writes it through the normal A/B transactional settings store. It does not call the settings namespace erase operation.

`WebSecurityService::eraseUsersPreservingIdentity()` clears users and sessions while transactionally retaining the security identity record. Failure to persist either reset portion is reported and prevents a success indication.

Because a valid reset configuration remains in NVS, LittleFS deployment defaults are preserved as recovery/provisioning data but do not automatically override the reset configuration on the next boot.

## Non-Goals

Factory reset is not secure decommissioning, firmware recovery, flash sanitization, or manufacturing reprovisioning. Erasing factory identity requires a separate, explicitly authorized manufacturing process and must not be added to this reset path.