# KNX/IP Adapter

The adapter uses `ESP32_KNX_IP_Library_fix2bytefloat` in KNXnet/IP routing mode over UDP multicast `224.0.23.12:3671`.

## Provisioning

- Provision Wi-Fi credentials in the ESP32 network stack before deployment. The adapter starts station mode with the stored credentials and enables automatic reconnect.
- Set `knx.enabled` to `true` in the active configuration.
- Store the KNX individual address as the standard packed 16-bit value: `area << 12 | line << 8 | device`.
- Store group addresses as standard packed 16-bit three-level values: `main << 11 | middle << 8 | sub`.
- A zero group address disables that binding in schema v2. This compatibility representation cannot assign the valid KNX address `0/0/0`; a future ETS association model must represent "unassigned" separately.
- Each channel supports DPT 1.001 switch and applied-state objects plus an optional DPT 1.005 fault object.
- Optional device-wide bindings provide central switch, central off, device fault, and device-in-operation heartbeat objects.
- Startup publication delay, minimum inter-telegram interval, cyclic status, command/status polarity, command reads, and central participation are fixed-capacity configuration values.

Inbound channel writes and atomic central batches are queued through `RelayCommandQueue`. Read responses and outbound status writes use the authoritative applied state from `RelayCommandService`. Spontaneous publications are paced and produce at most one telegram per adapter poll. The library EEPROM and built-in configuration web server are intentionally not used; the firmware configuration service remains the only owner of KNX bindings.

The selected library has a fixed limit of ten callback assignments. This implementation uses at most eight: six channel switch addresses, central switch, and central off. Scene, lock, forced-operation, logic, counters, KNX Secure, ETS product data, and tunnelling remain unexposed until a maintained stack and application-layer policies supporting them are integrated.