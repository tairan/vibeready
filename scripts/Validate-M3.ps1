param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$mainPath = Join-Path $repoRoot "apps\windows-client\src\main.cpp"

if (-not (Test-Path -LiteralPath $mainPath)) {
    throw "Missing source file: $mainPath"
}

$source = Get-Content -Raw -Encoding UTF8 -LiteralPath $mainPath

$requiredSnippets = @(
    "FixPlan",
    "FixProgress",
    "ManualSteps",
    "BuildRepairPlan",
    "StartRepairFlow",
    "RunWingetInstall",
    "IsApprovedPackageId",
    "--disable-interactivity",
    "Microsoft.VisualStudioCode",
    "Git.Git",
    "OpenJS.NodeJS.LTS",
    "https://code.visualstudio.com/Download",
    "https://git-scm.com/download/win",
    "https://nodejs.org/en/download"
)

foreach ($snippet in $requiredSnippets) {
    if (-not $source.Contains($snippet)) {
        throw "M3 validation failed. Missing snippet: $snippet"
    }
}

$unexpectedErrorCodes = @(
    "VSCODE_INSTALL_FAILED"
)

foreach ($errorCode in $unexpectedErrorCodes) {
    if ($source.Contains($errorCode)) {
        throw "M3 validation failed. Error code is not in the CHI-14 dictionary: $errorCode"
    }
}

$approvedPackages = @(
    "Microsoft.VisualStudioCode",
    "Git.Git",
    "OpenJS.NodeJS.LTS"
)

foreach ($packageId in $approvedPackages) {
    $escaped = [regex]::Escape($packageId)
    $matches = [regex]::Matches($source, $escaped)
    if ($matches.Count -lt 2) {
        throw "M3 validation failed. Package id is not both whitelisted and used: $packageId"
    }
}

Write-Output "M3 validation passed."
