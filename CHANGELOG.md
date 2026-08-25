CHANGELOG

## v0.0.0

PLEASE WRITE YOUR CHANGES BEFORE YOU CREATE A RELEASE IN THE CHANGELOG.MD FILE!

- Added a WLED-informed LittleFS configuration layer that assembles the seven
	`/config/*.json` sections, performs bounded reads and complete validation,
	recovers complete backup bundles, and reports mount failures.
- Documented filesystem ownership, boot precedence, deployment, and web
	security boundaries.
- Added maintenance-only `load-config` and `store-config` CLI operations with
	staged bundle validation, NVS commit integration, and IPv4 round-trip support.
- Froze Modbus Register Map v1.0 as a product contract, including transport,
	function, exception, broadcast, persistence, side-effect, and reset behavior.
- Implemented validated UART register encoding and persistence, safe broadcast
	configuration handling, and reachable RGB/buzzer register commands.
- Froze KNX/IP Application Model v1.0 with typed object/DPT metadata, stable
	channel object blocks, explicit scene reservations, and an ETS product-data
	and certification strategy grounded in implemented firmware behavior.
- Defined and implemented identity-preserving factory reset: user configuration,
	web users, sessions, scenes, and timers are removed while manufacturing
	identity, factory security identity, diagnostic counters, firmware, and
	LittleFS deployment data are retained.
- Added explicit hardware, firmware, configuration, API, Modbus, KNX
	application, and filesystem compatibility versions with centralized firmware
	constants, serial/web discovery, release-version embedding, and bump rules.
- Reorganized normative documentation under `docs/` around architecture,
	protocol, product, manufacturing, and release lifecycles, with a canonical
	index and validated repository links.
