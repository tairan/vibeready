# VibeReady State Model, Error Codes, and Telemetry Dictionary

Issue: CHI-14

This document is the deterministic rules baseline for the VibeReady MVP. Scanning, repair, verification, and telemetry must use these states, error codes, and event names unless a later Linear issue explicitly changes the contract.

## MVP Boundary

The rules in this document are limited to preparing a Windows 10/11 x64 machine for webpage vibe coding. The MVP checks VS Code, Git, Node.js, npm, WinGet, operating system support, and local verification. It does not cover Python, course codes, project recognition, Cursor, Trae, Docker, WSL, GitHub login, sharing links, teacher dashboards, auto update, or any LLM behavior.

## Tool State Model

| State | Meaning | User-facing category | Next action |
| --- | --- | --- | --- |
| `ready` | The required tool or capability is present and usable. | Ready | Continue to the next scan or verification step. |
| `missing` | The tool is not found through supported detection paths or commands. | Must fix | Offer an approved install or manual setup path. |
| `unusable` | The tool appears installed but cannot be launched, returns an invalid result, times out, or fails a required capability check. | Must fix | Re-detect after install or show targeted manual steps. |
| `unsupported` | The current OS, architecture, or feature surface is outside the MVP boundary. | Cannot handle automatically | Show an unsupported-system or manual explanation page. |
| `error` | The scan, repair, or verification step failed for a reason not represented by a more specific state. | Needs attention | Show the mapped error code, retry path, and fallback. |

State transitions must be monotonic inside one scan pass: a result can move from unknown to one of the five states, but a later step must not silently replace a failure with `ready` unless it re-runs the relevant check and records the new result.

## Required Tool Result Shape

Every scanner and verifier result must provide:

| Field | Required | Notes |
| --- | --- | --- |
| `tool` | Yes | Stable id such as `vscode`, `git`, `node`, `npm`, `winget`, `system`, or `network`. |
| `state` | Yes | One of the five tool states. |
| `error_code` | Conditional | Required when `state` is `missing`, `unusable`, `unsupported`, or `error`. |
| `detected_version` | Optional | Sanitized semantic or vendor version only. |
| `detail_key` | Optional | Localized UI string key. Do not store raw paths or command output. |
| `can_auto_repair` | Yes | Boolean. Only true for approved MVP repair flows. |
| `checked_at` | Yes | Local timestamp or event timestamp. |

## Error Code Table

| Error code | State | English | Simplified Chinese | Japanese |
| --- | --- | --- | --- | --- |
| `UNSUPPORTED_WINDOWS_VERSION` | `unsupported` | This Windows version is outside the supported Windows 10/11 x64 MVP range. | 当前 Windows 版本不在 MVP 支持的 Windows 10/11 x64 范围内。 | この Windows バージョンは MVP が対応する Windows 10/11 x64 の範囲外です。 |
| `UNSUPPORTED_ARCHITECTURE` | `unsupported` | This device is not running a supported x64 Windows architecture. | 当前设备不是受支持的 Windows x64 架构。 | このデバイスは対応対象の Windows x64 アーキテクチャではありません。 |
| `CONFIG_DIR_UNWRITABLE` | `error` | VibeReady cannot write its local configuration folder. | VibeReady 无法写入本地配置目录。 | VibeReady はローカル設定フォルダーに書き込めません。 |
| `TEMP_DIR_UNWRITABLE` | `error` | VibeReady cannot write to the temporary folder needed for checks. | VibeReady 无法写入检测所需的临时目录。 | VibeReady は確認に必要な一時フォルダーに書き込めません。 |
| `VSCODE_NOT_FOUND` | `missing` | VS Code was not found on this computer. | 未在这台电脑上找到 VS Code。 | このコンピューターで VS Code が見つかりません。 |
| `GIT_NOT_FOUND` | `missing` | Git was not found or cannot be started from this environment. | 未找到 Git，或当前环境无法启动 Git。 | Git が見つからないか、この環境から起動できません。 |
| `GIT_INIT_FAILED` | `unusable` | Git started, but `git init` did not complete successfully. | Git 可以启动，但 `git init` 未成功完成。 | Git は起動しましたが、`git init` が正常に完了しませんでした。 |
| `NODE_NOT_FOUND` | `missing` | Node.js was not found or cannot be started from this environment. | 未找到 Node.js，或当前环境无法启动 Node.js。 | Node.js が見つからないか、この環境から起動できません。 |
| `NPM_NOT_FOUND` | `missing` | npm was not found or cannot be started from this environment. | 未找到 npm，或当前环境无法启动 npm。 | npm が見つからないか、この環境から起動できません。 |
| `TOOL_TIMEOUT` | `unusable` | A required tool did not respond before the timeout. | 必需工具未在超时时间内响应。 | 必須ツールがタイムアウトまでに応答しませんでした。 |
| `WINGET_UNAVAILABLE` | `unusable` | WinGet is unavailable, so automatic installation cannot continue. | WinGet 不可用，因此无法继续自动安装。 | WinGet を利用できないため、自動インストールを続行できません。 |
| `NETWORK_UNAVAILABLE` | `error` | The network check failed, but local scan results can still be shown. | 网络检测失败，但仍可展示本地扫描结果。 | ネットワーク確認に失敗しましたが、ローカルスキャン結果は表示できます。 |
| `USER_CANCELLED_UAC` | `error` | The administrator permission prompt was cancelled by the user. | 用户取消了管理员权限确认。 | ユーザーが管理者権限の確認をキャンセルしました。 |
| `INSTALL_FAILED` | `error` | The installation did not finish successfully. | 安装未成功完成。 | インストールが正常に完了しませんでした。 |
| `VERIFY_FAILED` | `error` | Verification failed after repair or setup. | 修复或设置后验证失败。 | 修復または設定後の検証に失敗しました。 |
| `LOCAL_SERVER_FAILED` | `error` | The local Node test server did not start or did not respond. | 本地 Node 测试服务未启动或没有响应。 | ローカル Node テストサーバーが起動しないか応答しませんでした。 |
| `BROWSER_OPEN_FAILED` | `error` | The default browser could not be opened automatically. | 无法自动打开默认浏览器。 | 既定のブラウザーを自動で開けませんでした。 |
| `VSCODE_OPEN_FAILED` | `error` | VS Code could not open the test project automatically. | VS Code 无法自动打开测试项目。 | VS Code でテストプロジェクトを自動で開けませんでした。 |
| `UNKNOWN_ERROR` | `error` | An unexpected error occurred. Please retry or use the manual steps. | 发生未知错误。请重试或使用手动步骤。 | 予期しないエラーが発生しました。再試行するか手動手順を使ってください。 |

