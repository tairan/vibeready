param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$mainPath = Join-Path $repoRoot "src\windows\main.cpp"
$telemetryHeaderPath = Join-Path $repoRoot "src\windows\telemetry.h"
$telemetrySourcePath = Join-Path $repoRoot "src\windows\telemetry.cpp"
$cmakePath = Join-Path $repoRoot "CMakeLists.txt"

foreach ($path in @($mainPath, $telemetryHeaderPath, $telemetrySourcePath, $cmakePath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing client telemetry file: $path"
    }
}

$main = Get-Content -Raw -Encoding UTF8 -LiteralPath $mainPath
$header = Get-Content -Raw -Encoding UTF8 -LiteralPath $telemetryHeaderPath
$source = Get-Content -Raw -Encoding UTF8 -LiteralPath $telemetrySourcePath
$cmake = Get-Content -Raw -Encoding UTF8 -LiteralPath $cmakePath

foreach ($snippet in @(
    "src/windows/telemetry.cpp",
    "src/windows/VibeReady.rc.in",
    "VIBEREADY_TELEMETRY_ENDPOINT",
    "ole32"
)) {
    if (-not $cmake.Contains($snippet)) {
        throw "M5 client telemetry validation failed. CMakeLists.txt missing snippet: $snippet"
    }
}

foreach ($snippet in @(
    "struct Context",
    "SetConsent",
    "ClearQueue",
    "Track",
    "FlushAsync",
    "NewId"
)) {
    if (-not $header.Contains($snippet)) {
        throw "M5 client telemetry validation failed. telemetry.h missing snippet: $snippet"
    }
}

foreach ($snippet in @(
    "kMaxQueueEvents = 200",
    "kMaxQueueBytes = 256 * 1024",
    "kMaxBatchEvents = 20",
    "consentEnabled",
    "ClearQueue();",
    "https://",
    "http://127.0.0.1",
    "http://localhost",
    "CreateThread",
    "WinHttpSendRequest",
    "HasDisallowedFieldName",
    "HasSensitiveValue"
)) {
    if (-not $source.Contains($snippet)) {
        throw "M5 client telemetry validation failed. telemetry.cpp missing snippet: $snippet"
    }
}

foreach ($eventName in @(
    "app_opened",
    "startup_check_completed",
    "language_selected",
    "telemetry_consent_changed",
    "scan_started",
    "tool_check_completed",
    "scan_completed",
    "repair_plan_created",
    "repair_started",
    "repair_step_completed",
    "repair_completed",
    "verification_started",
    "verification_completed",
    "ready_reached"
)) {
    if (-not $main.Contains($eventName)) {
        throw "M5 client telemetry validation failed. main.cpp missing event: $eventName"
    }
}

foreach ($snippet in @(
    "ConfigureTelemetry(true)",
    "ConfigureTelemetry(false)",
    "TrackSavedPreferences",
    "if (!source.telemetryAllowed)",
    "new_consent_state",
    "enabled",
    "TrackScanToolChecks",
    "TrackScanCompleted",
    "TrackRepairPlanCreated",
    "TrackVerificationStarted",
    "TrackReadyReached"
)) {
    if (-not $main.Contains($snippet)) {
        throw "M5 client telemetry validation failed. main.cpp missing snippet: $snippet"
    }
}

$forbiddenTelemetryValues = @(
    "detectedVersion)",
    "executablePath)",
    "projectDir)",
    "localUrl)",
    "BuildNextPromptTemplate()",
    "GetEnvironmentValue(L`"PATH`")"
)
foreach ($snippet in $forbiddenTelemetryValues) {
    $telemetryIndex = $main.IndexOf("vibeready::telemetry::Text", [System.StringComparison]::Ordinal)
    $forbiddenIndex = $main.IndexOf($snippet, [System.StringComparison]::Ordinal)
    if ($telemetryIndex -ge 0 -and $forbiddenIndex -ge 0 -and [math]::Abs($forbiddenIndex - $telemetryIndex) -lt 240) {
        throw "M5 client telemetry validation failed. Sensitive local value appears near telemetry payload: $snippet"
    }
}

if ($main.Contains('new_consent_state", L"disabled"')) {
    throw "M5 client telemetry validation failed. Consent disabled must not enqueue outbound telemetry."
}

Write-Output "M5 client telemetry validation passed."
