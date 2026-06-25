param(
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build\windows-x64"
$packageRoot = Join-Path $repoRoot "dist\VibeReady-Windows-x64"
$zipPath = Join-Path $repoRoot "dist\VibeReady-Windows-x64.zip"

function Require-Command {
    param([string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $command) {
        throw "Required command not found: $Name"
    }
}

function Invoke-Native {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $FilePath $($Arguments -join ' ')"
    }
}

if (-not [Environment]::Is64BitOperatingSystem) {
    throw "VibeReady release builds require a Windows x64 operating system."
}

Require-Command "cmake"

$generatorArgs = @()
if (Get-Command "ninja" -ErrorAction SilentlyContinue) {
    $generatorArgs = @("-G", "Ninja", "-DCMAKE_BUILD_TYPE=$Configuration")
}

Invoke-Native "cmake" (@("-S", $repoRoot, "-B", $buildDir) + $generatorArgs)
Invoke-Native "cmake" @("--build", $buildDir, "--config", $Configuration)

$exeCandidates = @(
    (Join-Path $buildDir "VibeReady.exe"),
    (Join-Path $buildDir "$Configuration\VibeReady.exe")
)

$exePath = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exePath) {
    throw "Build completed but VibeReady.exe was not found."
}

if (Test-Path $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $packageRoot | Out-Null

Copy-Item -LiteralPath $exePath -Destination (Join-Path $packageRoot "VibeReady.exe")
Copy-Item -LiteralPath (Join-Path $repoRoot "README.txt") -Destination (Join-Path $packageRoot "README.txt")
Copy-Item -LiteralPath (Join-Path $repoRoot "THIRD-PARTY-NOTICES.txt") -Destination (Join-Path $packageRoot "THIRD-PARTY-NOTICES.txt")

if (Test-Path $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

Compress-Archive -Path (Join-Path $packageRoot "*") -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "Created $zipPath"
