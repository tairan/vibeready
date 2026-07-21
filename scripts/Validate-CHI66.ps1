param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$mainPath = Join-Path $repoRoot "apps\windows-client\src\main.cpp"
$flowPath = Join-Path $repoRoot "docs\mvp-page-flow.md"

if (-not (Test-Path -LiteralPath $mainPath)) {
    throw "Missing source file: $mainPath"
}
if (-not (Test-Path -LiteralPath $flowPath)) {
    throw "Missing flow document: $flowPath"
}

$source = Get-Content -Raw -Encoding UTF8 -LiteralPath $mainPath
$flow = Get-Content -Raw -Encoding UTF8 -LiteralPath $flowPath

$requiredSourceSnippets = @(
    "AppRoute::Settings",
    "UiControl::BackButton",
    "settingsDraft",
    "settingsReturnRoute",
    "manualReturnRoute",
    "EnterSettings",
    "CanNavigateBack",
    "BackLabel",
    "NavigateBack",
    "VK_ESCAPE",
    "VK_MENU",
    "KillTimer(g_state.window, kScanTimerId)",
    "g_state.scanningActive = false",
    "g_state.scanStep = 0",
    "scan_canceled_by_navigation"
)

foreach ($snippet in $requiredSourceSnippets) {
    if (-not $source.Contains($snippet)) {
        throw "CHI-66 validation failed. Missing source snippet: $snippet"
    }
}

$requiredPatterns = @(
    "case AppRoute::FixProgress:\s*return !g_state\.repair\.active;",
    "case AppRoute::Verifying:\s*return false;",
    "wparam == VK_ESCAPE \|\| \(\(GetKeyState\(VK_MENU\) & 0x8000\) && wparam == VK_LEFT\)",
    "case UiControl::BackButton:\s*if \(NavigateBack\(\)\)",
    "case AppRoute::Settings:\s*g_state\.route = g_state\.settingsReturnRoute;",
    "g_state\.route == AppRoute::Settings \? g_state\.settingsDraft : g_state\.preferences"
)

foreach ($pattern in $requiredPatterns) {
    if (-not [regex]::IsMatch($source, $pattern)) {
        throw "CHI-66 validation failed. Missing source pattern: $pattern"
    }
}

$settingsBranchMatch = [regex]::Match($source, "case UiControl::SettingsButton:[\s\S]*?case UiControl::TechnicalDetailsButton:")
if (-not $settingsBranchMatch.Success) {
    throw "CHI-66 validation failed. Could not locate SettingsButton branch."
}
if ($settingsBranchMatch.Value.Contains("EnterLanguageTelemetry(")) {
    throw "CHI-66 validation failed. Settings button still enters first-run language telemetry."
}

$requiredFlowSnippets = @(
    "## Navigation Rules",
    '`home` is the navigation root',
    '`scanning` is read-only',
    'Active `fix_progress` and active `verifying` do not allow Back',
    "Back or Cancel returns to the source screen without applying draft language, telemetry, or theme changes"
)

foreach ($snippet in $requiredFlowSnippets) {
    if (-not $flow.Contains($snippet)) {
        throw "CHI-66 validation failed. Missing flow documentation snippet: $snippet"
    }
}

Write-Output "CHI-66 validation passed."
