param(
    [string]$ExePath,
    [string]$AppDataDir,
    [int]$TimeoutSeconds = 10,
    [switch]$VerifyUiControls
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $ExePath) {
    $ExePath = Join-Path $repoRoot "build\windows-x64\VibeReady.exe"
}
if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "VibeReady.exe not found: $ExePath"
}

if (-not $AppDataDir) {
    $AppDataDir = Join-Path ([System.IO.Path]::GetTempPath()) ("VibeReadySmoke-" + [System.Guid]::NewGuid().ToString("N"))
}
New-Item -ItemType Directory -Path $AppDataDir -Force | Out-Null
$configDir = Join-Path $AppDataDir "VibeReady"
New-Item -ItemType Directory -Path $configDir -Force | Out-Null
Set-Content -LiteralPath (Join-Path $configDir "config.ini") -Value @("[preferences]", "language=en", "telemetry=0") -Encoding ASCII

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class VibeReadySmokeWin32 {
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindow(string className, string windowName);

    [DllImport("user32.dll")]
    public static extern IntPtr GetDlgItem(IntPtr parent, int id);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr hwnd, StringBuilder className, int maxCount);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hwnd);

    [DllImport("user32.dll")]
    public static extern bool IsWindowEnabled(IntPtr hwnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int maxCount);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr hwnd, int msg, IntPtr wparam, IntPtr lparam);
}
"@

function Get-WindowClassName {
    param([System.IntPtr]$Handle)

    $builder = [System.Text.StringBuilder]::new(128)
    [void][VibeReadySmokeWin32]::GetClassName($Handle, $builder, $builder.Capacity)
    return $builder.ToString()
}

function Get-WindowTextValue {
    param([System.IntPtr]$Handle)

    $builder = [System.Text.StringBuilder]::new(8192)
    [void][VibeReadySmokeWin32]::GetWindowText($Handle, $builder, $builder.Capacity)
    return $builder.ToString()
}

function Assert-Control {
    param(
        [System.IntPtr]$Parent,
        [int]$Id,
        [string]$ExpectedClass,
        [switch]$RequireText
    )

    $handle = [VibeReadySmokeWin32]::GetDlgItem($Parent, $Id)
    if ($handle -eq [System.IntPtr]::Zero) {
        throw "Control id $Id was not found."
    }

    $className = Get-WindowClassName -Handle $handle
    if ($className -ne $ExpectedClass) {
        throw "Control id $Id expected class $ExpectedClass but was $className."
    }

    if (-not [VibeReadySmokeWin32]::IsWindowVisible($handle)) {
        throw "Control id $Id is not visible."
    }

    if (-not [VibeReadySmokeWin32]::IsWindowEnabled($handle)) {
        throw "Control id $Id is not enabled."
    }

    $text = Get-WindowTextValue -Handle $handle
    if ($RequireText -and [string]::IsNullOrWhiteSpace($text)) {
        throw "Control id $Id has no observable text."
    }

    [pscustomobject]@{
        Id = $Id
        ClassName = $className
        Visible = $true
        Enabled = $true
        Text = $text
    }
}

function Test-ContainsAny {
    param(
        [string]$Text,
        [string[]]$Needles
    )

    foreach ($needle in $Needles) {
        if ($Text.Contains($needle)) {
            return $true
        }
    }
    return $false
}

function Assert-ScanResultsText {
    param(
        [string]$ButtonText,
        [string]$StatusText
    )

    $resultTitles = @("Scan results")
    $mustFixHeaders = @("Must fix")
    $readyHeaders = @("Ready")
    $manualHeaders = @("Cannot handle automatically")
    $rescanLabels = @("Rescan environment")

    if (-not (Test-ContainsAny -Text $StatusText -Needles $resultTitles)) {
        throw "Status text does not contain a localized scan results title."
    }
    if (-not (Test-ContainsAny -Text $StatusText -Needles $mustFixHeaders)) {
        throw "Status text does not contain a localized must-fix section."
    }
    if (-not (Test-ContainsAny -Text $StatusText -Needles $readyHeaders)) {
        throw "Status text does not contain a localized ready section."
    }
    if (-not (Test-ContainsAny -Text $StatusText -Needles $manualHeaders)) {
        throw "Status text does not contain a localized manual section."
    }
    if (-not (Test-ContainsAny -Text $ButtonText -Needles $rescanLabels)) {
        throw "Primary button does not expose a localized rescan action."
    }
}

