param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot

$requiredDirectories = @(
    "apps\windows-client",
    "apps\website",
    "services\telemetry-worker",
    "docs",
    "scripts",
    "specs",
    "telemetry"
)
$requiredFiles = @(
    "CMakeLists.txt",
    "README.md",
    "apps\windows-client\CMakeLists.txt",
    "apps\windows-client\src\main.cpp",
    "apps\windows-client\README.txt",
    "apps\windows-client\THIRD-PARTY-NOTICES.txt",
    "apps\website\README.md",
    "services\telemetry-worker\package.json",
    "services\telemetry-worker\wrangler.jsonc"
)
$legacyPaths = @(
    "src\windows",
    "tests\windows",
    "telemetry-worker",
    "README.txt",
    "THIRD-PARTY-NOTICES.txt"
)

foreach ($relativePath in $requiredDirectories) {
    $path = Join-Path $repoRoot $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Container)) {
        throw "Repository layout is missing directory: $relativePath"
    }
}

foreach ($relativePath in $requiredFiles) {
    $path = Join-Path $repoRoot $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Repository layout is missing file: $relativePath"
    }
}

$trackedLegacyPaths = @(& git -C $repoRoot ls-files -- $legacyPaths)
if ($LASTEXITCODE -ne 0) {
    throw "Could not inspect tracked repository paths."
}
if ($trackedLegacyPaths.Count -gt 0) {
    throw "Repository layout still tracks legacy paths: $($trackedLegacyPaths -join ', ')"
}

$websiteRoot = Join-Path $repoRoot "apps\website"
foreach ($implementationFile in @("package.json", "package-lock.json", "pnpm-lock.yaml", "yarn.lock", "wrangler.jsonc", "netlify.toml")) {
    if (Test-Path -LiteralPath (Join-Path $websiteRoot $implementationFile)) {
        throw "Website placeholder must not contain implementation file: $implementationFile"
    }
}

Write-Output "Repository layout validation passed."
