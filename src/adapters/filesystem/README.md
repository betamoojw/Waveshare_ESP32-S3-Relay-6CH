# LittleFS Adapter

`LittleFsConfigurationSource` owns the firmware's LittleFS mount and provides a
read-only `ConfigurationSource` to the application. It does not auto-format a
failed filesystem because mount failure must not silently erase recoverable
field data.

Configuration lookup order is:

1. the complete `/config/*.json` bundle
2. the complete `/config/.backup/*.json` bundle when the primary is absent or invalid
3. the immutable embedded `config/default_configuration.json`

Each section is limited to 8192 bytes. The adapter assembles all seven sections
and passes the resulting document through the same strict JSON and domain
validation used for the embedded default. A bundle is accepted only when every
required file is present and valid. A valid primary bundle refreshes the hidden
backup; a valid backup is restored with per-file temporary replacements. An
invalid or incomplete bundle is never partially applied.

`ethernet.json` must currently contain `"enabled": false` because the target BSP
does not provide Ethernet and the domain has no active Ethernet configuration.

Mutable runtime configuration remains in the NVS A/B store. LittleFS is a
recoverable deployment-default and future static-asset layer, not a second
mutable settings database. Filesystem paths are not exposed through HTTP.

The maintenance CLI can explicitly load the primary bundle or store the active
configuration through `ConfigurationFilePort`. Storage uses a staging bundle,
validation through the normal loader, backup of the prior primary, and per-file
temporary replacement. Wi-Fi credentials are necessarily serialized in
plaintext; callers must treat the bundle as sensitive deployment data.