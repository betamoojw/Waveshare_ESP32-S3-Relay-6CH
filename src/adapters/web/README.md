# Web Adapter

Optional HTTP and live-update adapter code belongs in this directory.

The adapter must not expose a generic LittleFS editor, directory listing, or
arbitrary filesystem path. Future static assets are restricted to an explicit
allowlist under `/www/`; `/config/`, `/config/.backup/`, and NVS credentials are
never web-readable. Essential pages should retain a compiled
fallback when filesystem assets are unavailable. See
`design/filesystem-architecture.md`.