param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$workerRoot = Join-Path $repoRoot "telemetry-worker"
$developmentServicesPath = Join-Path $repoRoot "config\development-services.json"
$developmentServicesDocPath = Join-Path $repoRoot "docs\development-services.md"
$developmentTelemetryTestPath = Join-Path $repoRoot "scripts\Test-DevelopmentTelemetry.ps1"
$requiredFiles = @(
    "package.json",
    "wrangler.jsonc",
    "src\index.ts",
    "migrations\0001_events.sql",
    "test\worker-contract.test.mjs"
)

foreach ($relativePath in $requiredFiles) {
    $path = Join-Path $workerRoot $relativePath
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing Worker file: $relativePath"
    }
}

foreach ($path in @($developmentServicesPath, $developmentServicesDocPath, $developmentTelemetryTestPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing development telemetry reference: $path"
    }
}

$source = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $workerRoot "src\index.ts")
$migration = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $workerRoot "migrations\0001_events.sql")
$wrangler = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $workerRoot "wrangler.jsonc")

foreach ($snippet in @(
    "/healthz",
    "/v1/telemetry/batch",
    "MAX_BODY_BYTES",
    "MAX_EVENTS_PER_BATCH",
    "MAX_EVENT_BYTES",
    "content-type must be application/json",
    "consent_state must be enabled",
    "env.DB",
    "db.batch",
    "prepare("
)) {
    if (-not $source.Contains($snippet)) {
        throw "Worker validation failed. Missing snippet: $snippet"
    }
}

foreach ($eventName in @("app_opened", "scan_started", "scan_completed", "repair_started", "repair_completed", "verification_completed", "ready_reached")) {
    if (-not $source.Contains($eventName)) {
        throw "Worker validation failed. Missing telemetry event: $eventName"
    }
}

foreach ($term in @("full_path", "folder_path", "username", "email", "token", "raw_log")) {
    if (-not $source.Contains($term)) {
        throw "Worker validation failed. Missing sensitive-field rejection term: $term"
    }
}

foreach ($snippet in @(
    "CREATE TABLE IF NOT EXISTS events",
    "event_name TEXT NOT NULL",
    "app_version TEXT NOT NULL",
    "installation_id TEXT NOT NULL",
    "idx_events_event_time",
    "idx_events_version_event_time",
    "idx_events_error_time"
)) {
    if (-not $migration.Contains($snippet)) {
        throw "Worker migration validation failed. Missing snippet: $snippet"
    }
}

if (-not $wrangler.Contains('"binding": "DB"')) {
    throw "Worker wrangler config must bind D1 as DB."
}

$developmentServices = Get-Content -Raw -Encoding UTF8 -LiteralPath $developmentServicesPath | ConvertFrom-Json
$developmentTelemetry = $developmentServices.telemetry
$expectedDevelopmentBaseUrl = "https://vibeready-telemetry.chieworks.workers.dev"
if ([string]$developmentServices.environment -ne "development") {
    throw "Development services config must identify the development environment."
}
if ([string]$developmentTelemetry.worker_base_url -ne $expectedDevelopmentBaseUrl) {
    throw "Unexpected development telemetry Worker URL."
}
if ([string]$developmentTelemetry.health_endpoint -ne "$expectedDevelopmentBaseUrl/healthz") {
    throw "Unexpected development telemetry health endpoint."
}
if ([string]$developmentTelemetry.batch_endpoint -ne "$expectedDevelopmentBaseUrl/v1/telemetry/batch") {
    throw "Unexpected development telemetry batch endpoint."
}
if ([string]$developmentTelemetry.schema_version -ne "1" -or [string]$developmentTelemetry.d1_binding -ne "DB") {
    throw "Development telemetry schema or D1 binding does not match the Worker contract."
}

Write-Output "M5 Worker validation passed."
