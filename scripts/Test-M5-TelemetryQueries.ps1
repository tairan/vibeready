param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$workerRoot = Join-Path $repoRoot "telemetry-worker"
$wranglerPath = Join-Path $workerRoot "node_modules\.bin\wrangler.cmd"
$wranglerConfigPath = Join-Path $workerRoot "wrangler.jsonc"
$queryScriptPath = Join-Path $PSScriptRoot "query-telemetry-funnel.ps1"

if (-not (Test-Path -LiteralPath $wranglerPath)) {
    throw "Wrangler is not installed. Run: npm ci --prefix `"$workerRoot`""
}

$suffix = [guid]::NewGuid().ToString("N").Substring(0, 12)
$appVersion = "0.1.0-query-test-$suffix"
$installationId = [guid]::NewGuid().ToString()
$sessionId = [guid]::NewGuid().ToString()
$occurredAt = (Get-Date).ToUniversalTime().ToString("o")
$eventNames = @(
    "app_opened",
    "scan_started",
    "scan_completed",
    "repair_started",
    "repair_completed",
    "verification_completed",
    "ready_reached"
)
$tempSql = Join-Path ([System.IO.Path]::GetTempPath()) ("VibeReadyTelemetryQuery-" + $suffix + ".sql")

try {
    $statements = foreach ($eventName in $eventNames) {
        $id = [guid]::NewGuid().ToString()
        $errorCode = if ($eventName -eq "scan_completed") { "'VERIFY_FAILED'" } else { "NULL" }
        @"
INSERT INTO events (
  id, event_name, event_version, occurred_at, received_at, app_version,
  installation_id, session_id, os_name, consent_state, error_code
) VALUES (
  '$id', '$eventName', 1, '$occurredAt', '$occurredAt', '$appVersion',
  '$installationId', '$sessionId', 'windows', 'enabled', $errorCode
);
"@
    }
    $statements -join [Environment]::NewLine | Set-Content -Encoding UTF8 -LiteralPath $tempSql

    $insertOutput = & $wranglerPath d1 execute DB `
        --local `
        --file $tempSql `
        --config $wranglerConfigPath `
        --json
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to seed the local D1 telemetry query test."
    }
    $insertResult = ($insertOutput -join [Environment]::NewLine) | ConvertFrom-Json
    if (-not $insertResult[0].success) {
        throw "Local D1 telemetry seed did not report success."
    }

    $funnelResult = & $queryScriptPath -DatabaseName DB -SinceDays 1 -Query funnel
    $funnelRow = @($funnelResult[0].results | Where-Object { $_.app_version -eq $appVersion })[0]
    if (-not $funnelRow) {
        throw "Funnel query did not return the seeded app version."
    }
    foreach ($eventName in $eventNames) {
        if ([int]$funnelRow.$eventName -ne 1) {
            throw "Funnel query count for $eventName was not 1."
        }
    }
    if ([int]$funnelRow.unique_installations -ne 1) {
        throw "Funnel query unique installation count was not 1."
    }

    $errorsResult = & $queryScriptPath -DatabaseName DB -SinceDays 1 -Query errors
    $errorRow = @($errorsResult[0].results | Where-Object {
        $_.app_version -eq $appVersion -and $_.error_code -eq "VERIFY_FAILED"
    })[0]
    if (-not $errorRow -or [int]$errorRow.event_count -ne 1) {
        throw "Error aggregation query did not return the seeded VERIFY_FAILED event."
    }

    $readyResult = & $queryScriptPath -DatabaseName DB -SinceDays 1 -Query ready
    $readyRow = @($readyResult[0].results | Where-Object { $_.app_version -eq $appVersion })[0]
    if (-not $readyRow -or [double]$readyRow.ready_reached_per_app_opened -ne 1.0) {
        throw "Ready conversion query did not return a 1.0 event conversion."
    }
    if ([double]$readyRow.ready_installation_conversion -ne 1.0) {
        throw "Ready conversion query did not return a 1.0 installation conversion."
    }

    [pscustomobject]@{
        app_version = $appVersion
        funnel_events = $eventNames.Count
        error_code = [string]$errorRow.error_code
        ready_conversion = [double]$readyRow.ready_reached_per_app_opened
    }
    Write-Output "M5 telemetry query validation passed."
} finally {
    $cleanupOutput = & $wranglerPath d1 execute DB `
        --local `
        --command "DELETE FROM events WHERE app_version = '$appVersion'" `
        --config $wranglerConfigPath `
        --json 2>$null
    if (Test-Path -LiteralPath $tempSql) {
        Remove-Item -LiteralPath $tempSql -Force
    }
}
