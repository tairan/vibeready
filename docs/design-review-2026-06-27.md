# VibeReady UI/UX Design Review

Date: 2026-06-27
Source issue: CHI-46
Reviewer role: Windows interaction design review

## Scope

This review inspected the current Windows portable client built from `main`.
CHI-46 is an operations and execution-rules issue, so this document records
review evidence and tracking issues without changing product behavior.

Scope references:

- `docs/mvp-scope.md`
- `docs/mvp-page-flow.md`
- `docs/i18n-copy.json`
- Linear CHI-46

## Local Validation

Commands run:

```powershell
git status --short --branch
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-release.ps1
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = 'dist\VibeReady-Windows-x64.zip'
[IO.Compression.ZipFile]::OpenRead((Resolve-Path $zip)).Entries | Select-Object FullName,Length
```

Observed ZIP contents:

- `apps/windows-client/README.txt`
- `apps/windows-client/THIRD-PARTY-NOTICES.txt`
- `VibeReady.exe`

The built executable was launched from:

```text
build\windows-x64\VibeReady.exe
```

The package test now also extracts the ZIP to a temporary directory and runs
the extracted `VibeReady.exe` with `VIBEREADY_APPDATA_DIR` pointing at a
temporary app data directory, so smoke tests do not write to the real user
configuration directory.

## CHI-46 Testing Baseline Fixes

During review against the updated CHI-46 rules, the repo was missing the
recommended automated test entry points. This branch now adds:

- `scripts/test.ps1`: runs rules validation, release build, package test, and
  CTest when configured.
- `scripts/test-package.ps1`: verifies ZIP contents, extracts the ZIP to a
  temporary directory, and runs package smoke validation.
- `scripts/test-smoke.ps1`: launches `VibeReady.exe`, waits for the main
  window, and validates stable Win32 control IDs for key controls.

UI automation定位 strategy for the current Win32 surface:

- Main window: class `VibeReadyMainWindow`, title `VibeReady`.
- Language selector: Win32 control ID `101`, class `ComboBox`.
- Telemetry consent: Win32 control ID `102`, class `Button`, observable text.
- Primary action: Win32 control ID `103`, class `Button`, observable text.
- Startup status block: Win32 control ID `104`, class `Static`, observable
  status text.

The product still uses default Win32 controls, so these IDs are the current
stable automation locator. CHI-47 through CHI-50 should preserve or improve
this locator strategy when they change UI behavior or layout.

## Observed UI

The current app opens directly to the Home screen. It shows:

- VibeReady title
- Language dropdown
- Telemetry checkbox
- Primary action button
- Startup diagnostics block

The review covered Simplified Chinese, English, and Japanese layouts.

## Findings Tracked In Linear

### CHI-47: Main CTA does not enter scanning flow

Clicking the primary action opens an informational message box instead of
moving into the frozen `scanning` flow. This blocks the MVP success path.

Linear: https://linear.app/chieworks/issue/CHI-47/design-review-主-cta-点击后未进入扫描流程

### CHI-48: First launch lacks explicit language and telemetry save step

The app starts directly on Home with language and telemetry controls mixed
into the main workspace. The frozen page flow expects a first-run
`language_telemetry` step before Home.

Linear: https://linear.app/chieworks/issue/CHI-48/design-review-首次启动缺少明确的语言与遥测偏好保存步骤

### CHI-49: Startup diagnostics are too technical for non-software users

The default status block exposes labels such as `x64`, `Temp`, `Config`,
`Log`, and `Telemetry` without user-facing explanation or recovery guidance.

Linear: https://linear.app/chieworks/issue/CHI-49/design-review-启动自检状态区过于技术化且缺少用户可执行解释

### CHI-50: Home visual hierarchy is not yet product-ready

The page relies on default Win32 controls and a large raw diagnostics block.
The main action, settings, telemetry, and diagnostic output compete for visual
weight, which weakens trust for non-software users.

Linear: https://linear.app/chieworks/issue/CHI-50/design-review-home-页面原生控件与状态块视觉层级不足

## Remaining Risk

This review was performed on the local Windows 11 machine only. It did not
validate clean Windows 10/11 machines, high-DPI variants, keyboard-only
navigation beyond default Win32 tab behavior, screen reader output quality, or
complete scan and repair flows.
