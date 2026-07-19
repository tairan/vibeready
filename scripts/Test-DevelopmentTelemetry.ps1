param(
    [string]$ConfigPath = (Join-Path (Split-Path -Parent $PSScriptRoot) "config\development-services.json")
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if (-not (Test-Path -LiteralPath $ConfigPath)) {
    throw "Development services config not found: $ConfigPath"
}

$config = Get-Content -Raw -Encoding UTF8 -LiteralPath $ConfigPath | ConvertFrom-Json
$telemetry = $config.telemetry
$healthEndpoint = [string]$telemetry.health_endpoint
$batchEndpoint = [string]$telemetry.batch_endpoint

if ([string]::IsNullOrWhiteSpace($healthEndpoint) -or [string]::IsNullOrWhiteSpace($batchEndpoint)) {
    throw "Development telemetry endpoints are missing from: $ConfigPath"
}

$health = Invoke-RestMethod -Method Get -Uri $healthEndpoint -TimeoutSec 30
if (-not $health.ok) {
    throw "Development telemetry health check returned ok=false."
}
if ([string]$health.service -ne "vibeready-telemetry") {
    throw "Unexpected telemetry service: $($health.service)"
}
if ([string]$health.schema_version -ne [string]$telemetry.schema_version) {
    throw "Unexpected telemetry schema version: $($health.schema_version)"
}
if (-not $health.d1_binding) {
    throw "Development telemetry Worker does not report a D1 binding."
}

$installationId = [guid]::NewGuid().ToString()
$sessionId = [guid]::NewGuid().ToString()
$event = @{
    event_name = "app_opened"
    event_version = 1
    occurred_at = (Get-Date).ToUniversalTime().ToString("o")
    app_version = "0.1.0-development-validation"
    installation_id = $installationId
    session_id = $sessionId
    os_name = "windows"
    consent_state = "enabled"
    launch_result = "validation"
}
$payload = @{ events = @($event) } | ConvertTo-Json -Depth 5
$accepted = Invoke-RestMethod `
    -Method Post `
    -Uri $batchEndpoint `
    -ContentType "application/json" `
    -Body $payload `
    -TimeoutSec 30

if (-not $accepted.ok -or [int]$accepted.accepted -ne 1) {
    throw "Development telemetry Worker did not accept the validation event."
}

$sensitiveEvent = $event.Clone()
$sensitiveEvent.installation_id = [guid]::NewGuid().ToString()
$sensitiveEvent.session_id = [guid]::NewGuid().ToString()
$sensitiveEvent.full_path = "C:\Users\example\secret.txt"
$sensitivePayload = @{ events = @($sensitiveEvent) } | ConvertTo-Json -Depth 5
$sensitiveStatus = $null
$sensitiveResponse = $null

try {
    Invoke-RestMethod `
        -Method Post `
        -Uri $batchEndpoint `
        -ContentType "application/json" `
        -Body $sensitivePayload `
        -TimeoutSec 30 | Out-Null
} catch {
    if ($_.Exception.Response) {
        $sensitiveStatus = [int]$_.Exception.Response.StatusCode
    }
    $sensitiveResponse = $_.ErrorDetails.Message
}

if ($sensitiveStatus -ne 400 -or $sensitiveResponse -notmatch "field is disallowed: full_path") {
    throw "Development telemetry Worker did not reject the prohibited full_path field as expected."
}

[pscustomobject]@{
    worker_base_url = [string]$telemetry.worker_base_url
    schema_version = [string]$health.schema_version
    d1_binding = [bool]$health.d1_binding
    accepted = [int]$accepted.accepted
    validation_installation_id = $installationId
    sensitive_field_status = $sensitiveStatus
}

Write-Output "Development telemetry endpoint validation passed."
