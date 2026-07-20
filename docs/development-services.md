# VibeReady development services

This document records shared development service endpoints. It must not contain API tokens, credentials, Cloudflare account secrets, D1 database IDs, or production-only configuration.

The machine-readable source for these values is [`config/development-services.json`](../config/development-services.json).

## Anonymous telemetry

| Setting | Development value |
| --- | --- |
| Linear issue | `CHI-37` |
| Worker base URL | `https://vibeready-telemetry.chieworks.workers.dev` |
| Health check | `https://vibeready-telemetry.chieworks.workers.dev/healthz` |
| Batch endpoint | `https://vibeready-telemetry.chieworks.workers.dev/v1/telemetry/batch` |
| Schema version | `1` |
| D1 binding | `DB` |
| Last verified | `2026-07-18` |

This is the shared endpoint for development and integration testing. It is not a declaration that the final production domain, retention policy, rate limits, monitoring, or release signing gates are complete.

### Verification

Run the remote contract check from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Test-DevelopmentTelemetry.ps1
```

The check performs the following operations:

1. Confirms `/healthz` reports the expected service, schema version, and D1 binding.
2. Posts one anonymous `app_opened` validation event using random installation and session UUIDs.
3. Confirms the Worker rejects a request containing the prohibited `full_path` field.

The successful batch response is also a D1 write-path check: the Worker awaits `DB.batch(...)` and returns a service error if the database write fails.

### Dependency usage

- `CHI-38` client development: set `VIBEREADY_TELEMETRY_ENDPOINT` to the batch endpoint, or pass the same URL to `scripts/build-release.ps1 -TelemetryEndpoint`.
- `CHI-39` privacy and funnel work: use schema version `1` and the `events` table contract in `services/telemetry-worker/migrations/0001_events.sql`. Production D1 queries still require separately authorized Cloudflare access.
- `CHI-40` development release checks: build with the batch endpoint and confirm `dist/release-manifest.json` reports `telemetry_endpoint_configured=true`. Do not treat this development URL as completion of production domain, signing, or QA requirements.

Build a development integration package directly from the machine-readable configuration:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-release.ps1 -UseDevelopmentTelemetry
```

The generated release manifest records both `telemetry_endpoint_configured=true` and the selected public endpoint. External production builds should pass their approved endpoint explicitly and must not rely on the development switch.

Example for the current PowerShell process:

```powershell
$services = Get-Content -Raw .\config\development-services.json | ConvertFrom-Json
$env:VIBEREADY_TELEMETRY_ENDPOINT = $services.telemetry.batch_endpoint
```