## Telemetry Privacy Boundary

Telemetry is optional and anonymous. If consent is off, no telemetry event may be sent.

Telemetry must never include:

- Full file paths or folder paths.
- Usernames or account names.
- Environment variable names or values.
- Project names, project content, source code, prompts, files, or directory listings.
- Email addresses.
- Secrets, API keys, tokens, credentials, cookies, or authorization headers.
- Raw command output, raw logs, crash dumps, screenshots, or clipboard content.

Allowed telemetry fields are constrained to stable product metadata, coarse OS facts, coarse result states, stable error codes, anonymous installation id, anonymous session id, event timestamp, and numeric durations.

## Common Telemetry Fields

All telemetry events share this base shape:

| Field | Type | Required | Privacy rule |
| --- | --- | --- | --- |
| `event_name` | string | Yes | Must match the event dictionary. |
| `event_version` | integer | Yes | Starts at `1`. |
| `occurred_at` | ISO-8601 string | Yes | Event timestamp. |
| `app_version` | string | Yes | Product version only. |
| `installation_id` | string | Yes | Random local UUID, not derived from user or device identity. |
| `session_id` | string | Yes | Random per-run UUID. |
| `os_name` | string | Yes | `windows`. |
| `os_major_version` | string | Optional | Coarse version such as `10` or `11`; no build path or username. |
| `architecture` | string | Optional | Coarse architecture such as `x64`. |
| `language` | string | Optional | One of `en`, `zh-CN`, `ja`. |
| `consent_state` | string | Yes | `enabled` or `disabled`; disabled prevents outbound events. |

## Telemetry Event Dictionary

The MVP event dictionary contains 14 events.

| Event | Trigger | Additional fields | Privacy boundary |
| --- | --- | --- | --- |
| `app_opened` | Process starts and the main window is created. | `launch_result` | No executable path or command line. |
| `startup_check_completed` | Startup self-check finishes. | `overall_state`, `error_code`, `duration_ms` | No local folder paths or raw errors. |
| `language_selected` | User changes or confirms language. | `selected_language`, `source` | Language code only. |
| `telemetry_consent_changed` | User enables or disables anonymous telemetry. | `new_consent_state`, `source` | Consent state only. If disabled, this is the last allowed outbound event. |
| `scan_started` | User starts the read-only environment scan. | `scan_id` | Random scan id only. |
| `tool_check_completed` | One tool or capability check finishes. | `scan_id`, `tool`, `state`, `error_code`, `duration_ms` | No detected paths, command output, registry values, or project data. |
| `scan_completed` | All read-only scan checks finish. | `scan_id`, `overall_state`, `missing_required_count`, `unusable_required_count`, `unsupported_count`, `duration_ms` | Counts and stable states only. |
| `repair_plan_created` | A repair plan is generated from scan results. | `scan_id`, `repairable_count`, `manual_count`, `requires_uac_possible` | No package source beyond approved tool ids. |
| `repair_started` | User confirms and starts an approved repair action. | `repair_id`, `tool`, `method` | Method is `winget` or `manual`; no command line. |
| `repair_step_completed` | One repair step finishes. | `repair_id`, `tool`, `result`, `error_code`, `duration_ms` | No installer logs, raw output, or paths. |
| `repair_completed` | Repair flow ends. | `repair_id`, `overall_state`, `completed_count`, `failed_count`, `duration_ms` | Aggregated counts and states only. |
| `verification_started` | Post-repair local verification starts. | `verification_id` | Random verification id only. |
| `verification_completed` | Local test project and service verification ends. | `verification_id`, `overall_state`, `error_code`, `duration_ms` | No project path, port can be omitted or bucketed. |
| `ready_reached` | User reaches the Ready page after successful verification. | `verified_tool_count`, `duration_from_open_ms` | Counts and duration only. |

## Implementation Rules

- UI copy must localize error explanations through the error code table instead of inventing one-off messages.
- Repair flows must only operate on approved MVP tools: VS Code, Git, Node.js, npm, and WinGet-mediated setup where allowed.
- A scan result page must not start installation automatically.
- Verification can only pass after the relevant checks are executed again.
- Telemetry consent must be checked before enqueueing or sending every event.
- Logs may contain local debugging details on the user's machine, but telemetry must use the sanitized event schema above.
