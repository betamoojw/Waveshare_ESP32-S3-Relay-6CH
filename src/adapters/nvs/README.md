# NVS Adapter

`NvsSettingsStore` persists configuration in two A/B records using the ESP32
Preferences API. Records use an explicit, fixed-width schema rather than raw
C++ object bytes. Each record carries magic, schema version, payload length,
generation, and CRC32 fields.

Writes target the inactive slot, read the record back for verification, and
only then update the active-slot marker. Loading validates both slots and
selects the valid record with the newest generation. A missing or corrupt slot
does not prevent recovery from the other valid slot.