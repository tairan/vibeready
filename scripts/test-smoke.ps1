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

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class VibeReadySmokeWin32 {
    public delegate bool EnumWindowProc(IntPtr hwnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindow(string className, string windowName);

    [DllImport("user32.dll")]
    public static extern bool EnumChildWindows(IntPtr parent, EnumWindowProc callback, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hwnd, out RECT rect);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr hwnd, StringBuilder className, int maxCount);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hwnd);

}
"@

function Get-WindowClassName {
    param([System.IntPtr]$Handle)

    $builder = [System.Text.StringBuilder]::new(128)
    [void][VibeReadySmokeWin32]::GetClassName($Handle, $builder, $builder.Capacity)
    return $builder.ToString()
}

function Get-ClientRectValue {
    param([System.IntPtr]$Handle)

    $rect = [VibeReadySmokeWin32+RECT]::new()
    if (-not [VibeReadySmokeWin32]::GetClientRect($Handle, [ref]$rect)) {
        throw "Unable to read VibeReady client rectangle."
    }
    return $rect
}

function Get-ChildWindowHandles {
    param([System.IntPtr]$Parent)

    $children = [System.Collections.Generic.List[System.IntPtr]]::new()
    $callback = [VibeReadySmokeWin32+EnumWindowProc]{
        param([System.IntPtr]$Child, [System.IntPtr]$State)
        [void]$children.Add($Child)
        return $true
    }
    [void][VibeReadySmokeWin32]::EnumChildWindows($Parent, $callback, [System.IntPtr]::Zero)
    return $children
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
        Start-Sleep -Milliseconds 500
        if ($process.HasExited) {
            throw "VibeReady.exe exited before UI verification with code $($process.ExitCode)."
        }

        $clientRect = Get-ClientRectValue -Handle $mainWindow
        $clientWidth = $clientRect.Right - $clientRect.Left
        $clientHeight = $clientRect.Bottom - $clientRect.Top
        if ($clientWidth -lt 640 -or $clientHeight -lt 480) {
            throw "VibeReady client area is unexpectedly small: ${clientWidth}x${clientHeight}."
        }

        $children = Get-ChildWindowHandles -Parent $mainWindow
        if ($children.Count -ne 0) {
            throw "Expected Direct2D self-drawn UI with no default child controls, but found $($children.Count) child window(s)."
        }

        $result.UiFoundation = [pscustomobject]@{
            RenderPath = "Direct2D/DirectWrite self-drawn"
            ChildWindowCount = $children.Count
            ClientWidth = $clientWidth
            ClientHeight = $clientHeight
        }
    }

    [pscustomobject]$result
} finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit(5000)
    }
}
