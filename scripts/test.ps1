param(
    [switch]$SkipBuild,
    [switch]$SkipSmoke
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot

& (Join-Path $PSScriptRoot "validate-rules.ps1")
& (Join-Path $PSScriptRoot "Validate-M3.ps1")
& (Join-Path $PSScriptRoot "Validate-M4.ps1")
& (Join-Path $PSScriptRoot "Validate-CHI66.ps1")
& (Join-Path $PSScriptRoot "Validate-M5-TelemetryPrivacy.ps1")
& (Join-Path $PSScriptRoot "Validate-M5-Worker.ps1")
node --test (Join-Path $repoRoot "telemetry-worker\test\worker-contract.test.mjs")
if ($LASTEXITCODE -ne 0) {
    throw "M5 Worker node contract tests failed with exit code $LASTEXITCODE."
}
& (Join-Path $PSScriptRoot "Validate-M5-ClientTelemetry.ps1")

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build-release.ps1")
}

& (Join-Path $PSScriptRoot "test-package.ps1") -SkipSmoke:$SkipSmoke
& (Join-Path $PSScriptRoot "Validate-M5-Release.ps1")

$ctestFile = Join-Path $repoRoot "build\windows-x64\CTestTestfile.cmake"
if (Test-Path -LiteralPath $ctestFile) {
    ctest --test-dir (Join-Path $repoRoot "build\windows-x64") --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "CTest failed with exit code $LASTEXITCODE."
    }
} else {
    Write-Host "CTest not configured; skipped."
}
