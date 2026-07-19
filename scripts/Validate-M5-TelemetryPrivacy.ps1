param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$rulesPath = Join-Path $repoRoot "specs\vibeready-rules.json"
$schemaPath = Join-Path $repoRoot "specs\telemetry-schema.json"
$casesPath = Join-Path $repoRoot "specs\telemetry-redaction-cases.json"
$validSamplesPath = Join-Path $repoRoot "telemetry\samples\valid-events.json"
$rejectedSamplesPath = Join-Path $repoRoot "telemetry\samples\rejected-events.json"
$sqlDir = Join-Path $repoRoot "telemetry\sql"
$queryScriptPath = Join-Path $repoRoot "scripts\query-telemetry-funnel.ps1"
$queryTestPath = Join-Path $repoRoot "scripts\Test-M5-TelemetryQueries.ps1"

function Read-JsonFile {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing required file: $Path"
    }
    return Get-Content -Raw -Encoding UTF8 -LiteralPath $Path | ConvertFrom-Json
}

function Get-EventMap {
    param($Schema)
    $map = @{}
    foreach ($event in @($Schema.events)) {
        $map[$event.name] = @($event.fields)
    }
    return $map
}

function Test-SensitiveValue {
    param($Value)

    if ($null -eq $Value) {
        return $false
    }
    if ($Value -is [string]) {
        return $Value -match '[A-Za-z]:\\' -or
            $Value -match '\\\\[^\\]+' -or
            $Value -match '[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}' -or
            $Value -match '(ghp_|github_pat_|Bearer\s+|api[_-]?key|secret|token)'
    }
    if ($Value -is [System.Collections.IEnumerable] -and -not ($Value -is [string])) {
        foreach ($item in $Value) {
            if (Test-SensitiveValue $item) {
                return $true
            }
        }
    }
    return $false
}

function Test-TelemetryEvent {
    param(
        $Event,
        $Schema,
        [switch]$ExpectRejected
    )

    $eventMap = Get-EventMap $Schema
    $eventName = [string]$Event.event_name
    if (-not $eventMap.ContainsKey($eventName)) {
        if ($ExpectRejected) { return }
        throw "Unknown telemetry event: $eventName"
    }

    foreach ($field in @($Schema.required_common_fields)) {
        if (-not $Event.PSObject.Properties[$field]) {
            if ($ExpectRejected) { return }
            throw "Telemetry event $eventName is missing required field: $field"
        }
    }

    if ($Event.consent_state -ne "enabled") {
        if ($ExpectRejected) { return }
        throw "Telemetry event $eventName must use consent_state=enabled"
    }

    $allowed = @{}
    foreach ($field in @($Schema.common_fields)) { $allowed[$field] = $true }
    foreach ($field in @($eventMap[$eventName])) { $allowed[$field] = $true }

    foreach ($property in $Event.PSObject.Properties) {
        $name = $property.Name
        foreach ($term in @($Schema.disallowed_fields)) {
            if ($name.IndexOf([string]$term, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
                if ($ExpectRejected) { return }
                throw "Telemetry event $eventName contains disallowed field: $name"
            }
        }
        if (-not $allowed.ContainsKey($name)) {
            if ($ExpectRejected) { return }
            throw "Telemetry event $eventName contains unknown field: $name"
        }
        if (Test-SensitiveValue $property.Value) {
            if ($ExpectRejected) { return }
            throw "Telemetry event $eventName contains sensitive value in field: $name"
        }
    }

    if ($ExpectRejected) {
        throw "Expected rejected telemetry event was accepted: $eventName"
    }
}

$rules = Read-JsonFile $rulesPath
$schema = Read-JsonFile $schemaPath
$cases = Read-JsonFile $casesPath
$validSamples = Read-JsonFile $validSamplesPath
$rejectedSamples = Read-JsonFile $rejectedSamplesPath

$ruleEvents = @($rules.telemetry.events | ForEach-Object { $_.name } | Sort-Object)
$schemaEvents = @($schema.events | ForEach-Object { $_.name } | Sort-Object)
if (($ruleEvents -join "|") -ne ($schemaEvents -join "|")) {
    throw "Telemetry schema events do not match vibeready-rules.json"
}

$ruleDisallowed = @($rules.telemetry.disallowed_fields | Sort-Object)
$schemaDisallowed = @($schema.disallowed_fields | Sort-Object)
if (($ruleDisallowed -join "|") -ne ($schemaDisallowed -join "|")) {
    throw "Telemetry schema disallowed_fields do not match vibeready-rules.json"
}

foreach ($term in @($schema.disallowed_fields)) {
    $serializedCases = $cases | ConvertTo-Json -Depth 20
    if ($serializedCases.IndexOf([string]$term, [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
        throw "Redaction cases do not cover disallowed field: $term"
    }
}

foreach ($event in @($cases.valid)) {
    Test-TelemetryEvent -Event $event -Schema $schema
}
foreach ($case in @($cases.rejected)) {
    Test-TelemetryEvent -Event $case.payload -Schema $schema -ExpectRejected
}
foreach ($event in @($validSamples.events)) {
    Test-TelemetryEvent -Event $event -Schema $schema
}
foreach ($event in @($rejectedSamples.events)) {
    Test-TelemetryEvent -Event $event -Schema $schema -ExpectRejected
}

$requiredSql = @(
    "funnel_core.sql",
    "errors_by_version.sql",
    "ready_conversion.sql"
)
foreach ($file in $requiredSql) {
    $path = Join-Path $sqlDir $file
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing SQL file: $file"
    }
}

if (-not (Test-Path -LiteralPath $queryScriptPath)) {
    throw "Missing D1 telemetry query entrypoint: $queryScriptPath"
}
$queryScript = Get-Content -Raw -Encoding UTF8 -LiteralPath $queryScriptPath
foreach ($snippet in @(
    '[string]$DatabaseName = "DB"',
    'node_modules\.bin\wrangler.cmd',
    '"wrangler.jsonc"',
    '"--config"',
    '"--json"',
    '"--remote"',
    '"--local"'
)) {
    if (-not $queryScript.Contains($snippet)) {
        throw "Telemetry query script is missing pinned Wrangler/config behavior: $snippet"
    }
}

if (-not (Test-Path -LiteralPath $queryTestPath)) {
    throw "Missing D1 telemetry query runtime test: $queryTestPath"
}
$queryTest = Get-Content -Raw -Encoding UTF8 -LiteralPath $queryTestPath
foreach ($snippet in @(
    "app_opened",
    "ready_reached",
    "VERIFY_FAILED",
    "ready_reached_per_app_opened",
    "ready_installation_conversion"
)) {
    if (-not $queryTest.Contains($snippet)) {
        throw "Telemetry query runtime test is missing assertion input: $snippet"
    }
}

$funnel = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $sqlDir "funnel_core.sql")
foreach ($eventName in @("app_opened", "scan_started", "scan_completed", "repair_started", "repair_completed", "verification_completed", "ready_reached")) {
    if (-not $funnel.Contains($eventName)) {
        throw "Funnel SQL does not include event: $eventName"
    }
}
foreach ($dimension in @("app_version", "os_name")) {
    if (-not $funnel.Contains($dimension)) {
        throw "Funnel SQL does not include dimension: $dimension"
    }
}

$errors = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $sqlDir "errors_by_version.sql")
foreach ($dimension in @("app_version", "os_name", "error_code")) {
    if (-not $errors.Contains($dimension)) {
        throw "Error SQL does not include dimension: $dimension"
    }
}

Write-Output "M5 telemetry privacy validation passed."
