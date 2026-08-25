# NVS Adapter

`NvsSettingsStore` persists configuration in two A/B records using the ESP32
Preferences API. Records use an explicit, fixed-width schema rather than raw
C++ object bytes. Each record carries magic, schema version, payload length,
generation, and CRC32 fields.

Writes target the inactive slot, read the record back for verification, and
only then update the active-slot marker. Loading validates both slots and
selects the valid record with the newest generation. A missing or corrupt slot
does not prevent recovery from the other valid slot.

The same product namespace stores persistent diagnostic counters in independent
`diag_a` and `diag_b` records. Each fixed-width record has a schema version,
generation, and CRC32; updates write and verify the inactive slot before moving
the active marker. The previous standalone `boot_count` key is migrated on the
first successful diagnostic-record write.

Boot count and watchdog/brownout reset classification are committed together
once per startup. Runtime counters accumulate in RAM and are checkpointed only
when dirty, at most once per 60 seconds, plus one final attempt before a
controlled restart. Consequently an uncontrolled power loss can discard up to
one checkpoint interval of runtime errors, while sustained errors cause no more
than one aggregate NVS update per minute. All counters saturate at `UINT32_MAX`.
Physical factory reset preserves these counters as service evidence and writes
an identity-preserving safe configuration through the normal A/B path.