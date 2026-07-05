# M5 QA Matrix

This matrix separates automated local gates from external release gates.

| Area | Gate | Command | Expected result | Owner |
| --- | --- | --- | --- | --- |
| Telemetry privacy | Schema, redaction cases, accepted/rejected samples, funnel SQL | `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Validate-M5-TelemetryPrivacy.ps1` | Passes without sensitive fields or unknown events | Local |
| Cloudflare Worker contract | Worker routes, D1 migration, request validation | `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Validate-M5-Worker.ps1` and `node --test .\telemetry-worker\test\worker-contract.test.mjs` | Health and batch endpoint contract passes | Local |
| Client telemetry | Consent guard, local queue cap, HTTPS/localhost endpoint, event hooks | `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Validate-M5-ClientTelemetry.ps1` | Consent-off path sends no events and clears queue | Local |
| Release ZIP | Build, ZIP content, manifest hashes, EXE version resource | `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test.ps1 -SkipSmoke` | `dist\VibeReady-Windows-x64.zip` and `dist\release-manifest.json` validate | Local |
| External signing | Authenticode certificate and timestamped signature | `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-release.ps1 -Sign -RequireSignature` | Packaged EXE has valid signature | Release owner |
| Clean Windows 10 x64 | First run, opt-in telemetry, scan, repair/manual, verification | Run ZIP on clean Windows 10 x64 VM | No crashes; no telemetry when consent off; Ready path works when tools are available | QA |
| Clean Windows 11 x64 | First run, opt-in telemetry, scan, repair/manual, verification | Run ZIP on clean Windows 11 x64 VM | No crashes; no telemetry when consent off; Ready path works when tools are available | QA |
| Cloudflare production | D1 DB, Worker deploy, migration, endpoint smoke | `wrangler d1 migrations apply` and `wrangler deploy` in `telemetry-worker` | `/healthz` returns ok and `/v1/telemetry/batch` persists accepted events | Infra owner |

External release remains blocked until a production Cloudflare account/project and code-signing certificate are provided.
