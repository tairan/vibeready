# CHI-15 MVP Page Flow Freeze

Linear issue: CHI-15, `[M0] 冻结页面流与三语言术语表`

## Scope

This document freezes the MVP interaction loop for VibeReady. It is the product and implementation reference for the first Windows portable client.

The MVP has one primary entry point only:

> 检查网页 vibe coding 环境

No other screen may introduce an equivalent primary CTA for starting the environment check. Secondary actions are allowed only for language, telemetry, settings, manual installation guidance, retry, or exit/navigation.

## Screens

| Order | Screen ID | Route | Purpose | Primary action |
| --- | --- | --- | --- | --- |
| 1 | `startup` | `/startup` | Initial launch, supported OS bootstrap, preference load. | Continue automatically when supported |
| 2 | `unsupported_system` | `/unsupported-system` | Blocks unsupported OS and explains Windows requirement. | Close |
| 3 | `language_telemetry` | `/language-telemetry` | Collects language and telemetry consent before first scan. | Save preferences |
| 4 | `home` | `/home` | Landing workspace with the only environment-check entry. | 检查网页 vibe coding 环境 |
| 5 | `scanning` | `/scanning` | Shows deterministic scanning progress. | None |
| 6 | `scan_results` | `/scan-results` | Presents Git, Node.js, npm, and VS Code readiness. | Review fix plan |
| 7 | `fix_plan` | `/fix-plan` | Explains actions and asks for user confirmation. | Start fix |
| 8 | `fix_progress` | `/fix-progress` | Shows install/repair progress and recoverable failures. | None |
| 9 | `manual_steps` | `/manual-steps` | Provides manual installation steps when automatic repair is unavailable. | Open official instructions |
| 10 | `verifying` | `/verifying` | Verifies repaired tools after automatic or manual action. | None |
| 11 | `verification_failed` | `/verification-failed` | Explains failed verification and next action. | Try again |
| 12 | `ready` | `/ready` | Confirms the machine is ready for web vibe coding. | Open VS Code |
| 13 | `settings` | `/settings` | Allows language and telemetry changes after setup. | Save settings |

## Flow

1. `startup` checks whether the host OS is supported.
2. Unsupported systems go to `unsupported_system` and stop.
3. Supported first launch goes to `language_telemetry`; returning users go to `home`.
4. `language_telemetry` saves language and telemetry preference, then opens `home`.
5. `home` exposes the only primary check entry: `检查网页 vibe coding 环境`.
6. The check opens `scanning`.
7. `scanning` opens `scan_results` when all tool states are known.
8. If all tools are ready, `scan_results` can continue directly to `ready`.
9. If repairable items exist, `scan_results` opens `fix_plan`.
10. `fix_plan` starts `fix_progress` only after user confirmation.
11. `fix_progress` opens `verifying` after automatic actions complete.
12. If automatic repair is unsupported, blocked, or declined, `fix_progress` or `scan_results` opens `manual_steps`.
13. `manual_steps` returns to `verifying` after the user completes manual action.
14. `verifying` opens `ready` on success.
15. `verifying` opens `verification_failed` on failure.
16. `verification_failed` can retry `scanning` or open `manual_steps`.
17. `settings` is reachable from `home`, `ready`, and non-blocking error screens. Saving settings returns to the previous screen.

## Non-Goals

- No project creation flow.
- No editor plugin installation flow.
- No background resident service.
- No account sign-in.
- No extra scan entry points beyond the single home primary action.