function Wait-ControlText {
    param(
        [System.IntPtr]$Parent,
        [int[]]$Ids,
        [DateTime]$Deadline
    )

    while ([DateTime]::UtcNow -lt $Deadline) {
        $allReady = $true
        foreach ($id in $Ids) {
            $handle = [VibeReadySmokeWin32]::GetDlgItem($Parent, $id)
            if ($handle -eq [System.IntPtr]::Zero -or
                -not [VibeReadySmokeWin32]::IsWindowVisible($handle) -or
                -not [VibeReadySmokeWin32]::IsWindowEnabled($handle) -or
                [string]::IsNullOrWhiteSpace((Get-WindowTextValue -Handle $handle))) {
                $allReady = $false
                break
            }
        }
        if ($allReady) {
            return
        }
        Start-Sleep -Milliseconds 200
    }

    throw "Timed out waiting for control text to become observable."
}

function Invoke-ButtonClick {
    param([System.IntPtr]$Handle)

    $bmClick = 0x00F5
    [void][VibeReadySmokeWin32]::SendMessage($Handle, $bmClick, [System.IntPtr]::Zero, [System.IntPtr]::Zero)
}

$process = $null
try {
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = (Resolve-Path -LiteralPath $ExePath).Path
    $startInfo.WorkingDirectory = Split-Path -Parent $startInfo.FileName
    $startInfo.UseShellExecute = $false
    $startInfo.EnvironmentVariables["VIBEREADY_APPDATA_DIR"] = (Resolve-Path -LiteralPath $AppDataDir).Path
    $process = [System.Diagnostics.Process]::Start($startInfo)
    if (-not $process) {
        throw "Failed to start VibeReady.exe."
    }

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $mainWindow = [System.IntPtr]::Zero
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($process.HasExited) {
            throw "VibeReady.exe exited early with code $($process.ExitCode)."
        }
        $mainWindow = [VibeReadySmokeWin32]::FindWindow("VibeReadyMainWindow", "VibeReady")
        if ($mainWindow -ne [System.IntPtr]::Zero) {
            break
        }
        Start-Sleep -Milliseconds 200
    }

    if ($mainWindow -eq [System.IntPtr]::Zero) {
        throw "VibeReady main window was not found within $TimeoutSeconds seconds."
    }

    $result = [ordered]@{
        Started = $true
        AliveAfterWindowFound = (-not $process.HasExited)
        MainWindowClass = Get-WindowClassName -Handle $mainWindow
        AppDataDir = (Resolve-Path -LiteralPath $AppDataDir).Path
    }

    if ($VerifyUiControls) {
        Wait-ControlText -Parent $mainWindow -Ids @(103) -Deadline $deadline
        $primaryHandle = [VibeReadySmokeWin32]::GetDlgItem($mainWindow, 103)
        $initialPrimary = Assert-Control -Parent $mainWindow -Id 103 -ExpectedClass "Button" -RequireText
        Invoke-ButtonClick -Handle $primaryHandle
        Wait-ControlText -Parent $mainWindow -Ids @(104) -Deadline ([DateTime]::UtcNow.AddSeconds($TimeoutSeconds))
        $primaryButton = Assert-Control -Parent $mainWindow -Id 103 -ExpectedClass "Button" -RequireText
        $statusText = Assert-Control -Parent $mainWindow -Id 104 -ExpectedClass "Static" -RequireText
        Assert-ScanResultsText -ButtonText $primaryButton.Text -StatusText $statusText.Text
        $result.UiControls = @($initialPrimary, $primaryButton, $statusText)
    }

    [pscustomobject]$result
} finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit(5000)
    }
}
