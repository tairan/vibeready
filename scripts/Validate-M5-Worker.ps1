param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$workerRoot = Join-Path $repoRoot "telemetry-worker"
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

Write-Output "M5 Worker validation passed."
