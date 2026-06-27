Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$copyPath = Join-Path $repoRoot 'docs/i18n-copy.json'
$flowPath = Join-Path $repoRoot 'docs/mvp-page-flow.md'
$termsPath = Join-Path $repoRoot 'docs/terminology.md'

foreach ($path in @($copyPath, $flowPath, $termsPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required CHI-15 artifact is missing: $path"
    }
}

$copy = Get-Content -LiteralPath $copyPath -Raw -Encoding UTF8 | ConvertFrom-Json

$requiredLocales = @('en', 'zh-CN', 'ja')
$requiredScreens = @(
    'startup',
    'unsupported_system',
    'language_telemetry',
    'home',
    'scanning',
    'scan_results',
    'fix_plan',
    'fix_progress',
    'manual_steps',
    'verifying',
    'verification_failed',
    'ready',
    'settings'
)
$requiredTools = @('Git', 'Node.js', 'npm', 'VS Code')

foreach ($locale in $requiredLocales) {
    if ($copy.locales -notcontains $locale) {
        throw "Missing locale: $locale"
    }
}

foreach ($screenId in $requiredScreens) {
    $screen = $copy.screens | Where-Object { $_.id -eq $screenId }
    if ($null -eq $screen) {
        throw "Missing screen: $screenId"
    }

    foreach ($locale in $requiredLocales) {
        $localeCopy = $screen.copy.$locale
        if ($null -eq $localeCopy) {
            throw "Screen $screenId is missing locale copy: $locale"
        }
        if ([string]::IsNullOrWhiteSpace($localeCopy.title)) {
            throw "Screen $screenId locale $locale is missing title"
        }
        if ([string]::IsNullOrWhiteSpace($localeCopy.body)) {
            throw "Screen $screenId locale $locale is missing body"
        }
    }
}

$primaryEntryScreens = @($copy.screens | Where-Object {
        $property = $_.PSObject.Properties['isPrimaryEntry']
        $null -ne $property -and $property.Value -eq $true
    })
if ($primaryEntryScreens.Count -ne 1) {
    throw "Expected exactly one primary entry screen, found $($primaryEntryScreens.Count)"
}
if ($primaryEntryScreens[0].id -ne $copy.primaryEntryScreenId) {
    throw "Primary entry screen id does not match primaryEntryScreenId"
}
$expectedZhPrimaryEntry = -join @(
    [char]0x68C0,
    [char]0x67E5,
    [char]0x7F51,
    [char]0x9875,
    ' vibe coding ',
    [char]0x73AF,
    [char]0x5883
)
if ($copy.primaryEntryCopy.'zh-CN' -ne $expectedZhPrimaryEntry) {
    throw "Simplified Chinese primary entry copy changed"
}

foreach ($locale in $requiredLocales) {
    $expected = $copy.primaryEntryCopy.$locale
    $actual = $primaryEntryScreens[0].copy.$locale.primaryAction
    if ($actual -ne $expected) {
        throw "Primary entry copy mismatch for $locale"
    }
}

$allPrimaryActions = @()
foreach ($screen in $copy.screens) {
    foreach ($locale in $requiredLocales) {
        $actionProperty = $screen.copy.$locale.PSObject.Properties['primaryAction']
        $action = if ($null -eq $actionProperty) { $null } else { $actionProperty.Value }
        if (-not [string]::IsNullOrWhiteSpace($action)) {
            $allPrimaryActions += [pscustomobject]@{
                Screen = $screen.id
                Locale = $locale
                Text = $action
            }
        }
    }
}

foreach ($entry in $allPrimaryActions) {
    foreach ($locale in $requiredLocales) {
        if ($entry.Text -eq $copy.primaryEntryCopy.$locale -and $entry.Screen -ne $copy.primaryEntryScreenId) {
            throw "Primary entry copy appears outside home: $($entry.Screen) $($entry.Locale)"
        }
    }
}

foreach ($tool in $requiredTools) {
    if ($copy.tools -notcontains $tool) {
        throw "Missing canonical tool name: $tool"
    }
}

$allText = Get-Content -LiteralPath $copyPath -Raw -Encoding UTF8
foreach ($tool in $requiredTools) {
    if ($allText -notmatch [regex]::Escape($tool)) {
        throw "Canonical tool name is absent from copy: $tool"
    }
}

Write-Output 'CHI-15 validation passed.'
Write-Output "Screens: $($requiredScreens.Count)"
Write-Output "Locales: $($requiredLocales -join ', ')"
Write-Output "Primary entry: $($copy.primaryEntryCopy.'zh-CN')"
