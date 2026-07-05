param(
    [string]$ZipPath,
    [switch]$SkipSmoke
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $ZipPath) {
    $ZipPath = Join-Path $repoRoot "dist\VibeReady-Windows-x64.zip"
}
if (-not (Test-Path -LiteralPath $ZipPath)) {
    throw "Missing ZIP: $ZipPath"
}

$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("VibeReadyPackage-" + [System.Guid]::NewGuid().ToString("N"))
$extractPath = Join-Path $testRoot "extract"
$appDataDir = Join-Path $testRoot "appdata"

try {
    New-Item -ItemType Directory -Path $extractPath -Force | Out-Null
    New-Item -ItemType Directory -Path $appDataDir -Force | Out-Null
    Expand-Archive -LiteralPath $ZipPath -DestinationPath $extractPath

    $required = @("VibeReady.exe", "README.txt", "THIRD-PARTY-NOTICES.txt")
    foreach ($item in $required) {
        $path = Join-Path $extractPath $item
        if (-not (Test-Path -LiteralPath $path)) {
            throw "ZIP is missing $item"
        }
    }

    $exePath = Join-Path $extractPath "VibeReady.exe"
    $result = [ordered]@{
        ZipPath = (Resolve-Path -LiteralPath $ZipPath).Path
        ExtractPath = $extractPath
        RequiredFiles = $required
    }

    if (-not $SkipSmoke) {
        $smoke = & (Join-Path $PSScriptRoot "test-smoke.ps1") -ExePath $exePath -AppDataDir $appDataDir -VerifyUiControls -VerifyResponsiveness -VerifyNavigation
        $result.Smoke = $smoke
    }

    [pscustomobject]$result
} finally {
    if (Test-Path -LiteralPath $testRoot) {
        Get-Process -Name "VibeReady" -ErrorAction SilentlyContinue | Stop-Process -Force
        for ($attempt = 1; $attempt -le 20; $attempt++) {
            try {
                Remove-Item -LiteralPath $testRoot -Recurse -Force
                break
            } catch {
                if ($attempt -eq 20) {
                    throw
                }
                [System.GC]::Collect()
                [System.GC]::WaitForPendingFinalizers()
                Start-Sleep -Milliseconds 500
            }
        }
    }
}
