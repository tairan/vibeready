param(
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build\windows-x64"
$packageRoot = Join-Path $repoRoot "dist\VibeReady-Windows-x64"
$zipPath = Join-Path $repoRoot "dist\VibeReady-Windows-x64.zip"

function Import-CmdEnvironment {
    param([string]$BatchFile)

    if (-not (Test-Path $BatchFile)) {
        throw "Batch file not found: $BatchFile"
    }

    $environment = cmd.exe /s /c "`"$BatchFile`" >nul && set"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to import environment from: $BatchFile"
    }

    foreach ($line in $environment) {
        $separator = $line.IndexOf("=")
        if ($separator -le 0) {
            continue
        }

        $name = $line.Substring(0, $separator)
        $value = $line.Substring($separator + 1)
        Set-Item -Path "Env:$name" -Value $value
    }
}

function Find-VcVars64 {
    $vswhereCandidates = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
    )

    foreach ($vswhere in $vswhereCandidates) {
        if (-not (Test-Path $vswhere)) {
            continue
        }

        $installationPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($LASTEXITCODE -eq 0 -and $installationPath) {
            $candidate = Join-Path $installationPath "VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path $candidate) {
                return $candidate
            }
        }
    }

    $candidates = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    return $null
}

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

if (-not (Get-Command "cl" -ErrorAction SilentlyContinue)) {
    $vcvars64 = Find-VcVars64
    if (-not $vcvars64) {
        throw "MSVC x64 compiler environment was not found. Install or enable Visual Studio Build Tools with MSVC."
    }

    Import-CmdEnvironment $vcvars64
}

Require-Command "cl"
Require-Command "link"

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
