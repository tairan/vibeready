$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$rulesPath = Join-Path $repoRoot 'specs/vibeready-rules.json'
$docPath = Join-Path $repoRoot 'docs/state-error-telemetry-dictionary.md'

if (-not (Test-Path -LiteralPath $rulesPath)) {
    throw "Missing rules file: $rulesPath"
}

if (-not (Test-Path -LiteralPath $docPath)) {
    throw "Missing documentation file: $docPath"
}

$utf8 = [System.Text.Encoding]::UTF8
$rules = [System.IO.File]::ReadAllText($rulesPath, $utf8) | ConvertFrom-Json
$allowedStates = @('ready', 'missing', 'unusable', 'unsupported', 'error')
$requiredErrorCodes = @(
    'WINGET_UNAVAILABLE',
    'USER_CANCELLED_UAC',
    'INSTALL_FAILED',
    'VERIFY_FAILED',
    'NETWORK_UNAVAILABLE',
    'NODE_NOT_FOUND',
    'NPM_NOT_FOUND',
    'GIT_NOT_FOUND',
    'VSCODE_NOT_FOUND'
)
$requiredLanguages = @('en', 'zh-CN', 'ja')
$disallowedTerms = @(
    'full_path',
    'folder_path',
    'username',
    'account_name',
    'environment_variable',
    'project_content',
    'source_code',
    'api_key',
    'token',
    'credential',
    'authorization_header',
    'raw_command_output',
    'raw_log',
    'crash_dump',
    'screenshot',
    'clipboard'
)

$stateIds = @($rules.tool_states | ForEach-Object { $_.id })
foreach ($state in $allowedStates) {
    if ($stateIds -notcontains $state) {
        throw "Missing required tool state: $state"
    }
}

foreach ($state in $stateIds) {
    if ($allowedStates -notcontains $state) {
        throw "Unexpected tool state: $state"
    }
}

$errorCodes = @($rules.error_codes | ForEach-Object { $_.code })
foreach ($code in $requiredErrorCodes) {
    if ($errorCodes -notcontains $code) {
        throw "Missing required error code: $code"
    }
}

foreach ($entry in $rules.error_codes) {
    if ($allowedStates -notcontains $entry.state) {
        throw "Error code $($entry.code) uses invalid state: $($entry.state)"
    }

    foreach ($language in $requiredLanguages) {
        $value = $entry.explanations.$language
        if ([string]::IsNullOrWhiteSpace($value)) {
            throw "Error code $($entry.code) is missing $language explanation"
        }
    }
}

$events = @($rules.telemetry.events)
if ($events.Count -gt [int]$rules.telemetry.max_core_events) {
    throw "Telemetry event count $($events.Count) exceeds max_core_events $($rules.telemetry.max_core_events)"
}

$eventNames = @{}
foreach ($event in $events) {
    if ([string]::IsNullOrWhiteSpace($event.name)) {
        throw 'Telemetry event has an empty name'
    }

    if ($eventNames.ContainsKey($event.name)) {
        throw "Duplicate telemetry event: $($event.name)"
    }

    $eventNames[$event.name] = $true

    if ([string]::IsNullOrWhiteSpace($event.trigger)) {
        throw "Telemetry event $($event.name) is missing trigger"
    }

    if ([string]::IsNullOrWhiteSpace($event.privacy)) {
        throw "Telemetry event $($event.name) is missing privacy boundary"
    }

    foreach ($field in @($event.fields)) {
        if ($disallowedTerms -contains $field) {
            throw "Telemetry event $($event.name) uses disallowed field: $field"
        }
    }
}

foreach ($term in $disallowedTerms) {
    if ($rules.telemetry.disallowed_fields -notcontains $term) {
        throw "Privacy disallowed_fields does not list required term: $term"
    }
}

[pscustomobject]@{
    RulesFile = $rulesPath
    ToolStates = $stateIds.Count
    ErrorCodes = $errorCodes.Count
    TelemetryEvents = $events.Count
    MaxTelemetryEvents = [int]$rules.telemetry.max_core_events
    Status = 'Valid'
} | Format-List
