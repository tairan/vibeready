param(
    [string]$Configuration = "Release",
    [string]$TelemetryEndpoint = $env:VIBEREADY_TELEMETRY_ENDPOINT,
    [switch]$UseDevelopmentTelemetry,
    [switch]$Sign,
    [switch]$RequireSignature,
    [string]$TimestampUrl = "http://timestamp.digicert.com",
    [string]$SignToolPath = $env:VIBEREADY_SIGNTOOL_PATH,
    [string]$CertificatePath = $env:VIBEREADY_SIGN_CERT_PATH,
    [string]$CertificatePassword = $env:VIBEREADY_SIGN_CERT_PASSWORD,
    [string]$CertificateThumbprint = $env:VIBEREADY_SIGN_CERT_THUMBPRINT
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$developmentServicesPath = Join-Path $repoRoot "config\development-services.json"
$buildDir = Join-Path $repoRoot "build\windows-x64"
$packageRoot = Join-Path $repoRoot "dist\VibeReady-Windows-x64"
$zipPath = Join-Path $repoRoot "dist\VibeReady-Windows-x64.zip"
$manifestPath = Join-Path $repoRoot "dist\release-manifest.json"

if ($UseDevelopmentTelemetry -and $PSBoundParameters.ContainsKey("TelemetryEndpoint")) {
    throw "UseDevelopmentTelemetry and TelemetryEndpoint are mutually exclusive."
}
if ($UseDevelopmentTelemetry) {
    if (-not (Test-Path -LiteralPath $developmentServicesPath)) {
        throw "Development services config not found: $developmentServicesPath"
    }
    $developmentServices = Get-Content -Raw -Encoding UTF8 -LiteralPath $developmentServicesPath | ConvertFrom-Json
    $TelemetryEndpoint = [string]$developmentServices.telemetry.batch_endpoint
    if ([string]::IsNullOrWhiteSpace($TelemetryEndpoint)) {
        throw "Development telemetry batch endpoint is missing from: $developmentServicesPath"
    }
}

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

function Find-SignTool {
    if ($SignToolPath -and (Test-Path -LiteralPath $SignToolPath)) {
        return (Resolve-Path -LiteralPath $SignToolPath).Path
    }

    $roots = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "${env:ProgramFiles}\Windows Kits\10\bin"
    )

    foreach ($root in $roots) {
        if (-not (Test-Path -LiteralPath $root)) {
            continue
        }
        $candidate = Get-ChildItem -Path $root -Filter "signtool.exe" -Recurse -ErrorAction SilentlyContinue |
            Sort-Object -Property FullName -Descending |
            Select-Object -First 1
        if ($candidate) {
            return $candidate.FullName
        }
    }

    return $null
}

function Invoke-CodeSigning {
    param([string]$Path)

    if (-not $Sign -and -not $RequireSignature) {
        return "not_requested"
    }

    $signtool = Find-SignTool
    if (-not $signtool) {
        if ($RequireSignature) {
            throw "Signing is required, but signtool.exe was not found. Set VIBEREADY_SIGNTOOL_PATH or install Windows SDK."
        }
        Write-Warning "Signing requested, but signtool.exe was not found. Build will remain unsigned."
        return "signtool_missing"
    }

    $signArgs = @("sign", "/fd", "SHA256", "/tr", $TimestampUrl, "/td", "SHA256")
    if ($CertificateThumbprint) {
        $signArgs += @("/sha1", $CertificateThumbprint)
    } elseif ($CertificatePath) {
        if (-not (Test-Path -LiteralPath $CertificatePath)) {
            throw "Signing certificate file not found: $CertificatePath"
        }
        $signArgs += @("/f", $CertificatePath)
        if ($CertificatePassword) {
            $signArgs += @("/p", $CertificatePassword)
        }
    } else {
        if ($RequireSignature) {
            throw "Signing is required, but no certificate was configured. Set VIBEREADY_SIGN_CERT_THUMBPRINT or VIBEREADY_SIGN_CERT_PATH."
        }
        Write-Warning "Signing requested, but no certificate was configured. Build will remain unsigned."
        return "certificate_not_configured"
    }

    Invoke-Native $signtool ($signArgs + @($Path))
    return "signed"
}

function Get-ProjectVersion {
    $cmakePath = Join-Path $repoRoot "CMakeLists.txt"
    $cmake = Get-Content -Raw -Encoding UTF8 -LiteralPath $cmakePath
    $match = [regex]::Match($cmake, 'project\(\s*VibeReady\s+VERSION\s+([0-9]+(?:\.[0-9]+){1,3})')
    if (-not $match.Success) {
        throw "Could not read project version from CMakeLists.txt."
    }
    return $match.Groups[1].Value
}

function Get-GitCommit {
    $git = Get-Command "git" -ErrorAction SilentlyContinue
    if (-not $git) {
        return $null
    }
    $commit = & git -C $repoRoot rev-parse --short HEAD 2>$null
    if ($LASTEXITCODE -ne 0) {
        return $null
    }
    return $commit
}

function Write-ReleaseManifest {
    param(
        [string]$PackagedExePath,
        [string]$SigningAction
    )

    $signature = Get-AuthenticodeSignature -FilePath $PackagedExePath
    $zipHash = Get-FileHash -Algorithm SHA256 -LiteralPath $zipPath
    $exeHash = Get-FileHash -Algorithm SHA256 -LiteralPath $PackagedExePath
    $version = Get-ProjectVersion
    $manifest = [ordered]@{
        app_version = $version
        exe_product_version_expected = $version
        platform = "windows-x64"
        package_name = "VibeReady-Windows-x64.zip"
        package_path = $zipPath
        package_sha256 = $zipHash.Hash
        exe_sha256 = $exeHash.Hash
        git_commit = Get-GitCommit
        telemetry_endpoint_configured = -not [string]::IsNullOrWhiteSpace($TelemetryEndpoint)
        telemetry_endpoint = $TelemetryEndpoint
        signing_action = $SigningAction
        signature_status = $signature.Status.ToString()
        signed_for_external_release = $signature.Status -eq "Valid"
        files = @(
            "VibeReady.exe",
            "README.txt",
            "THIRD-PARTY-NOTICES.txt"
        )
    }
    $manifest | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath $manifestPath
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

Invoke-Native "cmake" (@(
    "-S",
    $repoRoot,
    "-B",
    $buildDir,
    "-DVIBEREADY_TELEMETRY_ENDPOINT=$TelemetryEndpoint"
) + $generatorArgs)
Invoke-Native "cmake" @("--build", $buildDir, "--config", $Configuration)

$exeCandidates = @(
    (Join-Path $buildDir "VibeReady.exe"),
    (Join-Path $buildDir "$Configuration\VibeReady.exe")
)

$exePath = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exePath) {
    throw "Build completed but VibeReady.exe was not found."
}

$signingAction = Invoke-CodeSigning -Path $exePath

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
Write-ReleaseManifest -PackagedExePath (Join-Path $packageRoot "VibeReady.exe") -SigningAction $signingAction

Write-Host "Created $zipPath"
Write-Host "Created $manifestPath"
