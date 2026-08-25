# Product Configuration

## Ownership

`ConfigurationService` is the single owner of active and staged product configuration. Adapters parse, serialize, or transport configuration, but they do not apply partially validated values.

All mutations follow:

```text
validate -> stage -> persist -> apply/restart
```

Invalid or incomplete input is rejected without changing the active configuration.

## Sources and Precedence

Boot resolves configuration in this order:

1. newest valid transactional NVS generation;
2. complete valid LittleFS `/config/*.json` bundle;
3. complete valid LittleFS backup bundle;
4. embedded `config/default_configuration.json`;
5. safe domain defaults.

NVS is authoritative for mutable runtime configuration. LittleFS is deployment/recovery input and web-asset storage. Detailed atomicity and recovery behavior is defined in [Filesystem architecture](../architecture/filesystem.md).

## Configuration Sections

| Section | Product responsibility |
|---|---|
| System | Schema, manufacturing identity, relay policy, indicators |
| Network | Network enablement, host name, recovery access point |
| Wi-Fi | Ordered profiles, credentials, DHCP/static IPv4 |
| Ethernet | Optional board capability; disabled when unsupported |
| KNX | Individual/group addresses and channel behavior |
| Modbus | Unit ID and UART framing |
| UI | Web enablement and security-provisioning state |

The current schema is `CFG-4`. Schema readers explicitly migrate supported older records or reject them safely. See [Compatibility](compatibility.md).

## Secrets

Wi-Fi passphrases are configuration secrets and must never appear in diagnostics, logs, or ordinary API responses. Web users, password verifiers, signing keys, certificates, and private keys live in the protected web-security store rather than the general configuration record.

## Reset

Factory reset replaces user-controlled sections with validated safe defaults while preserving manufacturing identity and factory security identity. The complete remove/preserve contract is [Factory reset](../manufacturing/factory-reset.md).

## Change Effects

Changes that affect active transports, including Modbus framing, KNX configuration, and web enablement, report when a controlled restart is required. Relay commands and runtime role changes remain separate from persistent configuration unless the relevant product contract explicitly states otherwise.
