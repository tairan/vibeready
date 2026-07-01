param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$mainPath = Join-Path $repoRoot "src\windows\main.cpp"

if (-not (Test-Path -LiteralPath $mainPath)) {
    throw "Missing source file: $mainPath"
}

$source = Get-Content -Raw -Encoding UTF8 -LiteralPath $mainPath

$requiredSnippets = @(
    "Verifying",
    "VerificationFailed",
    "Ready",
    "StartVerificationFlow",
    "AdvanceVerificationFlow",
    "RefreshProcessEnvironmentPath",
    "WriteLocalTestProject",
    '\"dev\": \"node server.js\"',
    "npm.cmd run dev",
    "ProbeLocalHttp",
    "OpenDefaultBrowserToLocalPage",
    "OpenVscodeProject",
    "BuildNextPromptTemplate",
    "CopyReadyPrompt",
    "CopyLocalAddress",
    "StopVerificationServer",
    "LOCAL_SERVER_FAILED",
    "BROWSER_OPEN_FAILED",
    "VSCODE_OPEN_FAILED",
    "verification_started",
    "verification_completed",
    "ready_reached"
)

foreach ($snippet in $requiredSnippets) {
    if (-not $source.Contains($snippet)) {
        throw "M4 validation failed. Missing snippet: $snippet"
    }
}

$forbiddenSnippets = @(
    "npm install",
    "npx ",
    "yarn ",
    "pnpm "
)

foreach ($snippet in $forbiddenSnippets) {
    if ($source.Contains($snippet)) {
        throw "M4 validation failed. Local verification must not download dependencies: $snippet"
    }
}

Write-Output "M4 validation passed."
