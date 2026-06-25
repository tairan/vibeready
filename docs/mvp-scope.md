# VibeReady MVP Scope

Status: frozen for MVP execution
Owner: Product, design, and engineering
Source Linear issue: CHI-13

## MVP Objective

VibeReady MVP is a Windows local web vibe coding environment check and repair tool for non-software users.

The MVP success path is:

1. User downloads a ZIP.
2. User extracts the ZIP.
3. User double-clicks `VibeReady.exe`.
4. User chooses or confirms language.
5. User chooses whether to send anonymous telemetry.
6. VibeReady scans the required local environment for web vibe coding.
7. VibeReady checks VS Code, Git, Node.js, npm, WinGet, network basics, system support, and permission state.
8. User reviews scan results.
9. User reviews a repair plan.
10. User confirms repair before any installation runs.
11. VibeReady repairs missing required items where automatic repair is supported.
12. VibeReady re-detects the environment after repair.
13. VibeReady creates and runs a minimal local Node.js test project.
14. VibeReady opens the local test page in the default browser and opens the test project in VS Code.
15. User reaches the Ready page.

The MVP is complete only when this local loop works without requiring a preinstalled developer environment.

## Supported Platform

MVP support is limited to:

- Windows 10 x64.
- Windows 11 x64.
- ZIP distribution.
- No installer.
- No background service.
- No tray process.
- No automatic update system.

Non-Windows platforms and non-x64 platforms are out of scope for MVP.

## Startup And Dependency Constraints

The distributed application must start from an extracted ZIP by double-clicking `VibeReady.exe`.

Startup must not require the user to preinstall:

- Node.js.
- npm.
- Python.
- Git.
- VS Code.
- WebView.
- Electron.
- Tauri.
- Java.
- .NET runtime.
- Any package manager or developer runtime.

If later engineering work needs a dependency that could affect ZIP startup, Windows 10/11 x64 support, or runtime installation requirements, that dependency must be explicitly reviewed before implementation.

## Required MVP Features

The MVP includes only the minimum capabilities needed for web vibe coding environment readiness:

- English, Simplified Chinese, and Japanese UI.
- Startup self-check.
- Anonymous telemetry consent.
- Read-only environment scan before repair.
- Windows 10/11 x64 support detection.
- Permission status detection.
- VS Code detection.
- Git detection and `git init` usability validation.
- Node.js detection.
- npm detection.
- WinGet detection.
- Basic network detection.
- Scan result page.
- Repair plan page.
- Controlled WinGet installation executor.
- VS Code automatic installation adapter.
- Git automatic installation adapter.
- Node.js LTS automatic installation adapter.
- Manual installation fallback.
- Post-install re-detection.
- PATH refresh or re-detection strategy.
- Minimal local Node.js test project.
- `npm run dev` / Node HTTP service verification.
- Default browser launch for the local test page.
- VS Code launch for the test project.
- Ready page.
- Three-language next-step prompts.
- Cloudflare Workers + D1 anonymous telemetry receiver.
- Client anonymous event queue.
- Telemetry consent control.
- Client-side event redaction.
- QA test matrix.
- Code signing.
- Release ZIP build.
- Three-language README.
- External MVP release checklist.

## Explicit Scope-Out List

The MVP must not include:

- Python environment detection.
- Python automatic repair.
- Course codes.
- Tutorial codes.
- Cloud recipes.
- Project folder recognition.
- README parsing.
- Cursor detection.
- Trae detection.
- Windsurf detection.
- Docker.
- WSL.
- GitHub login.
- GitHub CLI.
- pnpm.
- yarn.
- Database setup for local user projects.
- Teacher dashboard.
- Share links.
- Success cards beyond the Ready page.
- Account system.
- Auto-update.
- Plugin system.
- System tray.
- Background resident service.
- Runtime LLM features.
- OpenAI API calls.
- Anthropic API calls.
- Google model API calls.
- Azure OpenAI API calls.
- Local model calls.
- Any cloud-controlled command execution.

These items remain out of scope even if they are easy to implement.

## Safety And Privacy Boundaries

Scanning must be read-only. VibeReady must not install software, modify system configuration, or run repair commands before the user reaches a repair plan and confirms repair.

Automatic installation must use local deterministic rules and a local package whitelist. Telemetry or any cloud endpoint must never control install commands.

Telemetry must be anonymous, optional, non-blocking, and sent only after consent. If the user rejects telemetry, no telemetry events may be sent.

Telemetry may include coarse environment and funnel data such as anonymous installation ID, app version, coarse Windows version, architecture, event name, tool status, error code, scan duration, repair phase, and whether Ready was reached.

Telemetry must not include usernames, machine names, full paths, project paths, project code, README content, raw environment variables, tokens, keys, secrets, SSH keys, npm tokens, GitHub tokens, email addresses, or complete raw command logs.

## Product, Design, And Engineering Alignment

Product, design, and engineering must use this document as the MVP boundary.

Product must not add user stories outside the required MVP features without changing the frozen scope through Linear.

Design must keep the UI focused on one primary action: checking the local web vibe coding environment. Copy must be understandable to non-software users and must not rely on unexplained technical terms in default explanations.

Engineering must implement only the active Linear issue scope, preserve the no-LLM and no-runtime-startup constraints, and stop for clarification if a request conflicts with this document, Linear, or repository facts.
