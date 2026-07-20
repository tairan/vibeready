param(
    [string]$ZipPath,
    [switch]$RequireSignature
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $ZipPath) {
    $ZipPath = Join-Path $repoRoot "dist\VibeReady-Windows-x64.zip"
}
$manifestPath = Join-Path $repoRoot "dist\release-manifest.json"
$packageRoot = Join-Path $repoRoot "dist\VibeReady-Windows-x64"
$packagedExePath = Join-Path $packageRoot "VibeReady.exe"
$buildScriptPath = Join-Path $repoRoot "scripts\build-release.ps1"
$versionTemplatePath = Join-Path $repoRoot "apps\windows-client\src\version.h.in"
$resourceTemplatePath = Join-Path $repoRoot "apps\windows-client\src\VibeReady.rc.in"

foreach ($path in @($ZipPath, $manifestPath, $packagedExePath, $buildScriptPath, $versionTemplatePath, $resourceTemplatePath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing required release artifact or source: $path"
    }
}

$buildScript = Get-Content -Raw -Encoding UTF8 -LiteralPath $buildScriptPath
foreach ($snippet in @(
    "VIBEREADY_TELEMETRY_ENDPOINT",
    "UseDevelopmentTelemetry",
    "config\development-services.json",
    "release-manifest.json",
    "Get-AuthenticodeSignature",
    "RequireSignature",
    "signtool.exe"
)) {
    if (-not $buildScript.Contains($snippet)) {
        throw "M5 release validation failed. build-release.ps1 is missing snippet: $snippet"
    }
}
if ($buildScript.Contains('$UseDevelopmentTelemetry -and [string]::IsNullOrWhiteSpace($TelemetryEndpoint)')) {
    throw "M5 release validation failed. Development telemetry selection must override any ambient endpoint."
}
if (-not $buildScript.Contains('$PSBoundParameters.ContainsKey("TelemetryEndpoint")')) {
    throw "M5 release validation failed. Explicit and development telemetry endpoints must be mutually exclusive."
}

$versionTemplate = Get-Content -Raw -Encoding UTF8 -LiteralPath $versionTemplatePath
foreach ($snippet in @("VIBEREADY_APP_VERSION_W", "VIBEREADY_COMPILED_TELEMETRY_ENDPOINT")) {
    if (-not $versionTemplate.Contains($snippet)) {
        throw "M5 release validation failed. version.h.in is missing snippet: $snippet"
    }
}

$resourceTemplate = Get-Content -Raw -Encoding UTF8 -LiteralPath $resourceTemplatePath
foreach ($snippet in @("VS_VERSION_INFO", "ProductVersion", "FileVersion")) {
    if (-not $resourceTemplate.Contains($snippet)) {
        throw "M5 release validation failed. VibeReady.rc.in is missing snippet: $snippet"
    }
}

$manifest = Get-Content -Raw -Encoding UTF8 -LiteralPath $manifestPath | ConvertFrom-Json
if (-not $manifest.app_version) {
    throw "Release manifest is missing app_version."
}
if ($manifest.exe_product_version_expected -ne $manifest.app_version) {
    throw "Release manifest app_version and exe_product_version_expected differ."
}
if ($manifest.telemetry_endpoint_configured) {
    if ([string]::IsNullOrWhiteSpace([string]$manifest.telemetry_endpoint) -or
        -not ([string]$manifest.telemetry_endpoint).StartsWith("https://", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Configured release telemetry endpoint must be recorded as an HTTPS URL."
    }
} elseif (-not [string]::IsNullOrWhiteSpace([string]$manifest.telemetry_endpoint)) {
    throw "Release manifest records an endpoint while telemetry_endpoint_configured=false."
}

$zipHash = Get-FileHash -Algorithm SHA256 -LiteralPath $ZipPath
if ($zipHash.Hash -ne $manifest.package_sha256) {
    throw "Release ZIP hash does not match manifest."
}

$exeHash = Get-FileHash -Algorithm SHA256 -LiteralPath $packagedExePath
if ($exeHash.Hash -ne $manifest.exe_sha256) {
    throw "Packaged EXE hash does not match manifest."
}

$versionInfo = (Get-Item -LiteralPath $packagedExePath).VersionInfo
$productVersion = [string]$versionInfo.ProductVersion
$fileVersion = [string]$versionInfo.FileVersion
if (-not $productVersion.StartsWith([string]$manifest.app_version, [System.StringComparison]::Ordinal)) {
    throw "EXE ProductVersion '$productVersion' does not start with app_version '$($manifest.app_version)'."
}
if (-not $fileVersion.StartsWith([string]$manifest.app_version, [System.StringComparison]::Ordinal)) {
    throw "EXE FileVersion '$fileVersion' does not start with app_version '$($manifest.app_version)'."
}

$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("VibeReadyRelease-" + [System.Guid]::NewGuid().ToString("N"))
try {
    New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
    Expand-Archive -LiteralPath $ZipPath -DestinationPath $testRoot
    $expectedFiles = @("VibeReady.exe", "README.txt", "THIRD-PARTY-NOTICES.txt")
    foreach ($file in $expectedFiles) {
        if (-not (Test-Path -LiteralPath (Join-Path $testRoot $file))) {
            throw "Release ZIP is missing required file: $file"
        }
    }
} finally {
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}

$signature = Get-AuthenticodeSignature -FilePath $packagedExePath
if ($RequireSignature -and $signature.Status -ne "Valid") {
    throw "External release requires a valid Authenticode signature. Current status: $($signature.Status)"
}

Write-Output "M5 release validation passed. Signature status: $($signature.Status)."
