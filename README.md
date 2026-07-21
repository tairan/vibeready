# VibeReady

VibeReady is a multi-project repository for the native Windows client and its supporting services.

## Repository layout

- `apps/windows-client/` — native Windows x64 portable client.
- `apps/website/` — reserved location for the future official website.
- `services/telemetry-worker/` — Cloudflare Worker and D1 telemetry service.
- `docs/` — repository-wide product, development, and release documentation.
- `specs/` and `telemetry/` — shared contracts, samples, and telemetry queries.
- `scripts/` — repository-wide build, validation, and release entrypoints.

## Validation

Run the complete local validation flow from the repository root:

```powershell
.\scripts\test.ps1
```

Windows build artifacts remain under `build/windows-x64/`, and release artifacts remain under `dist/`.
