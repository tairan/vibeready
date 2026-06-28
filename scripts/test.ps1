param(
    [switch]$SkipBuild,
    [switch]$SkipSmoke
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot

& (Join-Path $PSScriptRoot "validate-rules.ps1")

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build-release.ps1")
}

& (Join-Path $PSScriptRoot "test-package.ps1") -SkipSmoke:$SkipSmoke

$ctestFile = Join-Path $repoRoot "build\windows-x64\CTestTestfile.cmake"
if (Test-Path -LiteralPath $ctestFile) {
    ctest --test-dir (Join-Path $repoRoot "build\windows-x64") --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "CTest failed with exit code $LASTEXITCODE."
    }
} else {
    Write-Host "CTest not configured; skipped."
}
