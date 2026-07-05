param(
    [switch]$SkipBuild,
    [switch]$SkipSmoke,
    [switch]$RequireSignature
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$resultPath = Join-Path $repoRoot "dist\qa-matrix-m5.json"

function Invoke-Gate {
    param(
        [string]$Name,
        [scriptblock]$Command
    )

    $started = Get-Date
    try {
        & $Command | Out-Null
        return [pscustomobject][ordered]@{
            name = $Name
            status = "passed"
            started_at = $started.ToString("o")
            ended_at = (Get-Date).ToString("o")
            detail = $null
        }
    } catch {
        return [pscustomobject][ordered]@{
            name = $Name
            status = "failed"
            started_at = $started.ToString("o")
            ended_at = (Get-Date).ToString("o")
            detail = $_.Exception.Message
        }
    }
}

$results = @()
$results += Invoke-Gate "telemetry_privacy" { & (Join-Path $PSScriptRoot "Validate-M5-TelemetryPrivacy.ps1") }
$results += Invoke-Gate "worker_static_contract" { & (Join-Path $PSScriptRoot "Validate-M5-Worker.ps1") }
$results += Invoke-Gate "worker_node_contract" { node --test (Join-Path $repoRoot "telemetry-worker\test\worker-contract.test.mjs") }
$results += Invoke-Gate "client_telemetry" { & (Join-Path $PSScriptRoot "Validate-M5-ClientTelemetry.ps1") }

if (-not $SkipBuild) {
    $results += Invoke-Gate "release_build" { & (Join-Path $PSScriptRoot "build-release.ps1") }
}

$results += Invoke-Gate "package_smoke" { & (Join-Path $PSScriptRoot "test-package.ps1") -SkipSmoke:$SkipSmoke }
$results += Invoke-Gate "release_manifest" { & (Join-Path $PSScriptRoot "Validate-M5-Release.ps1") -RequireSignature:$RequireSignature }

$results += [pscustomobject][ordered]@{
    name = "clean_windows_10_x64_manual"
    status = "pending_external"
    started_at = $null
    ended_at = $null
    detail = "Requires clean Windows 10 x64 QA VM."
}
$results += [pscustomobject][ordered]@{
    name = "clean_windows_11_x64_manual"
    status = "pending_external"
    started_at = $null
    ended_at = $null
    detail = "Requires clean Windows 11 x64 QA VM."
}
$results += [pscustomobject][ordered]@{
    name = "production_cloudflare_deploy"
    status = "pending_external"
    started_at = $null
    ended_at = $null
    detail = "Requires Cloudflare account, D1 database, Worker route, and deployment credentials."
}
$results += [pscustomobject][ordered]@{
    name = "external_release_signature"
    status = $(if ($RequireSignature) { "enforced" } else { "pending_external" })
    started_at = $null
    ended_at = $null
    detail = "Requires release signing certificate for external distribution."
}

New-Item -ItemType Directory -Path (Split-Path -Parent $resultPath) -Force | Out-Null
[ordered]@{
    generated_at = (Get-Date).ToString("o")
    results = $results
} | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath $resultPath

$failed = @($results | Where-Object { $_.status -eq "failed" })
if ($failed.Count -gt 0) {
    throw "M5 QA matrix has failed automated gates. See $resultPath"
}

Write-Output "M5 QA matrix automated gates passed. Results: $resultPath"
