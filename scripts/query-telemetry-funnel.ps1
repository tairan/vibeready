param(
    [Parameter(Mandatory = $true)]
    [string]$DatabaseName,
    [ValidateRange(1, 365)]
    [int]$SinceDays = 30,
    [ValidateSet("funnel", "errors", "ready")]
    [string]$Query = "funnel",
    [switch]$Remote
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$sqlName = switch ($Query) {
    "funnel" { "funnel_core.sql" }
    "errors" { "errors_by_version.sql" }
    "ready" { "ready_conversion.sql" }
}
$sqlPath = Join-Path $repoRoot (Join-Path "telemetry\sql" $sqlName)
if (-not (Test-Path -LiteralPath $sqlPath)) {
    throw "Missing SQL query: $sqlPath"
}

$sql = Get-Content -Raw -Encoding UTF8 -LiteralPath $sqlPath
$sql = $sql.Replace(":since_days", [string]$SinceDays)

$tempSql = Join-Path ([System.IO.Path]::GetTempPath()) ("vibeready-telemetry-" + [System.Guid]::NewGuid().ToString("N") + ".sql")
try {
    Set-Content -LiteralPath $tempSql -Encoding UTF8 -Value $sql
    $arguments = @("wrangler", "d1", "execute", $DatabaseName, "--file", $tempSql)
    if ($Remote) {
        $arguments += "--remote"
    } else {
        $arguments += "--local"
    }
    & npx @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Wrangler D1 query failed with exit code $LASTEXITCODE."
    }
} finally {
    if (Test-Path -LiteralPath $tempSql) {
        Remove-Item -LiteralPath $tempSql -Force
    }
}
