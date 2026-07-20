# M5 QA Matrix

This matrix separates automated local gates from external release gates.

| Area | Gate | Command | Expected result | Owner |
| --- | --- | --- | --- | --- |
| Telemetry privacy and queries | Schema, redaction cases, accepted/rejected samples, seeded local D1 funnel/error/conversion SQL | `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Validate-M5-TelemetryPrivacy.ps1` and `scripts\Test-M5-TelemetryQueries.ps1` | No sensitive fields or unknown events; all seven funnel stages aggregate; `VERIFY_FAILED` and Ready conversion return expected values | Local |
| Cloudflare Worker contract | Worker routes, D1 migration, request validation | `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Validate-M5-Worker.ps1` and `node --test .\services\telemetry-worker\test\worker-contract.test.mjs` | Health and batch endpoint contract passes | Local |
| Client telemetry | Consent guard, local queue cap, HTTPS/localhost endpoint, 429 retry, 20-event batches, event hooks | `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Validate-M5-ClientTelemetry.ps1`, CTest `VibeReadyTelemetryContract`, and `scripts\Test-M5-ClientTelemetryTransport.ps1` | Consent-off sends no events; queue is bounded and cleared; 45 events drain after a mock 429 in batches no larger than 20; telemetry failures do not escape into the app flow | Local |
| Release ZIP | Build, ZIP content, manifest hashes, EXE version resource, selected telemetry endpoint | `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-release.ps1 -UseDevelopmentTelemetry` followed by `scripts\Validate-M5-Release.ps1` | ZIP and manifest validate; development integration manifest records the selected HTTPS endpoint | Local |
| External signing | Authenticode certificate and timestamped signature | `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-release.ps1 -Sign -RequireSignature` | Packaged EXE has valid signature | Release owner |
| Clean Windows 10 x64 | First run, opt-in telemetry, scan, repair/manual, verification | Run ZIP on clean Windows 10 x64 VM | No crashes; no telemetry when consent off; Ready path works when tools are available | QA |
| Clean Windows 11 x64 | First run, opt-in telemetry, scan, repair/manual, verification | Run ZIP on clean Windows 11 x64 VM | No crashes; no telemetry when consent off; Ready path works when tools are available | QA |
| Cloudflare development | Deployed Worker, D1 binding, accepted event, privacy rejection | `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Test-DevelopmentTelemetry.ps1` or full `scripts/run-qa-matrix.ps1` | Development `/healthz` reports schema v1 and D1; batch accepts one anonymous event; prohibited `full_path` is rejected | Automated remote check |
| Cloudflare production | Production domain, retention, rate limits, monitoring, endpoint smoke | Production deployment evidence and the same remote contract check against the production configuration | `/healthz` returns ok and `/v1/telemetry/batch` persists accepted events | Infra owner |

External release remains blocked until a production Cloudflare account/project and code-signing certificate are provided.
