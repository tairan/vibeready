param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$mainPath = Join-Path $repoRoot "src\windows\main.cpp"
$telemetryHeaderPath = Join-Path $repoRoot "src\windows\telemetry.h"
$telemetrySourcePath = Join-Path $repoRoot "src\windows\telemetry.cpp"
$cmakePath = Join-Path $repoRoot "CMakeLists.txt"
$testScriptPath = Join-Path $repoRoot "scripts\test.ps1"
$qaMatrixPath = Join-Path $repoRoot "scripts\run-qa-matrix.ps1"

foreach ($path in @($mainPath, $telemetryHeaderPath, $telemetrySourcePath, $cmakePath, $testScriptPath, $qaMatrixPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing client telemetry file: $path"
    }
}

$main = Get-Content -Raw -Encoding UTF8 -LiteralPath $mainPath
$header = Get-Content -Raw -Encoding UTF8 -LiteralPath $telemetryHeaderPath
$source = Get-Content -Raw -Encoding UTF8 -LiteralPath $telemetrySourcePath
$cmake = Get-Content -Raw -Encoding UTF8 -LiteralPath $cmakePath
$testScript = Get-Content -Raw -Encoding UTF8 -LiteralPath $testScriptPath
$qaMatrix = Get-Content -Raw -Encoding UTF8 -LiteralPath $qaMatrixPath

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
    "kMaxEventBytes = 8 * 1024",
    "consentEnabled",
    "ClearQueue();",
    "WinHttpCrackUrl",
    'host == L"127.0.0.1"',
    'host == L"localhost"',
    "status == 408",
    "status == 429",
    "kInitialRetryDelayMs",
    "Sleep(retryDelayMs)",
    "CreateThread",
    "WinHttpSendRequest",
    "HasDisallowedFieldName",
    "HasSensitiveValue",
    "Telemetry must never terminate or alter the main application flow"
)) {
    if (-not $source.Contains($snippet)) {
        throw "M5 client telemetry validation failed. telemetry.cpp missing snippet: $snippet"
    }
}

$telemetryTestPath = Join-Path $repoRoot "tests\windows\telemetry_contract.cpp"
$transportTestPath = Join-Path $repoRoot "scripts\Test-M5-ClientTelemetryTransport.ps1"
foreach ($path in @($telemetryTestPath, $transportTestPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "M5 client telemetry validation failed. Missing runtime contract test: $path"
    }
}
$telemetryTest = Get-Content -Raw -Encoding UTF8 -LiteralPath $telemetryTestPath
foreach ($snippet in @(
    "Consent-off telemetry created a queue file.",
    "Telemetry queue exceeded the 200-event limit.",
    "Telemetry queue exceeded the 256 KiB limit.",
    "Oversized telemetry was added to the queue.",
    "Disabling consent did not clear the telemetry queue.",
    "Telemetry operation escaped into the application flow"
)) {
    if (-not $telemetryTest.Contains($snippet)) {
        throw "M5 client telemetry validation failed. Contract test missing assertion: $snippet"
    }
}
if ($telemetryTest.Contains("FlushAsync();")) {
    throw "M5 client telemetry validation failed. Transport contract must verify automatic retry without manual flushing."
}

$transportTest = Get-Content -Raw -Encoding UTF8 -LiteralPath $transportTestPath
foreach ($snippet in @(
    "first_response_status = 429",
    "accepted_events",
    "max_batch_events",
    "Configuration",
    "VibeReadyTelemetryTests.exe",
    'build\windows-x64\$Configuration\VibeReadyTelemetryTests.exe',
    "Expected 45 accepted telemetry events after retry",
    "Telemetry batch exceeded the 20-event client limit"
)) {
    if (-not $transportTest.Contains($snippet)) {
        throw "M5 client telemetry validation failed. Transport test missing assertion: $snippet"
    }
}

foreach ($snippet in @("-C `$Configuration", "Test-M5-ClientTelemetryTransport.ps1")) {
    if (-not $testScript.Contains($snippet)) {
        throw "M5 client telemetry validation failed. test.ps1 does not select and execute the configured runtime tests: $snippet"
    }
}
foreach ($snippet in @(
    "client_telemetry_runtime",
    "client_telemetry_transport",
    "ctest --test-dir",
    "Test-M5-ClientTelemetryTransport.ps1"
)) {
    if (-not $qaMatrix.Contains($snippet)) {
        throw "M5 client telemetry validation failed. QA matrix does not execute runtime telemetry gate: $snippet"
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
