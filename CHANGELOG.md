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
