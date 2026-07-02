#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <shellapi.h>

#include "ui_foundation.h"

#include <algorithm>
#include <cwctype>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kWindowClassName[] = L"VibeReadyMainWindow";
constexpr wchar_t kWindowTitle[] = L"VibeReady";
constexpr wchar_t kSingleInstanceMutex[] = L"Local\\VibeReadySingleInstance";
constexpr UINT_PTR kStartupTimerId = 1;
constexpr UINT_PTR kScanTimerId = 2;
constexpr UINT_PTR kRepairTimerId = 3;
constexpr UINT_PTR kVerificationTimerId = 4;

enum class AppRoute {
    Startup,
    LanguageTelemetry,
    Settings,
    Home,
    Scanning,
    ScanResults,
    FixPlan,
    FixProgress,
    ManualSteps,
    Verifying,
    VerificationFailed,
    Ready,
};

enum class UiControl {
    None,
    BackButton,
    LanguageSelect,
    TelemetryToggle,
    ThemeToggle,
    PrimaryButton,
    SettingsButton,
    TechnicalDetailsButton,
    ExitButton,
};

enum class Locale {
    En,
    ZhCn,
    Ja,
};

enum class ToolState {
    Ready,
    Missing,
    Unusable,
    Unsupported,
    Error,
};

struct ScanResult {
    std::wstring tool;
    ToolState state = ToolState::Error;
    std::wstring errorCode;
    std::wstring detectedVersion;
    std::wstring detailKey;
    bool canAutoRepair = false;
    SYSTEMTIME checkedAt = {};
};

struct SystemScanDetails {
    DWORD majorVersion = 0;
    DWORD minorVersion = 0;
    DWORD buildNumber = 0;
    WORD nativeArchitecture = PROCESSOR_ARCHITECTURE_UNKNOWN;
    bool isWindows10Or11 = false;
    bool isX64 = false;
    bool isAdministrator = false;
    bool mayRequireUacForInstall = true;
};

struct VscodeScanResult {
    ToolState state = ToolState::Missing;
    std::wstring errorCode = L"VSCODE_NOT_FOUND";
    std::wstring detectedVersion;
    std::wstring detectionSource;
    std::wstring executablePath;
    bool cliAvailable = false;
    bool canAutoRepair = true;
    SYSTEMTIME checkedAt = {};
};

struct GitScanResult {
    ToolState state = ToolState::Missing;
    std::wstring errorCode = L"GIT_NOT_FOUND";
    bool versionCommandSucceeded = false;
    bool initSucceeded = false;
    bool canAutoRepair = true;
    SYSTEMTIME checkedAt = {};
};

struct CommandCheckResult {
    std::wstring tool;
    ToolState state = ToolState::Missing;
    std::wstring errorCode;
    std::wstring detectedVersion;
    bool canAutoRepair = true;
    SYSTEMTIME checkedAt = {};
};

enum class ResultCategory {
    MustFix,
    Ready,
    Manual,
};

enum class RepairTool {
    Vscode,
    Git,
    NodeJs,
};

enum class RepairStepState {
    Pending,
    Running,
    Succeeded,
    Failed,
    Manual,
};

struct ResultPageItem {
    std::wstring name;
    ToolState state = ToolState::Error;
    std::wstring errorCode;
    std::wstring reason;
    std::wstring nextStep;
    std::wstring detectedVersion;
    ResultCategory category = ResultCategory::Manual;
};

struct RepairPlanItem {
    RepairTool tool = RepairTool::Vscode;
    std::wstring name;
    std::wstring packageId;
    std::wstring reason;
    std::wstring source;
    std::wstring scanErrorCode;
    std::wstring resultErrorCode;
    DWORD exitCode = 0;
    bool manualOnly = false;
    bool mayRequireUac = true;
    RepairStepState stepState = RepairStepState::Pending;
};

struct Copy {
    const wchar_t* languageName;
    const wchar_t* startupTitle;
    const wchar_t* startupBody;
    const wchar_t* startupHint;
    const wchar_t* homeBody;
    const wchar_t* trustNote;
    const wchar_t* primaryAction;
    const wchar_t* checkingAction;
    const wchar_t* languageLabel;
    const wchar_t* languageTelemetryTitle;
    const wchar_t* languageTelemetryBody;
    const wchar_t* savePreferencesAction;
    const wchar_t* settingsAction;
    const wchar_t* themeLabel;
    const wchar_t* toggleOn;
    const wchar_t* toggleOff;
    const wchar_t* scanningTitle;
    const wchar_t* scanningBody;
    const wchar_t* scanResultsTitle;
    const wchar_t* scanResultsBody;
    const wchar_t* scanResultsAction;
    const wchar_t* technicalDetailsAction;
    const wchar_t* technicalDetailsLabel;
    const wchar_t* hideTechnicalDetailsAction;
    const wchar_t* telemetryLabel;
    const wchar_t* telemetryOn;
    const wchar_t* telemetryOff;
    const wchar_t* settingsSaved;
    const wchar_t* unsupportedLanguageTelemetry;
    const wchar_t* diagnosticsTitle;
    const wchar_t* supported;
    const wchar_t* unsupported;
    const wchar_t* configWarning;
    const wchar_t* logWarning;
    const wchar_t* unsupportedSystemTitle;
    const wchar_t* unsupportedSystemBody;
    const wchar_t* administrator;
    const wchar_t* standardUser;
    const wchar_t* uacMayAppear;
    const wchar_t* uacNotExpected;
    const wchar_t* cliAvailable;
    const wchar_t* cliNotRequired;
    const wchar_t* gitVersionOk;
    const wchar_t* gitInitOk;
    const wchar_t* readyToStart;
    const wchar_t* missingCountPrefix;
    const wchar_t* missingCountSuffix;
    const wchar_t* mustFixHeader;
    const wchar_t* readyHeader;
    const wchar_t* manualHeader;
    const wchar_t* reasonLabel;
    const wchar_t* currentStateLabel;
    const wchar_t* errorCodeLabel;
    const wchar_t* nextStepLabel;
    const wchar_t* rescanAction;
    const wchar_t* nextInstall;
    const wchar_t* nextManual;
    const wchar_t* nextRetry;
    const wchar_t* reasonReady;
    const wchar_t* reasonMissing;
    const wchar_t* reasonUnusable;
    const wchar_t* reasonUnsupported;
    const wchar_t* reasonError;
    const wchar_t* homeStatus;
};

const Copy& Text(Locale locale) {
    static const Copy en = {
        L"English",
        L"Starting VibeReady",
        L"Checking whether this Windows device can run the portable client.",
        L"Preparing the first-run setup flow.",
        L"Check this device before starting web vibe coding work.",
        L"Scan phase is read-only. VibeReady will not install tools or modify the system until you confirm a fix.",
        L"Check web vibe coding environment",
        L"Checking...",
        L"Language",
        L"Language and telemetry",
        L"Choose the UI language and telemetry preference.",
        L"Save preferences",
        L"Settings",
        L"Dark theme",
        L"On",
        L"Off",
        L"Checking environment",
        L"VibeReady is checking readiness step by step.",
        L"Scan results",
        L"Review what is ready and what still needs attention.",
        L"Back to home",
        L"Show technical details",
        L"Technical details",
        L"Hide technical details",
        L"Send anonymous usage data",
        L"Telemetry: allowed",
        L"Telemetry: off",
        L"Preferences saved.",
        L"Please set both language and telemetry preference first.",
        L"Startup checks",
        L"Supported",
        L"Needs attention",
        L"Preferences cannot be saved, but scanning can continue.",
        L"Local log cannot be written, but scanning can continue.",
        L"This system is not supported",
        L"VibeReady MVP supports Windows 10 x64 and Windows 11 x64.",
        L"Administrator",
        L"Standard user",
        L"Future installs may ask for administrator confirmation.",
        L"Administrator confirmation is not expected for the current scan.",
        L"code command available",
        L"code command not required for MVP readiness",
        L"git --version passed",
        L"git init passed",
        L"Everything required is ready. You can start web vibe coding.",
        L"You still need to fix ",
        L" item(s) before web vibe coding is ready.",
        L"Must fix",
        L"Ready",
        L"Cannot handle automatically",
        L"Reason",
        L"Current status",
        L"Error code",
        L"Next step",
        L"Rescan environment",
        L"Install or repair this required tool, then rescan.",
        L"Use the manual guidance for this item, then rescan.",
        L"Retry the scan after the environment changes.",
        L"Detected and usable.",
        L"Required tool was not found.",
        L"Detected, but not usable for the required check.",
        L"Outside the VibeReady MVP support boundary.",
        L"The check failed; local scan results are still shown.",
        L"Use the button below to run a read-only check. Nothing will be installed or changed before you confirm a fix plan."
    };
    static const Copy zh = {
        L"简体中文",
        L"正在启动 VibeReady",
        L"正在检查这台 Windows 设备是否可以运行免安装客户端。",
        L"正在准备首次配置流程。",
        L"开始网页 vibe coding 工作前，先检查这台设备。",
        L"扫描阶段只读。除非你确认修复，VibeReady 不会安装工具或修改系统。",
        L"检查网页 vibe coding 环境",
        L"正在检查...",
        L"语言",
        L"语言与遥测",
        L"先选择界面语言，再确认是否允许匿名遥测。",
        L"保存偏好设置",
        L"设置",
        L"深色主题",
        L"开启",
        L"关闭",
        L"正在检查环境",
        L"VibeReady 正在按步骤检查环境。",
        L"扫描结果",
        L"查看本机环境可用性，并给出可执行的下一步。",
        L"返回主页",
        L"查看技术详情",
        L"技术详情",
        L"隐藏技术详情",
        L"发送匿名使用数据",
        L"遥测：允许",
        L"遥测：关闭",
        L"偏好设置已保存。",
        L"请先保存语言与遥测偏好。",
        L"启动自检",
        L"支持",
        L"需要注意",
        L"偏好设置无法保存，但仍可继续扫描。",
        L"本地日志无法写入，但仍可继续扫描。",
        L"当前系统不受支持",
        L"VibeReady MVP 支持 Windows 10 x64 和 Windows 11 x64。",
        L"管理员",
        L"标准用户",
        L"后续安装可能会要求管理员确认。",
        L"当前扫描预计不会要求管理员确认。",
        L"code 命令可用",
        L"MVP 就绪不要求 code 命令可用",
        L"git --version 已通过",
        L"git init 已通过",
        L"必需项已就绪，你可以开始网页 vibe coding。",
        L"还差 ",
        L" 项，你就可以开始网页 vibe coding。",
        L"必须修复",
        L"已就绪",
        L"无法自动处理",
        L"原因",
        L"当前状态",
        L"错误码",
        L"下一步",
        L"重新扫描环境",
        L"安装或修复这个必需工具，然后重新扫描。",
        L"按此项的手动说明处理，然后重新扫描。",
        L"环境变化后重新扫描。",
        L"已检测到且可用。",
        L"未找到必需工具。",
        L"已检测到，但无法通过必需检查。",
        L"超出 VibeReady MVP 支持范围。",
        L"检测失败；仍可展示本地扫描结果。",
        L"使用下方按钮运行只读检查。在你确认修复计划前，VibeReady 不会安装或修改任何内容。"
    };
    static const Copy ja = {
        L"日本語",
        L"VibeReady を起動中",
        L"この Windows デバイスでポータブルクライアントを実行できるか確認しています。",
        L"初回セットアップ画面を準備しています。",
        L"Web vibe coding 作業を始める前に、このデバイスをチェックします。",
        L"スキャン段階は読み取り専用です。修復を確認するまで、VibeReady はツールのインストールやシステム変更を行いません。",
        L"Web vibe coding 環境をチェック",
        L"チェック中...",
        L"言語",
        L"言語とテレメトリー",
        L"UI 言語を選び、匿名テレメトリー同意を設定します。",
        L"設定を保存",
        L"設定",
        L"ダークテーマ",
        L"オン",
        L"オフ",
        L"環境をチェック中",
        L"VibeReady は環境を順番にチェックしています。",
        L"スキャン結果",
        L"準備状況の確認と、実行可能な次のアクションを表示します。",
        L"ホームに戻る",
        L"技術詳細を表示",
        L"技術詳細",
        L"技術詳細を隠す",
        L"匿名の利用データを送信する",
        L"テレメトリー: 許可",
        L"テレメトリー: オフ",
        L"設定を保存しました。",
        L"言語とテレメトリー設定を先に保存してください。",
        L"起動チェック",
        L"対応済み",
        L"確認が必要",
        L"設定を保存できませんが、スキャンは続行できます。",
        L"ローカルログを書き込めませんが、スキャンは続行できます。",
        L"このシステムはサポート対象外です",
        L"VibeReady MVP は Windows 10 x64 と Windows 11 x64 に対応しています。",
        L"管理者",
        L"標準ユーザー",
        L"今後のインストールでは管理者確認が表示される可能性があります。",
        L"現在のスキャンでは管理者確認は不要です。",
        L"code コマンドを利用できます",
        L"MVP の準備完了に code コマンドは必須ではありません",
        L"git --version に成功しました",
        L"git init に成功しました",
        L"必要な項目はすべて準備できています。Web vibe coding を開始できます。",
        L"Web vibe coding の準備まであと ",
        L" 件の修復が必要です。",
        L"修復が必要",
        L"準備完了",
        L"自動処理できません",
        L"理由",
        L"現在の状態",
        L"エラーコード",
        L"次の手順",
        L"環境を再スキャン",
        L"この必須ツールをインストールまたは修復してから再スキャンしてください。",
        L"この項目の手動手順に従ってから再スキャンしてください。",
        L"環境を変更した後に再スキャンしてください。",
        L"検出され、利用できます。",
        L"必須ツールが見つかりません。",
        L"検出されましたが、必須チェックには利用できません。",
        L"VibeReady MVP のサポート範囲外です。",
        L"チェックに失敗しました。ローカルスキャン結果は表示できます。",
        L"下のボタンで読み取り専用チェックを実行します。修復計画を確認するまで、インストールや変更は行いません。"
    };

    switch (locale) {
    case Locale::ZhCn:
        return zh;
    case Locale::Ja:
        return ja;
    case Locale::En:
    default:
        return en;
    }
}

std::wstring LocaleCode(Locale locale) {
    switch (locale) {
    case Locale::ZhCn:
        return L"zh-CN";
    case Locale::Ja:
        return L"ja";
    case Locale::En:
    default:
        return L"en";
    }
}

Locale LocaleFromCode(const std::wstring& code) {
    if (code == L"zh-CN") {
        return Locale::ZhCn;
    }
    if (code == L"ja") {
        return Locale::Ja;
    }
    return Locale::En;
}

Locale DetectSystemLocale() {
    wchar_t localeName[LOCALE_NAME_MAX_LENGTH] = {};
    if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) == 0) {
        return Locale::En;
    }

    std::wstring name(localeName);
    if (name.rfind(L"zh", 0) == 0) {
        return Locale::ZhCn;
    }
    if (name.rfind(L"ja", 0) == 0) {
        return Locale::Ja;
    }
    return Locale::En;
}

struct StartupChecks {
    bool supportedWindows = false;
    bool x64 = false;
    bool administrator = false;
    bool mayRequireUacForInstall = true;
    VscodeScanResult vscode;
    GitScanResult git;
    CommandCheckResult node;
    CommandCheckResult npm;
    CommandCheckResult winget;
    CommandCheckResult network;
    bool tempWritable = false;
    bool configWritable = false;
    bool logWritable = false;
    std::wstring configDir;
    std::wstring logPath;
    ScanResult systemResult;
    SystemScanDetails systemDetails;
};

struct Preferences {
    Locale locale = DetectSystemLocale();
    bool telemetryAllowed = false;
    bool darkTheme = false;
    bool languageKnown = false;
    bool telemetryKnown = false;
    bool themeKnown = false;
    bool loaded = false;
    bool saved = false;
};

struct UiControlLayout {
    D2D1_RECT_F rect = D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f);
    bool visible = false;
    bool enabled = true;
};

struct UiLayout {
    UiControlLayout backButton;
    UiControlLayout languageSelect;
    UiControlLayout telemetryToggle;
    UiControlLayout themeToggle;
    UiControlLayout primaryButton;
    UiControlLayout settingsButton;
    UiControlLayout technicalDetailsButton;
    UiControlLayout exitButton;
};

struct RepairState {
    std::vector<RepairPlanItem> plan;
    size_t currentIndex = 0;
    RepairTool manualTool = RepairTool::Vscode;
    bool active = false;
    bool completed = false;
    bool failed = false;
    std::wstring lastMessage;
};

struct VerificationState {
    int step = 0;
    bool active = false;
    bool completed = false;
    bool succeeded = false;
    bool failed = false;
    bool projectCreated = false;
    bool serviceStarted = false;
    bool serviceResponded = false;
    bool browserOpened = false;
    bool vscodeOpened = false;
    bool folderFallbackOpened = false;
    DWORD port = 0;
    std::wstring projectDir;
    std::wstring localUrl;
    std::wstring errorCode;
    std::wstring lastMessage;
    PROCESS_INFORMATION serverProcess = {};
};

struct AppState {
    HINSTANCE instance = nullptr;
    HWND window = nullptr;
    vibeready::ui::Foundation ui;
    UiLayout layout;
    StartupChecks checks;
    Preferences preferences;
    Preferences settingsDraft;
    AppRoute route = AppRoute::Startup;
    AppRoute settingsReturnRoute = AppRoute::Home;
    AppRoute manualReturnRoute = AppRoute::ScanResults;
    int scanStep = 0;
    bool scanningActive = false;
    RepairState repair;
    VerificationState verification;
    bool showTechnicalDetails = false;
    bool initialized = false;
    float dpi = 96.0f;
    UiControl hoveredControl = UiControl::None;
    UiControl pressedControl = UiControl::None;
    UiControl focusedControl = UiControl::None;
    bool trackingMouse = false;
};

AppState g_state;

const Preferences& VisiblePreferences() {
    return g_state.route == AppRoute::Settings ? g_state.settingsDraft : g_state.preferences;
}

Preferences& EditablePreferences() {
    return g_state.route == AppRoute::Settings ? g_state.settingsDraft : g_state.preferences;
}

Locale ActiveLocale() {
    return VisiblePreferences().locale;
}

const Copy& CurrentCopy() {
    return Text(ActiveLocale());
}

std::wstring JoinPath(const std::wstring& left, const std::wstring& right) {
    if (left.empty()) {
        return right;
    }
    if (left.back() == L'\\') {
        return left + right;
    }
    return left + L"\\" + right;
}

std::wstring GetKnownFolder(REFKNOWNFOLDERID folderId) {
    PWSTR rawPath = nullptr;
    if (FAILED(SHGetKnownFolderPath(folderId, KF_FLAG_CREATE, nullptr, &rawPath))) {
        return L"";
    }
    std::wstring path(rawPath);
    CoTaskMemFree(rawPath);
    return path;
}

std::wstring GetEnvironmentValue(const wchar_t* name) {
    DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
    if (needed == 0) {
        return L"";
    }

    std::wstring value(needed, L'\0');
    DWORD written = GetEnvironmentVariableW(name, value.data(), needed);
    if (written == 0 || written >= needed) {
        return L"";
    }
    value.resize(written);
    return value;
}

std::wstring GetAppDataRoot() {
    std::wstring testOverride = GetEnvironmentValue(L"VIBEREADY_APPDATA_DIR");
    if (!testOverride.empty()) {
        return testOverride;
    }
    return GetKnownFolder(FOLDERID_LocalAppData);
}

bool WriteTextFile(const std::wstring& path, const std::wstring& content, bool append) {
    DWORD disposition = append ? OPEN_ALWAYS : CREATE_ALWAYS;
    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    std::string utf8;
    int bytesNeeded = WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bytesNeeded > 1) {
        utf8.resize(static_cast<size_t>(bytesNeeded - 1));
        WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, utf8.data(), bytesNeeded, nullptr, nullptr);
    }

    DWORD written = 0;
    BOOL ok = WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    CloseHandle(file);
    return ok && written == utf8.size();
}

bool DirectoryWritable(const std::wstring& dir) {
    if (dir.empty()) {
        return false;
    }

    std::wstring probe = JoinPath(dir, L"vibeready-write-test.tmp");
    HANDLE file = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    CloseHandle(file);
    DeleteFileW(probe.c_str());
    return true;
}

std::wstring ToLower(std::wstring value) {
    for (wchar_t& ch : value) {
        ch = static_cast<wchar_t>(std::towlower(ch));
    }
    return value;
}

bool ContainsCaseInsensitive(const std::wstring& value, const std::wstring& needle) {
    return ToLower(value).find(ToLower(needle)) != std::wstring::npos;
}

bool FileExists(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring EnvironmentPath(const wchar_t* name) {
    DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
    if (needed == 0) {
        return L"";
    }

    std::wstring value(needed, L'\0');
    DWORD written = GetEnvironmentVariableW(name, value.data(), needed);
    if (written == 0 || written >= needed) {
        return L"";
    }
    value.resize(written);
    return value;
}

std::wstring ExpandRegistryString(const std::wstring& value) {
    DWORD needed = ExpandEnvironmentStringsW(value.c_str(), nullptr, 0);
    if (needed == 0) {
        return value;
    }

    std::wstring expanded(needed, L'\0');
    DWORD written = ExpandEnvironmentStringsW(value.c_str(), expanded.data(), needed);
    if (written == 0 || written > needed) {
        return value;
    }
    expanded.resize(written - 1);
    return expanded;
}

bool QueryRegistryString(HKEY key, const wchar_t* valueName, std::wstring* value) {
    DWORD type = 0;
    DWORD bytes = 0;
    LONG sizeResult = RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &bytes);
    if (sizeResult != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || bytes == 0) {
        return false;
    }

    std::wstring buffer(bytes / sizeof(wchar_t), L'\0');
    LONG readResult = RegQueryValueExW(key, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()), &bytes);
    if (readResult != ERROR_SUCCESS) {
        return false;
    }

    while (!buffer.empty() && buffer.back() == L'\0') {
        buffer.pop_back();
    }

    *value = type == REG_EXPAND_SZ ? ExpandRegistryString(buffer) : buffer;
    return !value->empty();
}

bool SearchCommandOnPath(const wchar_t* command) {
    DWORD needed = SearchPathW(nullptr, command, nullptr, 0, nullptr, nullptr);
    return needed > 0;
}

bool CommandExists(const wchar_t* command) {
    return SearchCommandOnPath(command);
}

std::wstring VscodeVersionOrFallback(const std::wstring& version) {
    return version.empty() ? L"installed" : version;
}

bool DetectVscodeFromCommonPaths(VscodeScanResult* result) {
    std::vector<std::wstring> candidates;
    std::wstring localAppData = EnvironmentPath(L"LOCALAPPDATA");
    std::wstring programFiles = EnvironmentPath(L"ProgramFiles");
    std::wstring programFilesX86 = EnvironmentPath(L"ProgramFiles(x86)");

    if (!localAppData.empty()) {
        candidates.push_back(JoinPath(localAppData, L"Programs\\Microsoft VS Code\\Code.exe"));
    }
    if (!programFiles.empty()) {
        candidates.push_back(JoinPath(programFiles, L"Microsoft VS Code\\Code.exe"));
    }
    if (!programFilesX86.empty()) {
        candidates.push_back(JoinPath(programFilesX86, L"Microsoft VS Code\\Code.exe"));
    }

    for (const std::wstring& candidate : candidates) {
        if (FileExists(candidate)) {
            result->state = ToolState::Ready;
            result->errorCode.clear();
            result->detectedVersion = L"installed";
            result->detectionSource = L"common install path";
            result->executablePath = candidate;
            result->canAutoRepair = false;
            return true;
        }
    }

    return false;
}

bool DetectVscodeFromUninstallRegistry(VscodeScanResult* result) {
    const HKEY roots[] = {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE};
    const wchar_t* uninstallPaths[] = {
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
    };

    for (HKEY root : roots) {
        for (const wchar_t* uninstallPath : uninstallPaths) {
            HKEY uninstallKey = nullptr;
            if (RegOpenKeyExW(root, uninstallPath, 0, KEY_READ, &uninstallKey) != ERROR_SUCCESS) {
                continue;
            }

            DWORD index = 0;
            wchar_t subkeyName[256] = {};
            DWORD subkeyLength = ARRAYSIZE(subkeyName);
            while (RegEnumKeyExW(uninstallKey, index, subkeyName, &subkeyLength, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                HKEY appKey = nullptr;
                if (RegOpenKeyExW(uninstallKey, subkeyName, 0, KEY_READ, &appKey) == ERROR_SUCCESS) {
                    std::wstring displayName;
                    if (QueryRegistryString(appKey, L"DisplayName", &displayName) &&
                        ContainsCaseInsensitive(displayName, L"Visual Studio Code")) {
                        std::wstring displayVersion;
                        QueryRegistryString(appKey, L"DisplayVersion", &displayVersion);
                        result->state = ToolState::Ready;
                        result->errorCode.clear();
                        result->detectedVersion = VscodeVersionOrFallback(displayVersion);
                        result->detectionSource = L"Windows uninstall registry";
                        std::wstring installLocation;
                        if (QueryRegistryString(appKey, L"InstallLocation", &installLocation)) {
                            std::wstring codePath = JoinPath(installLocation, L"Code.exe");
                            if (FileExists(codePath)) {
                                result->executablePath = codePath;
                            }
                        }
                        result->canAutoRepair = false;
                        RegCloseKey(appKey);
                        RegCloseKey(uninstallKey);
                        return true;
                    }
                    RegCloseKey(appKey);
                }

                ++index;
                subkeyLength = ARRAYSIZE(subkeyName);
                subkeyName[0] = L'\0';
            }

            RegCloseKey(uninstallKey);
        }
    }

    return false;
}

VscodeScanResult RunVscodeScan() {
    VscodeScanResult result;
    GetLocalTime(&result.checkedAt);
    result.cliAvailable = SearchCommandOnPath(L"code.cmd") || SearchCommandOnPath(L"code.exe");

    if (DetectVscodeFromCommonPaths(&result)) {
        return result;
    }
    if (DetectVscodeFromUninstallRegistry(&result)) {
        return result;
    }

    return result;
}

bool DeleteDirectoryTree(const std::wstring& dir) {
    if (dir.empty()) {
        return false;
    }

    std::wstring search = JoinPath(dir, L"*");
    WIN32_FIND_DATAW data = {};
    HANDLE find = FindFirstFileW(search.c_str(), &data);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            std::wstring name(data.cFileName);
            if (name == L"." || name == L"..") {
                continue;
            }

            std::wstring child = JoinPath(dir, name);
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                DeleteDirectoryTree(child);
            } else {
                SetFileAttributesW(child.c_str(), FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(child.c_str());
            }
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }

    SetFileAttributesW(dir.c_str(), FILE_ATTRIBUTE_NORMAL);
    return RemoveDirectoryW(dir.c_str()) != 0;
}

bool CreateOwnedTempDirectory(std::wstring* dir) {
    wchar_t tempPath[MAX_PATH + 1] = {};
    DWORD tempLength = GetTempPathW(MAX_PATH, tempPath);
    if (tempLength == 0 || tempLength >= MAX_PATH) {
        return false;
    }

    wchar_t tempFile[MAX_PATH + 1] = {};
    if (GetTempFileNameW(tempPath, L"vrg", 0, tempFile) == 0) {
        return false;
    }
    DeleteFileW(tempFile);
    if (!CreateDirectoryW(tempFile, nullptr)) {
        return false;
    }

    *dir = tempFile;
    return true;
}

std::wstring TrimVersion(std::wstring value) {
    while (!value.empty() && (value.back() == L'\r' || value.back() == L'\n' || value.back() == L' ' || value.back() == L'\t')) {
        value.pop_back();
    }
    size_t start = 0;
    while (start < value.size() && (value[start] == L'\r' || value[start] == L'\n' || value[start] == L' ' || value[start] == L'\t')) {
        ++start;
    }
    if (start > 0) {
        value.erase(0, start);
    }
    if (value.size() > 32) {
        value.resize(32);
    }
    return value;
}

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return L"";
    }

    int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (needed == 0) {
        needed = MultiByteToWideChar(CP_ACP, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (needed == 0) {
            return L"";
        }
        std::wstring fallback(static_cast<size_t>(needed), L'\0');
        MultiByteToWideChar(CP_ACP, 0, value.data(), static_cast<int>(value.size()), fallback.data(), needed);
        return fallback;
    }

    std::wstring wide(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), wide.data(), needed);
    return wide;
}

enum class ProcessRunStatus {
    Started,
    NotStarted,
    TimedOut,
};

ProcessRunStatus RunProcessWithTimeout(const std::wstring& commandLine, const std::wstring& workingDir, DWORD timeoutMs, DWORD* exitCode) {
    std::wstring mutableCommand = commandLine;

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    BOOL created = CreateProcessW(
        nullptr,
        mutableCommand.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        workingDir.empty() ? nullptr : workingDir.c_str(),
        &startup,
        &process);

    if (!created) {
        return ProcessRunStatus::NotStarted;
    }

    DWORD waitResult = WaitForSingleObject(process.hProcess, timeoutMs);
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, 1000);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return ProcessRunStatus::TimedOut;
    }

    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    *exitCode = code;
    return ProcessRunStatus::Started;
}

ProcessRunStatus RunProcessCaptureWithTimeout(const std::wstring& commandLine, DWORD timeoutMs, DWORD* exitCode, std::wstring* output) {
    SECURITY_ATTRIBUTES security = {};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &security, 0)) {
        return ProcessRunStatus::NotStarted;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    std::wstring mutableCommand = commandLine;
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    PROCESS_INFORMATION process = {};

    BOOL created = CreateProcessW(
        nullptr,
        mutableCommand.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startup,
        &process);

    CloseHandle(writePipe);
    if (!created) {
        CloseHandle(readPipe);
        return ProcessRunStatus::NotStarted;
    }

    DWORD waitResult = WaitForSingleObject(process.hProcess, timeoutMs);
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, 1000);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        CloseHandle(readPipe);
        return ProcessRunStatus::TimedOut;
    }

    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);

    std::string captured;
    char buffer[256] = {};
    DWORD read = 0;
    while (captured.size() < 1024 && ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0) {
        captured.append(buffer, buffer + read);
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(readPipe);
    *exitCode = code;
    *output = TrimVersion(Utf8ToWide(captured));
    return ProcessRunStatus::Started;
}

CommandCheckResult RunVersionCommandCheck(const wchar_t* tool, const wchar_t* command, const wchar_t* commandLine, const wchar_t* missingCode) {
    constexpr DWORD kToolTimeoutMs = 5000;
    CommandCheckResult result;
    result.tool = tool;
    result.errorCode = missingCode;
    GetLocalTime(&result.checkedAt);

    if (!CommandExists(command)) {
        result.state = ToolState::Missing;
        return result;
    }

    DWORD exitCode = 1;
    std::wstring version;
    ProcessRunStatus status = RunProcessCaptureWithTimeout(commandLine, kToolTimeoutMs, &exitCode, &version);
    if (status == ProcessRunStatus::TimedOut) {
        result.state = ToolState::Unusable;
        result.errorCode = L"TOOL_TIMEOUT";
        return result;
    }
    if (status != ProcessRunStatus::Started || exitCode != 0) {
        result.state = ToolState::Unusable;
        return result;
    }

    result.state = ToolState::Ready;
    result.errorCode.clear();
    result.detectedVersion = version.empty() ? L"detected" : version;
    result.canAutoRepair = false;
    return result;
}

CommandCheckResult RunWingetCheck() {
    constexpr DWORD kToolTimeoutMs = 5000;
    CommandCheckResult result;
    result.tool = L"winget";
    result.errorCode = L"WINGET_UNAVAILABLE";
    GetLocalTime(&result.checkedAt);

    if (!CommandExists(L"winget.exe")) {
        result.state = ToolState::Unusable;
        return result;
    }

    DWORD exitCode = 1;
    std::wstring version;
    ProcessRunStatus status = RunProcessCaptureWithTimeout(L"winget.exe --version", kToolTimeoutMs, &exitCode, &version);
    if (status == ProcessRunStatus::TimedOut) {
        result.state = ToolState::Unusable;
        result.errorCode = L"TOOL_TIMEOUT";
        return result;
    }
    if (status != ProcessRunStatus::Started || exitCode != 0) {
        result.state = ToolState::Unusable;
        return result;
    }

    result.state = ToolState::Ready;
    result.errorCode.clear();
    result.detectedVersion = version.empty() ? L"detected" : version;
    result.canAutoRepair = false;
    return result;
}

CommandCheckResult RunNetworkCheck() {
    CommandCheckResult result;
    result.tool = L"network";
    result.state = ToolState::Unusable;
    result.errorCode = L"NETWORK_UNAVAILABLE";
    GetLocalTime(&result.checkedAt);

    HINTERNET session = WinHttpOpen(L"VibeReady/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        return result;
    }
    WinHttpSetTimeouts(session, 3000, 3000, 3000, 3000);

    HINTERNET connection = WinHttpConnect(session, L"www.microsoft.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connection) {
        WinHttpCloseHandle(session);
        return result;
    }

    HINTERNET request = WinHttpOpenRequest(connection, L"HEAD", L"/", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    BOOL ok = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr);

    if (ok) {
        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX) &&
            statusCode >= 200 && statusCode < 500) {
            result.state = ToolState::Ready;
            result.errorCode.clear();
            result.detectedVersion = L"https reachable";
            result.canAutoRepair = false;
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return result;
}

GitScanResult RunGitScan() {
    constexpr DWORD kToolTimeoutMs = 5000;
    GitScanResult result;
    GetLocalTime(&result.checkedAt);

    if (!CommandExists(L"git.exe")) {
        result.state = ToolState::Missing;
        result.errorCode = L"GIT_NOT_FOUND";
        return result;
    }

    DWORD versionExitCode = 1;
    ProcessRunStatus versionStatus = RunProcessWithTimeout(L"git.exe --version", L"", kToolTimeoutMs, &versionExitCode);
    if (versionStatus == ProcessRunStatus::TimedOut) {
        result.state = ToolState::Unusable;
        result.errorCode = L"TOOL_TIMEOUT";
        return result;
    }
    if (versionStatus != ProcessRunStatus::Started || versionExitCode != 0) {
        result.state = ToolState::Missing;
        result.errorCode = L"GIT_NOT_FOUND";
        return result;
    }
    result.versionCommandSucceeded = true;

    std::wstring tempDir;
    if (!CreateOwnedTempDirectory(&tempDir)) {
        result.state = ToolState::Error;
        result.errorCode = L"TEMP_DIR_UNWRITABLE";
        return result;
    }

    DWORD initExitCode = 1;
    ProcessRunStatus initStatus = RunProcessWithTimeout(L"git.exe init", tempDir, kToolTimeoutMs, &initExitCode);
    DeleteDirectoryTree(tempDir);
    if (initStatus == ProcessRunStatus::TimedOut) {
        result.state = ToolState::Unusable;
        result.errorCode = L"TOOL_TIMEOUT";
        return result;
    }
    if (initStatus != ProcessRunStatus::Started || initExitCode != 0) {
        result.state = ToolState::Unusable;
        result.errorCode = L"GIT_INIT_FAILED";
        return result;
    }

    result.state = ToolState::Ready;
    result.errorCode.clear();
    result.initSucceeded = true;
    result.canAutoRepair = false;
    return result;
}

bool QueryWindowsVersion(RTL_OSVERSIONINFOW* version) {
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) {
        return false;
    }
    auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
    if (!rtlGetVersion) {
        return false;
    }

    version->dwOSVersionInfoSize = sizeof(*version);
    if (rtlGetVersion(version) != 0) {
        return false;
    }
    return true;
}

std::wstring WindowsDisplayVersion(const SystemScanDetails& details) {
    std::wstring name = details.buildNumber >= 22000 ? L"Windows 11" : L"Windows 10";
    return name + L" build " + std::to_wstring(details.buildNumber);
}

bool IsSupportedWindowsVersion(const RTL_OSVERSIONINFOW& version) {
    return version.dwMajorVersion == 10 && version.dwBuildNumber >= 10240;
}

WORD NativeArchitecture() {
    SYSTEM_INFO info = {};
    GetNativeSystemInfo(&info);
    return info.wProcessorArchitecture;
}

std::wstring ArchitectureName(WORD architecture) {
    switch (architecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
        return L"x64";
    case PROCESSOR_ARCHITECTURE_ARM64:
        return L"arm64";
    case PROCESSOR_ARCHITECTURE_INTEL:
        return L"x86";
    default:
        return L"unknown";
    }
}

bool IsAdministrator() {
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID administratorsGroup = nullptr;
    if (!AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administratorsGroup)) {
        return false;
    }

    BOOL isMember = FALSE;
    BOOL ok = CheckTokenMembership(nullptr, administratorsGroup, &isMember);
    FreeSid(administratorsGroup);
    return ok && isMember;
}

ScanResult RunSystemScan(SystemScanDetails* details) {
    ScanResult result;
    result.tool = L"system";
    result.detailKey = L"scan.system";
    result.canAutoRepair = false;
    GetLocalTime(&result.checkedAt);

    RTL_OSVERSIONINFOW version = {};
    if (QueryWindowsVersion(&version)) {
        details->majorVersion = version.dwMajorVersion;
        details->minorVersion = version.dwMinorVersion;
        details->buildNumber = version.dwBuildNumber;
        details->isWindows10Or11 = IsSupportedWindowsVersion(version);
    }

    details->nativeArchitecture = NativeArchitecture();
    details->isX64 = details->nativeArchitecture == PROCESSOR_ARCHITECTURE_AMD64;
    details->isAdministrator = IsAdministrator();
    details->mayRequireUacForInstall = !details->isAdministrator;

    if (!details->isWindows10Or11) {
        result.state = ToolState::Unsupported;
        result.errorCode = L"UNSUPPORTED_WINDOWS_VERSION";
    } else if (!details->isX64) {
        result.state = ToolState::Unsupported;
        result.errorCode = L"UNSUPPORTED_ARCHITECTURE";
    } else {
        result.state = ToolState::Ready;
    }

    if (details->majorVersion > 0) {
        result.detectedVersion = WindowsDisplayVersion(*details) + L" " + ArchitectureName(details->nativeArchitecture);
    } else {
        result.detectedVersion = L"unknown Windows version " + ArchitectureName(details->nativeArchitecture);
    }

    return result;
}

void AppendLog(const std::wstring& message) {
    if (g_state.checks.logPath.empty()) {
        return;
    }
    SYSTEMTIME now = {};
    GetLocalTime(&now);
    wchar_t prefix[64] = {};
    wsprintfW(prefix, L"%04u-%02u-%02u %02u:%02u:%02u ",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    WriteTextFile(g_state.checks.logPath, std::wstring(prefix) + message + L"\r\n", true);
}

bool IsApprovedPackageId(const std::wstring& packageId) {
    return packageId == L"Microsoft.VisualStudioCode" ||
        packageId == L"Git.Git" ||
        packageId == L"OpenJS.NodeJS.LTS";
}

std::wstring QuoteArgument(const std::wstring& value) {
    std::wstring quoted = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'"') {
            quoted += L"\\\"";
        } else {
            quoted += ch;
        }
    }
    quoted += L"\"";
    return quoted;
}

std::wstring InstallErrorFromExitCode(DWORD exitCode) {
    if (exitCode == static_cast<DWORD>(ERROR_CANCELLED) ||
        exitCode == static_cast<DWORD>(HRESULT_FROM_WIN32(ERROR_CANCELLED))) {
        return L"USER_CANCELLED_UAC";
    }
    return L"INSTALL_FAILED";
}

ProcessRunStatus RunWingetInstall(const std::wstring& packageId, DWORD* exitCode) {
    if (!IsApprovedPackageId(packageId)) {
        *exitCode = 1;
        return ProcessRunStatus::NotStarted;
    }

    constexpr DWORD kInstallTimeoutMs = 10 * 60 * 1000;
    std::wstring commandLine = L"winget.exe install --id " + QuoteArgument(packageId) +
        L" --exact --source winget --accept-package-agreements --accept-source-agreements --silent --disable-interactivity";
    return RunProcessWithTimeout(commandLine, L"", kInstallTimeoutMs, exitCode);
}

bool RefreshScanForRepairTool(RepairTool tool, std::wstring* verifyError) {
    switch (tool) {
    case RepairTool::Vscode:
        g_state.checks.vscode = RunVscodeScan();
        if (g_state.checks.vscode.state == ToolState::Ready) {
            return true;
        }
        *verifyError = g_state.checks.vscode.errorCode.empty() ? L"VERIFY_FAILED" : g_state.checks.vscode.errorCode;
        return false;
    case RepairTool::Git:
        g_state.checks.git = RunGitScan();
        if (g_state.checks.git.state == ToolState::Ready) {
            return true;
        }
        *verifyError = g_state.checks.git.errorCode.empty() ? L"VERIFY_FAILED" : g_state.checks.git.errorCode;
        return false;
    case RepairTool::NodeJs:
    default:
        g_state.checks.node = RunVersionCommandCheck(L"node", L"node.exe", L"node.exe --version", L"NODE_NOT_FOUND");
        g_state.checks.npm = RunVersionCommandCheck(L"npm", L"npm.cmd", L"cmd.exe /d /s /c \"npm.cmd --version\"", L"NPM_NOT_FOUND");
        if (g_state.checks.node.state == ToolState::Ready && g_state.checks.npm.state == ToolState::Ready) {
            return true;
        }
        if (g_state.checks.node.state != ToolState::Ready) {
            *verifyError = g_state.checks.node.errorCode.empty() ? L"VERIFY_FAILED" : g_state.checks.node.errorCode;
        } else {
            *verifyError = g_state.checks.npm.errorCode.empty() ? L"VERIFY_FAILED" : g_state.checks.npm.errorCode;
        }
        return false;
    }
}

std::wstring ConfigPath() {
    return JoinPath(g_state.checks.configDir, L"config.ini");
}

void LoadPreferences() {
    std::wstring path = ConfigPath();
    if (path.empty()) {
        return;
    }

    g_state.preferences.languageKnown = false;
    g_state.preferences.telemetryKnown = false;
    g_state.preferences.themeKnown = false;
    g_state.preferences.loaded = false;

    wchar_t locale[32] = {};
    GetPrivateProfileStringW(L"preferences", L"language", L"", locale, 32, path.c_str());
    if (locale[0] != L'\0') {
        g_state.preferences.locale = LocaleFromCode(locale);
        g_state.preferences.languageKnown = true;
        g_state.preferences.loaded = true;
    }

    int telemetry = GetPrivateProfileIntW(L"preferences", L"telemetry", -1, path.c_str());
    if (telemetry >= 0) {
        g_state.preferences.telemetryAllowed = telemetry == 1;
        g_state.preferences.telemetryKnown = true;
        g_state.preferences.loaded = true;
    }

    int theme = GetPrivateProfileIntW(L"preferences", L"darkTheme", -1, path.c_str());
    if (theme >= 0) {
        g_state.preferences.darkTheme = theme == 1;
        g_state.preferences.themeKnown = true;
        g_state.preferences.loaded = true;
    }
}

bool SavePreferences() {
    if (!g_state.checks.configWritable) {
        g_state.preferences.saved = false;
        return false;
    }

    Preferences source = g_state.route == AppRoute::Settings ? g_state.settingsDraft : g_state.preferences;
    std::wstring path = ConfigPath();
    BOOL languageOk = WritePrivateProfileStringW(L"preferences", L"language", LocaleCode(source.locale).c_str(), path.c_str());
    BOOL telemetryOk = WritePrivateProfileStringW(L"preferences", L"telemetry", source.telemetryAllowed ? L"1" : L"0", path.c_str());
    BOOL themeOk = WritePrivateProfileStringW(L"preferences", L"darkTheme", source.darkTheme ? L"1" : L"0", path.c_str());
    bool saved = languageOk && telemetryOk && themeOk;
    if (saved) {
        g_state.preferences = source;
        g_state.preferences.languageKnown = true;
        g_state.preferences.telemetryKnown = true;
        g_state.preferences.themeKnown = true;
    }
    g_state.preferences.saved = saved;
    return saved;
}

bool ArePreferencesComplete() {
    return g_state.preferences.languageKnown && g_state.preferences.telemetryKnown && g_state.preferences.themeKnown;
}

std::wstring L10n(const wchar_t* en, const wchar_t* zh, const wchar_t* ja);
std::vector<RepairPlanItem> BuildRepairPlan();
void StartVerificationFlow();
void AdvanceVerificationFlow();
void StopVerificationServer();
void RefreshUi();

void EnterLanguageTelemetry() {
    g_state.route = AppRoute::LanguageTelemetry;
    g_state.showTechnicalDetails = false;
}

AppRoute SafeSettingsReturnRoute(AppRoute route) {
    switch (route) {
    case AppRoute::Home:
    case AppRoute::ScanResults:
    case AppRoute::VerificationFailed:
    case AppRoute::Ready:
        return route;
    default:
        return AppRoute::Home;
    }
}

void EnterSettings(AppRoute returnRoute) {
    g_state.settingsDraft = g_state.preferences;
    g_state.settingsDraft.saved = false;
    g_state.settingsReturnRoute = SafeSettingsReturnRoute(returnRoute);
    g_state.route = AppRoute::Settings;
    g_state.showTechnicalDetails = false;
}

void EnterHome() {
    g_state.route = AppRoute::Home;
    g_state.showTechnicalDetails = false;
}

void EnterScanResults() {
    if (g_state.window) {
        KillTimer(g_state.window, kRepairTimerId);
        KillTimer(g_state.window, kVerificationTimerId);
    }
    StopVerificationServer();
    g_state.route = AppRoute::ScanResults;
    g_state.showTechnicalDetails = false;
}

void EnterFixPlan() {
    g_state.repair.plan = BuildRepairPlan();
    g_state.repair.currentIndex = 0;
    g_state.repair.active = false;
    g_state.repair.completed = false;
    g_state.repair.failed = false;
    g_state.repair.lastMessage.clear();
    if (!g_state.repair.plan.empty()) {
        g_state.repair.manualTool = g_state.repair.plan.front().tool;
    }
    g_state.route = AppRoute::FixPlan;
    g_state.showTechnicalDetails = false;
}

void EnterManualSteps(RepairTool tool, AppRoute returnRoute = AppRoute::ScanResults) {
    StopVerificationServer();
    g_state.repair.manualTool = tool;
    g_state.repair.active = false;
    g_state.manualReturnRoute = returnRoute;
    g_state.route = AppRoute::ManualSteps;
    g_state.showTechnicalDetails = false;
}

void EnterScanning() {
    if (g_state.window) {
        KillTimer(g_state.window, kRepairTimerId);
        KillTimer(g_state.window, kVerificationTimerId);
    }
    StopVerificationServer();
    g_state.route = AppRoute::Scanning;
    g_state.scanStep = 0;
    g_state.scanningActive = true;
    g_state.repair.active = false;
}

bool CanNavigateBack() {
    switch (g_state.route) {
    case AppRoute::Settings:
    case AppRoute::Scanning:
    case AppRoute::ScanResults:
    case AppRoute::FixPlan:
    case AppRoute::ManualSteps:
    case AppRoute::VerificationFailed:
    case AppRoute::Ready:
        return true;
    case AppRoute::FixProgress:
        return !g_state.repair.active;
    case AppRoute::Verifying:
        return false;
    case AppRoute::Startup:
    case AppRoute::LanguageTelemetry:
    case AppRoute::Home:
    default:
        return false;
    }
}

std::wstring BackLabel() {
    switch (g_state.route) {
    case AppRoute::Scanning:
    case AppRoute::ScanResults:
        return L10n(L"Back to home", L"返回主页", L"ホームに戻る");
    case AppRoute::FixPlan:
    case AppRoute::FixProgress:
    case AppRoute::ManualSteps:
    case AppRoute::VerificationFailed:
    case AppRoute::Ready:
        return L10n(L"Back to results", L"返回结果", L"結果に戻る");
    case AppRoute::Settings:
    default:
        return L10n(L"Back", L"返回", L"戻る");
    }
}

bool NavigateBack() {
    if (!CanNavigateBack()) {
        return false;
    }

    switch (g_state.route) {
    case AppRoute::Settings:
        g_state.route = g_state.settingsReturnRoute;
        g_state.settingsDraft = g_state.preferences;
        g_state.showTechnicalDetails = false;
        AppendLog(L"settings canceled and returned to source route");
        return true;
    case AppRoute::Scanning:
        if (g_state.window) {
            KillTimer(g_state.window, kScanTimerId);
        }
        g_state.scanningActive = false;
        g_state.scanStep = 0;
        EnterHome();
        AppendLog(L"scan_canceled_by_navigation");
        return true;
    case AppRoute::ScanResults:
        EnterHome();
        return true;
    case AppRoute::FixPlan:
    case AppRoute::FixProgress:
    case AppRoute::VerificationFailed:
    case AppRoute::Ready:
        EnterScanResults();
        return true;
    case AppRoute::ManualSteps:
        g_state.route = g_state.manualReturnRoute;
        g_state.showTechnicalDetails = false;
        return true;
    default:
        return false;
    }
}

StartupChecks RunStartupChecks() {
    StartupChecks checks;
    checks.systemResult = RunSystemScan(&checks.systemDetails);
    checks.supportedWindows = checks.systemDetails.isWindows10Or11;
    checks.x64 = checks.systemDetails.isX64;
    checks.administrator = checks.systemDetails.isAdministrator;
    checks.mayRequireUacForInstall = checks.systemDetails.mayRequireUacForInstall;
    checks.vscode = RunVscodeScan();
    checks.git = RunGitScan();
    checks.node = RunVersionCommandCheck(L"node", L"node.exe", L"node.exe --version", L"NODE_NOT_FOUND");
    checks.npm = RunVersionCommandCheck(L"npm", L"npm.cmd", L"cmd.exe /d /s /c \"npm.cmd --version\"", L"NPM_NOT_FOUND");
    checks.winget = RunWingetCheck();
    checks.network = RunNetworkCheck();

    wchar_t tempPath[MAX_PATH + 1] = {};
    DWORD tempLength = GetTempPathW(MAX_PATH, tempPath);
    checks.tempWritable = tempLength > 0 && tempLength < MAX_PATH && DirectoryWritable(tempPath);

    std::wstring localAppData = GetAppDataRoot();
    checks.configDir = JoinPath(localAppData, L"VibeReady");
    if (!checks.configDir.empty()) {
        CreateDirectoryW(checks.configDir.c_str(), nullptr);
        checks.configWritable = DirectoryWritable(checks.configDir);
        checks.logPath = JoinPath(checks.configDir, L"vibeready.log");
        checks.logWritable = WriteTextFile(checks.logPath, L"", true);
    }

    return checks;
}

void FocusExistingInstance() {
    HWND existing = FindWindowW(kWindowClassName, nullptr);
    if (!existing) {
        return;
    }
    if (IsIconic(existing)) {
        ShowWindow(existing, SW_RESTORE);
    }
    SetForegroundWindow(existing);
}

int LocaleIndex(Locale locale) {
    switch (locale) {
    case Locale::ZhCn:
        return 1;
    case Locale::Ja:
        return 2;
    case Locale::En:
    default:
        return 0;
    }
}

Locale LocaleFromIndex(int index) {
    switch (index) {
    case 1:
        return Locale::ZhCn;
    case 2:
        return Locale::Ja;
    case 0:
    default:
        return Locale::En;
    }
}

Locale NextLocale(Locale locale, int direction) {
    int index = LocaleIndex(locale);
    index = (index + direction + 3) % 3;
    return LocaleFromIndex(index);
}

float DipsFromPixels(float pixels) {
    return pixels * 96.0f / std::max(g_state.dpi, 1.0f);
}

D2D1_SIZE_F ClientSizeDips(HWND hwnd) {
    RECT rect = {};
    GetClientRect(hwnd, &rect);
    return D2D1::SizeF(
        DipsFromPixels(static_cast<float>(std::max<LONG>(1, rect.right - rect.left))),
        DipsFromPixels(static_cast<float>(std::max<LONG>(1, rect.bottom - rect.top))));
}

D2D1_POINT_2F PointFromLparam(LPARAM lparam) {
    int x = static_cast<short>(LOWORD(lparam));
    int y = static_cast<short>(HIWORD(lparam));
    return D2D1::Point2F(DipsFromPixels(static_cast<float>(x)), DipsFromPixels(static_cast<float>(y)));
}

bool ContainsPoint(const D2D1_RECT_F& rect, D2D1_POINT_2F point) {
    return point.x >= rect.left && point.x <= rect.right && point.y >= rect.top && point.y <= rect.bottom;
}

bool HighContrastEnabled() {
    HIGHCONTRASTW highContrast = {};
    highContrast.cbSize = sizeof(highContrast);
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(highContrast), &highContrast, 0) &&
        (highContrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

vibeready::ui::ThemeMode CurrentThemeMode() {
    if (HighContrastEnabled()) {
        return vibeready::ui::ThemeMode::HighContrast;
    }
    return VisiblePreferences().darkTheme ? vibeready::ui::ThemeMode::Dark : vibeready::ui::ThemeMode::Light;
}

UiControlLayout& MutableLayout(UiControl control) {
    switch (control) {
    case UiControl::BackButton:
        return g_state.layout.backButton;
    case UiControl::LanguageSelect:
        return g_state.layout.languageSelect;
    case UiControl::TelemetryToggle:
        return g_state.layout.telemetryToggle;
    case UiControl::ThemeToggle:
        return g_state.layout.themeToggle;
    case UiControl::PrimaryButton:
        return g_state.layout.primaryButton;
    case UiControl::SettingsButton:
        return g_state.layout.settingsButton;
    case UiControl::TechnicalDetailsButton:
        return g_state.layout.technicalDetailsButton;
    case UiControl::ExitButton:
        return g_state.layout.exitButton;
    case UiControl::None:
    default:
        return g_state.layout.primaryButton;
    }
}

const UiControlLayout& LayoutFor(UiControl control) {
    return MutableLayout(control);
}

bool ControlVisible(UiControl control) {
    return control != UiControl::None && LayoutFor(control).visible;
}

bool ControlEnabled(UiControl control) {
    return ControlVisible(control) && LayoutFor(control).enabled;
}

std::vector<UiControl> FocusOrder() {
    std::vector<UiControl> controls = {
        UiControl::BackButton,
        UiControl::LanguageSelect,
        UiControl::TelemetryToggle,
        UiControl::ThemeToggle,
        UiControl::PrimaryButton,
        UiControl::SettingsButton,
        UiControl::TechnicalDetailsButton,
        UiControl::ExitButton,
    };
    std::vector<UiControl> visible;
    for (UiControl control : controls) {
        if (ControlEnabled(control)) {
            visible.push_back(control);
        }
    }
    return visible;
}

void EnsureFocusedControl() {
    if (ControlEnabled(g_state.focusedControl)) {
        return;
    }
    std::vector<UiControl> order = FocusOrder();
    g_state.focusedControl = order.empty() ? UiControl::None : order.front();
}

void MoveFocus(int direction) {
    std::vector<UiControl> order = FocusOrder();
    if (order.empty()) {
        g_state.focusedControl = UiControl::None;
        return;
    }

    auto current = std::find(order.begin(), order.end(), g_state.focusedControl);
    int index = current == order.end() ? 0 : static_cast<int>(current - order.begin());
    index = (index + direction + static_cast<int>(order.size())) % static_cast<int>(order.size());
    g_state.focusedControl = order[static_cast<size_t>(index)];
}

UiControl HitTest(D2D1_POINT_2F point) {
    const UiControl controls[] = {
        UiControl::BackButton,
        UiControl::LanguageSelect,
        UiControl::TelemetryToggle,
        UiControl::ThemeToggle,
        UiControl::PrimaryButton,
        UiControl::SettingsButton,
        UiControl::TechnicalDetailsButton,
    };
    for (UiControl control : controls) {
        const UiControlLayout& layout = LayoutFor(control);
        if (layout.visible && layout.enabled && ContainsPoint(layout.rect, point)) {
            return control;
        }
    }
    return UiControl::None;
}

void SetControl(UiControlLayout* control, D2D1_RECT_F rect, bool visible, bool enabled = true) {
    control->rect = rect;
    control->visible = visible;
    control->enabled = enabled;
}

void UpdateLayout(HWND hwnd) {
    D2D1_SIZE_F size = ClientSizeDips(hwnd);
    float width = size.width;
    float height = size.height;
    float margin = width < 720.0f ? 28.0f : 40.0f;
    float right = std::max(margin + 320.0f, width - margin);
    float bottomActionY = std::max(500.0f, height - 76.0f);

    g_state.layout = {};

    if (CanNavigateBack()) {
        SetControl(&g_state.layout.backButton, D2D1::RectF(margin, 20.0f, margin + 184.0f, 64.0f), true);
    }

    if (g_state.route == AppRoute::LanguageTelemetry || g_state.route == AppRoute::Settings) {
        float panelLeft = margin;
        float panelRight = std::min(right, panelLeft + 520.0f);
        float panelTop = g_state.route == AppRoute::Settings ? 218.0f : 198.0f;
        SetControl(&g_state.layout.languageSelect, D2D1::RectF(panelLeft + 20.0f, panelTop + 44.0f, panelRight - 20.0f, panelTop + 88.0f), true);
        SetControl(&g_state.layout.telemetryToggle, D2D1::RectF(panelLeft + 20.0f, panelTop + 102.0f, panelRight - 20.0f, panelTop + 146.0f), true);
        SetControl(&g_state.layout.themeToggle, D2D1::RectF(panelLeft + 20.0f, panelTop + 160.0f, panelRight - 20.0f, panelTop + 204.0f), true);
        SetControl(&g_state.layout.primaryButton, D2D1::RectF(panelLeft + 20.0f, panelTop + 228.0f, panelLeft + 260.0f, panelTop + 272.0f), true);
        if (g_state.route == AppRoute::Settings) {
            SetControl(&g_state.layout.settingsButton, D2D1::RectF(panelLeft + 278.0f, panelTop + 228.0f, panelLeft + 438.0f, panelTop + 272.0f), true);
        }
    } else if (g_state.route == AppRoute::Scanning) {
        SetControl(&g_state.layout.primaryButton, D2D1::RectF(margin, bottomActionY, margin + 220.0f, bottomActionY + 44.0f), true, false);
    } else if (g_state.route == AppRoute::Verifying) {
        SetControl(&g_state.layout.primaryButton, D2D1::RectF(margin, bottomActionY, margin + 220.0f, bottomActionY + 44.0f), true, false);
    } else if (g_state.route == AppRoute::VerificationFailed) {
        SetControl(&g_state.layout.primaryButton, D2D1::RectF(margin, bottomActionY, margin + 190.0f, bottomActionY + 44.0f), true);
        SetControl(&g_state.layout.settingsButton, D2D1::RectF(margin + 208.0f, bottomActionY, margin + 332.0f, bottomActionY + 44.0f), true);
        SetControl(&g_state.layout.technicalDetailsButton, D2D1::RectF(std::max(margin + 350.0f, right - 236.0f), bottomActionY, right, bottomActionY + 44.0f), true);
    } else if (g_state.route == AppRoute::Ready) {
        float firstRowY = bottomActionY - 54.0f;
        SetControl(&g_state.layout.primaryButton, D2D1::RectF(margin, firstRowY, margin + 210.0f, firstRowY + 44.0f), true);
        SetControl(&g_state.layout.settingsButton, D2D1::RectF(margin + 228.0f, firstRowY, margin + 352.0f, firstRowY + 44.0f), true);
        SetControl(&g_state.layout.technicalDetailsButton, D2D1::RectF(margin, bottomActionY, margin + 210.0f, bottomActionY + 44.0f), true);
        SetControl(&g_state.layout.exitButton, D2D1::RectF(margin + 228.0f, bottomActionY, margin + 438.0f, bottomActionY + 44.0f), true);
    } else if (g_state.route == AppRoute::Home || g_state.route == AppRoute::ScanResults) {
        float buttonY = g_state.route == AppRoute::Home ? 250.0f : bottomActionY;
        SetControl(&g_state.layout.primaryButton, D2D1::RectF(margin, buttonY, margin + 306.0f, buttonY + 44.0f), true);
        SetControl(&g_state.layout.settingsButton, D2D1::RectF(margin + 324.0f, buttonY, margin + 448.0f, buttonY + 44.0f), true);
        SetControl(&g_state.layout.technicalDetailsButton, D2D1::RectF(std::max(margin + 466.0f, right - 190.0f), buttonY, right, buttonY + 44.0f), true);
    } else if (g_state.route == AppRoute::FixPlan || g_state.route == AppRoute::FixProgress || g_state.route == AppRoute::ManualSteps) {
        bool repairBusy = g_state.route == AppRoute::FixProgress && g_state.repair.active;
        SetControl(&g_state.layout.primaryButton, D2D1::RectF(margin, bottomActionY, margin + 248.0f, bottomActionY + 44.0f), true, !repairBusy);
        if (g_state.route == AppRoute::FixProgress || g_state.route == AppRoute::ManualSteps) {
            SetControl(&g_state.layout.settingsButton, D2D1::RectF(margin + 266.0f, bottomActionY, margin + 486.0f, bottomActionY + 44.0f), true, !repairBusy);
        }
        if (g_state.route == AppRoute::FixPlan || g_state.route == AppRoute::ManualSteps) {
            SetControl(&g_state.layout.technicalDetailsButton, D2D1::RectF(std::max(margin + 504.0f, right - 236.0f), bottomActionY, right, bottomActionY + 44.0f), true, !repairBusy);
        }
    }

    EnsureFocusedControl();
}

vibeready::ui::ControlState ControlStateFor(UiControl control, bool loading = false) {
    if (loading) {
        return vibeready::ui::ControlState::Loading;
    }
    if (!ControlEnabled(control)) {
        return vibeready::ui::ControlState::Disabled;
    }
    if (g_state.pressedControl == control) {
        return vibeready::ui::ControlState::Pressed;
    }
    if (g_state.hoveredControl == control) {
        return vibeready::ui::ControlState::Hover;
    }
    if (g_state.focusedControl == control && GetFocus() == g_state.window) {
        return vibeready::ui::ControlState::Focused;
    }
    return vibeready::ui::ControlState::Normal;
}

std::wstring L10n(const wchar_t* en, const wchar_t* zh, const wchar_t* ja) {
    switch (ActiveLocale()) {
    case Locale::ZhCn:
        return zh;
    case Locale::Ja:
        return ja;
    case Locale::En:
    default:
        return en;
    }
}

const wchar_t* VerificationStepLabel(int index) {
    switch (index) {
    case 0:
        return L"Refresh installed tool visibility";
    case 1:
        return L"Create local Node test project";
    case 2:
        return L"Run npm run dev and check HTTP";
    case 3:
        return L"Open local page in browser";
    case 4:
    default:
        return L"Open test project in VS Code";
    }
}

std::wstring VerificationStatusValue(int index) {
    if (g_state.verification.failed && g_state.verification.step == index) {
        return L10n(L"failed", L"失败", L"失敗");
    }
    bool done = false;
    switch (index) {
    case 0:
        done = g_state.verification.step > 0 || g_state.verification.succeeded;
        break;
    case 1:
        done = g_state.verification.projectCreated;
        break;
    case 2:
        done = g_state.verification.serviceResponded;
        break;
    case 3:
        done = g_state.verification.browserOpened;
        break;
    case 4:
        done = g_state.verification.vscodeOpened;
        break;
    default:
        break;
    }
    if (done) {
        return L10n(L"passed", L"已通过", L"成功");
    }
    if (g_state.verification.active && g_state.verification.step == index) {
        return L10n(L"running", L"进行中", L"実行中");
    }
    return L10n(L"pending", L"等待中", L"待機中");
}

std::wstring BuildNextPromptTemplate() {
    if (ActiveLocale() == Locale::ZhCn) {
        return L"请帮我创建一个最小网页项目：使用 HTML、CSS 和 JavaScript 做一个清晰的欢迎页，包含标题、按钮和一段说明。请告诉我每一步要运行的命令，并解释如何在本地浏览器中查看结果。";
    }
    if (ActiveLocale() == Locale::Ja) {
        return L"最小のWebプロジェクトを作ってください。HTML、CSS、JavaScriptで見やすいウェルカムページを作り、タイトル、ボタン、短い説明を入れてください。実行するコマンドと、ローカルブラウザーで結果を見る方法も説明してください。";
    }
    return L"Help me create a minimal web project with HTML, CSS, and JavaScript. Build a clear welcome page with a title, a button, and a short explanation. Tell me each command to run and how to view the result in my local browser.";
}

std::wstring BuildVerificationText() {
    std::wstring text = L10n(L"Local verification\r\n", L"本地验证\r\n", L"ローカル検証\r\n");
    text += L10n(
        L"VibeReady checks the actual local run path without downloading dependencies.\r\n\r\n",
        L"VibeReady 会验证真实本地运行路径，不会下载第三方依赖。\r\n\r\n",
        L"VibeReady は依存関係をダウンロードせず、実際のローカル実行経路を確認します。\r\n\r\n");
    for (int i = 0; i < 5; ++i) {
        text += std::wstring(L"• ") + VerificationStepLabel(i) + L": " + VerificationStatusValue(i) + L"\r\n";
    }
    if (!g_state.verification.localUrl.empty()) {
        text += L"\r\n" + L10n(L"Local page: ", L"本地页面：", L"ローカルページ: ") + g_state.verification.localUrl + L"\r\n";
    }
    if (!g_state.verification.errorCode.empty()) {
        text += L"\r\n" + L10n(L"Error code: ", L"错误码：", L"エラーコード: ") + g_state.verification.errorCode + L"\r\n";
    }
    if (!g_state.verification.lastMessage.empty()) {
        text += L"\r\n" + g_state.verification.lastMessage + L"\r\n";
    }
    return text;
}

std::wstring BuildReadyText() {
    std::wstring text = L"Ready\r\n";
    text += L10n(
        L"This computer has passed the required checks and local verification.\r\n",
        L"这台电脑已通过必需项检查和本地验证。\r\n",
        L"このコンピューターは必須チェックとローカル検証に合格しました。\r\n");
    text += L"\r\n";
    text += L"• VS Code: ready\r\n";
    text += L"• Git: ready\r\n";
    text += L"• Node.js: ready\r\n";
    text += L"• npm: ready\r\n";
    if (!g_state.verification.localUrl.empty()) {
        text += L"\r\n" + L10n(L"Local test page: ", L"本地测试页面：", L"ローカルテストページ: ") + g_state.verification.localUrl + L"\r\n";
    }
    text += L"\r\n";
    text += L10n(L"Next prompt template\r\n", L"下一步提示词模板\r\n", L"次のプロンプトテンプレート\r\n");
    text += BuildNextPromptTemplate() + L"\r\n";
    return text;
}

std::wstring RouteTitle() {
    const Copy& copy = CurrentCopy();
    switch (g_state.route) {
    case AppRoute::Startup:
        return copy.startupTitle;
    case AppRoute::LanguageTelemetry:
        return copy.languageTelemetryTitle;
    case AppRoute::Settings:
        return copy.settingsAction;
    case AppRoute::Scanning:
        return copy.scanningTitle;
    case AppRoute::ScanResults:
        return copy.scanResultsTitle;
    case AppRoute::FixPlan:
        return L10n(L"Fix plan", L"修复计划", L"修復プラン");
    case AppRoute::FixProgress:
        return L10n(L"Fixing required tools", L"正在修复必需工具", L"必須ツールを修復中");
    case AppRoute::ManualSteps:
        return L10n(L"Manual installation steps", L"手动安装步骤", L"手動インストール手順");
    case AppRoute::Verifying:
        return L10n(L"Verifying environment", L"正在验证环境", L"環境を検証中");
    case AppRoute::VerificationFailed:
        return L10n(L"Verification failed", L"验证失败", L"検証に失敗しました");
    case AppRoute::Ready:
        return L"Ready";
    case AppRoute::Home:
    default:
        return L"VibeReady";
    }
}

std::wstring RouteBody() {
    const Copy& copy = CurrentCopy();
    switch (g_state.route) {
    case AppRoute::Startup:
        return std::wstring(copy.startupBody) + L" " + copy.startupHint;
    case AppRoute::LanguageTelemetry:
        return copy.languageTelemetryBody;
    case AppRoute::Settings:
        return L10n(
            L"Update language, telemetry, and theme. Save or go back to return to the previous screen.",
            L"更新语言、遥测和主题。保存或返回后会回到来源页面。",
            L"言語、テレメトリー、テーマを更新します。保存または戻ると元の画面に戻ります。");
    case AppRoute::Scanning:
        return copy.scanningBody;
    case AppRoute::ScanResults:
        return copy.scanResultsBody;
    case AppRoute::FixPlan:
        return L10n(
            L"Review exactly what VibeReady will install before anything changes.",
            L"在执行任何安装前，先确认 VibeReady 将安装哪些必需项。",
            L"変更を行う前に、VibeReady がインストールする必須項目を確認します。");
    case AppRoute::FixProgress:
        return L10n(
            L"VibeReady is running only approved WinGet installs and verifying each tool again.",
            L"VibeReady 只会执行已批准的 WinGet 安装，并在每一步后重新验证工具。",
            L"VibeReady は承認済みの WinGet インストールだけを実行し、各ツールを再確認します。");
    case AppRoute::ManualSteps:
        return L10n(
            L"Use official installers when automatic repair is unavailable or fails.",
            L"当自动修复不可用或失败时，请使用官方安装器。",
            L"自動修復が利用できない、または失敗した場合は公式インストーラーを使用します。");
    case AppRoute::Verifying:
        return L10n(
            L"VibeReady is creating a local test project and checking that it really runs.",
            L"VibeReady 正在创建本地测试项目，并确认它真的可以运行。",
            L"VibeReady はローカルテストプロジェクトを作成し、実際に動作するか確認しています。");
    case AppRoute::VerificationFailed:
        return L10n(
            L"One verification step did not complete. No project files outside VibeReady were touched.",
            L"有一个验证步骤未完成。VibeReady 没有访问或修改你的已有项目文件。",
            L"検証手順の一部が完了しませんでした。既存のプロジェクトファイルには触れていません。");
    case AppRoute::Ready:
        return L10n(
            L"This device is ready for web vibe coding with Git, Node.js, npm, and VS Code.",
            L"这台设备已准备好使用 Git、Node.js、npm 和 VS Code 进行网页 vibe coding。",
            L"このデバイスは Git、Node.js、npm、VS Code で Web vibe coding を始められます。");
    case AppRoute::Home:
    default:
        return copy.homeBody;
    }
}

vibeready::ui::StatusTone ToneFor(bool ok) {
    return ok ? vibeready::ui::StatusTone::Success : vibeready::ui::StatusTone::Warning;
}

std::wstring ValueFor(bool ok) {
    const Copy& copy = CurrentCopy();
    return ok ? copy.supported : copy.unsupported;
}

std::wstring StateName(ToolState state) {
    switch (state) {
    case ToolState::Ready:
        return L"ready";
    case ToolState::Missing:
        return L"missing";
    case ToolState::Unusable:
        return L"unusable";
    case ToolState::Unsupported:
        return L"unsupported";
    case ToolState::Error:
    default:
        return L"error";
    }
}

std::wstring ReasonForState(ToolState state) {
    const Copy& copy = CurrentCopy();
    switch (state) {
    case ToolState::Ready:
        return copy.reasonReady;
    case ToolState::Missing:
        return copy.reasonMissing;
    case ToolState::Unusable:
        return copy.reasonUnusable;
    case ToolState::Unsupported:
        return copy.reasonUnsupported;
    case ToolState::Error:
    default:
        return copy.reasonError;
    }
}

ResultCategory CategoryFor(ToolState state, bool canAutoRepair) {
    if (state == ToolState::Ready) {
        return ResultCategory::Ready;
    }
    if ((state == ToolState::Missing || state == ToolState::Unusable) && canAutoRepair) {
        return ResultCategory::MustFix;
    }
    return ResultCategory::Manual;
}

std::wstring NextStepFor(ResultCategory category) {
    const Copy& copy = CurrentCopy();
    switch (category) {
    case ResultCategory::MustFix:
        return copy.nextInstall;
    case ResultCategory::Ready:
        return copy.nextRetry;
    case ResultCategory::Manual:
    default:
        return copy.nextManual;
    }
}

ResultPageItem MakeResultItem(
    const std::wstring& name,
    ToolState state,
    const std::wstring& errorCode,
    const std::wstring& detectedVersion,
    bool canAutoRepair) {
    ResultPageItem item;
    item.name = name;
    item.state = state;
    item.errorCode = errorCode;
    item.detectedVersion = detectedVersion;
    item.reason = ReasonForState(state);
    item.category = CategoryFor(state, canAutoRepair);
    item.nextStep = NextStepFor(item.category);
    return item;
}

std::vector<ResultPageItem> BuildResultItems() {
    std::vector<ResultPageItem> items;
    items.push_back(MakeResultItem(L"Windows", g_state.checks.systemResult.state, g_state.checks.systemResult.errorCode,
        g_state.checks.systemResult.detectedVersion, false));
    items.push_back(MakeResultItem(L"VS Code", g_state.checks.vscode.state, g_state.checks.vscode.errorCode,
        g_state.checks.vscode.detectedVersion, g_state.checks.vscode.canAutoRepair));
    items.push_back(MakeResultItem(L"Git", g_state.checks.git.state, g_state.checks.git.errorCode, L"",
        g_state.checks.git.canAutoRepair));
    items.push_back(MakeResultItem(L"Node.js", g_state.checks.node.state, g_state.checks.node.errorCode,
        g_state.checks.node.detectedVersion, g_state.checks.node.canAutoRepair));
    items.push_back(MakeResultItem(L"npm", g_state.checks.npm.state, g_state.checks.npm.errorCode,
        g_state.checks.npm.detectedVersion, g_state.checks.npm.canAutoRepair));
    items.push_back(MakeResultItem(L"WinGet", g_state.checks.winget.state, g_state.checks.winget.errorCode,
        g_state.checks.winget.detectedVersion, g_state.checks.winget.canAutoRepair));
    items.push_back(MakeResultItem(L"Network", g_state.checks.network.state, g_state.checks.network.errorCode,
        g_state.checks.network.detectedVersion, g_state.checks.network.canAutoRepair));

    if (!g_state.checks.tempWritable) {
        items.push_back(MakeResultItem(L"Temp directory", ToolState::Error, L"TEMP_DIR_UNWRITABLE", L"", false));
    }
    if (!g_state.checks.configWritable) {
        items.push_back(MakeResultItem(L"Config directory", ToolState::Error, L"CONFIG_DIR_UNWRITABLE", L"", false));
    }
    if (!g_state.checks.logWritable) {
        items.push_back(MakeResultItem(L"Local log", ToolState::Error, L"CONFIG_DIR_UNWRITABLE", L"", false));
    }
    return items;
}

std::wstring RepairToolName(RepairTool tool) {
    switch (tool) {
    case RepairTool::Vscode:
        return L"VS Code";
    case RepairTool::Git:
        return L"Git";
    case RepairTool::NodeJs:
    default:
        return L"Node.js LTS";
    }
}

std::wstring RepairPackageId(RepairTool tool) {
    switch (tool) {
    case RepairTool::Vscode:
        return L"Microsoft.VisualStudioCode";
    case RepairTool::Git:
        return L"Git.Git";
    case RepairTool::NodeJs:
    default:
        return L"OpenJS.NodeJS.LTS";
    }
}

std::wstring RepairOfficialUrl(RepairTool tool) {
    switch (tool) {
    case RepairTool::Vscode:
        return L"https://code.visualstudio.com/Download";
    case RepairTool::Git:
        return L"https://git-scm.com/download/win";
    case RepairTool::NodeJs:
    default:
        return L"https://nodejs.org/en/download";
    }
}

std::wstring RepairSourceName(bool manualOnly) {
    if (manualOnly) {
        return L10n(L"Official download page", L"官方下载页面", L"公式ダウンロードページ");
    }
    return L"WinGet";
}

bool RepairCandidateState(ToolState state) {
    return state == ToolState::Missing || state == ToolState::Unusable;
}

RepairPlanItem MakeRepairItem(RepairTool tool, ToolState state, const std::wstring& errorCode) {
    bool automaticUnavailable = g_state.checks.winget.state != ToolState::Ready || g_state.checks.network.state != ToolState::Ready;
    RepairPlanItem item;
    item.tool = tool;
    item.name = RepairToolName(tool);
    item.packageId = RepairPackageId(tool);
    item.scanErrorCode = errorCode;
    item.reason = ReasonForState(state);
    item.manualOnly = automaticUnavailable;
    item.mayRequireUac = g_state.checks.mayRequireUacForInstall && !item.manualOnly;
    item.source = RepairSourceName(item.manualOnly);
    item.stepState = item.manualOnly ? RepairStepState::Manual : RepairStepState::Pending;
    return item;
}

std::vector<RepairPlanItem> BuildRepairPlan() {
    std::vector<RepairPlanItem> plan;
    if (RepairCandidateState(g_state.checks.vscode.state)) {
        plan.push_back(MakeRepairItem(RepairTool::Vscode, g_state.checks.vscode.state, g_state.checks.vscode.errorCode));
    }
    if (RepairCandidateState(g_state.checks.git.state)) {
        plan.push_back(MakeRepairItem(RepairTool::Git, g_state.checks.git.state, g_state.checks.git.errorCode));
    }
    if (RepairCandidateState(g_state.checks.node.state) || RepairCandidateState(g_state.checks.npm.state)) {
        std::wstring error = RepairCandidateState(g_state.checks.node.state) ? g_state.checks.node.errorCode : g_state.checks.npm.errorCode;
        ToolState state = RepairCandidateState(g_state.checks.node.state) ? g_state.checks.node.state : g_state.checks.npm.state;
        plan.push_back(MakeRepairItem(RepairTool::NodeJs, state, error));
    }
    return plan;
}

bool HasAutoRepairItems() {
    for (const RepairPlanItem& item : g_state.repair.plan) {
        if (!item.manualOnly) {
            return true;
        }
    }
    return false;
}

RepairTool FirstManualRepairTool() {
    for (const RepairPlanItem& item : g_state.repair.plan) {
        if (item.manualOnly || item.stepState == RepairStepState::Failed) {
            return item.tool;
        }
    }
    return g_state.repair.plan.empty() ? RepairTool::Vscode : g_state.repair.plan.front().tool;
}

int CountCategory(const std::vector<ResultPageItem>& items, ResultCategory category) {
    int count = 0;
    for (const ResultPageItem& item : items) {
        if (item.category == category) {
            ++count;
        }
    }
    return count;
}

std::wstring BuildSection(const std::vector<ResultPageItem>& items, ResultCategory category, const wchar_t* title) {
    const Copy& copy = CurrentCopy();
    std::wstring text = std::wstring(title) + L"\r\n";
    bool hasItems = false;
    for (const ResultPageItem& item : items) {
        if (item.category != category) {
            continue;
        }
        hasItems = true;
        text += L"  " + item.name + L"\r\n";
        text += L"    " + std::wstring(copy.currentStateLabel) + L": " + StateName(item.state);
        if (!item.detectedVersion.empty()) {
            text += L" (" + item.detectedVersion + L")";
        }
        text += L"\r\n";
        if (category != ResultCategory::Ready) {
            text += L"    " + std::wstring(copy.reasonLabel) + L": " + item.reason + L"\r\n";
            if (!item.errorCode.empty()) {
                text += L"    " + std::wstring(copy.errorCodeLabel) + L": " + item.errorCode + L"\r\n";
            }
            text += L"    " + std::wstring(copy.nextStepLabel) + L": " + item.nextStep + L"\r\n";
        }
    }
    if (!hasItems) {
        text += L"  -\r\n";
    }
    return text;
}

std::wstring BuildStatusText() {
    const Copy& copy = CurrentCopy();
    std::vector<ResultPageItem> items = BuildResultItems();
    int mustFixCount = CountCategory(items, ResultCategory::MustFix);

    std::wstring text = std::wstring(copy.scanResultsTitle) + L"\r\n";
    if (mustFixCount == 0) {
        text += std::wstring(copy.readyToStart) + L"\r\n";
    } else {
        text += std::wstring(copy.missingCountPrefix) + std::to_wstring(mustFixCount) + copy.missingCountSuffix + L"\r\n";
    }
    text += L"\r\n";
    text += BuildSection(items, ResultCategory::MustFix, copy.mustFixHeader);
    text += L"\r\n";
    text += BuildSection(items, ResultCategory::Manual, copy.manualHeader);
    text += L"\r\n";
    text += BuildSection(items, ResultCategory::Ready, copy.readyHeader);
    text += L"\r\n";
    if (!g_state.checks.configWritable) {
        text += std::wstring(L"\r\n") + copy.configWarning;
    } else if (!g_state.checks.logWritable) {
        text += std::wstring(L"\r\n") + copy.logWarning;
    } else if (g_state.preferences.saved) {
        text += std::wstring(L"\r\n") + copy.settingsSaved;
    }
    text += std::wstring(L"\r\n") + (g_state.preferences.telemetryAllowed ? copy.telemetryOn : copy.telemetryOff);
    return text;
}

std::wstring RepairStepStateName(RepairStepState state) {
    switch (state) {
    case RepairStepState::Running:
        return L10n(L"running", L"执行中", L"実行中");
    case RepairStepState::Succeeded:
        return L10n(L"succeeded", L"已成功", L"成功");
    case RepairStepState::Failed:
        return L10n(L"failed", L"失败", L"失敗");
    case RepairStepState::Manual:
        return L10n(L"manual steps", L"手动步骤", L"手動手順");
    case RepairStepState::Pending:
    default:
        return L10n(L"pending", L"等待中", L"待機中");
    }
}

std::wstring BuildRepairPlanText() {
    std::wstring text = L10n(L"Repair plan\r\n", L"修复计划\r\n", L"修復プラン\r\n");
    if (g_state.repair.plan.empty()) {
        text += L10n(
            L"Everything required is already ready. No install action is needed.\r\n",
            L"所有必需项已就绪，不需要安装。\r\n",
            L"必須項目はすべて準備済みです。インストールは不要です。\r\n");
        return text;
    }

    text += L10n(
        L"Nothing will be installed until you confirm Start fix.\r\n\r\n",
        L"点击“开始修复”前不会执行任何安装。\r\n\r\n",
        L"「修復を開始」を確認するまで、何もインストールしません。\r\n\r\n");

    int order = 1;
    for (const RepairPlanItem& item : g_state.repair.plan) {
        text += std::to_wstring(order) + L". " + item.name + L"\r\n";
        text += L"   " + L10n(L"Source: ", L"来源：", L"入手元: ") + item.source;
        if (!item.manualOnly) {
            text += L" (" + item.packageId + L")";
        }
        text += L"\r\n";
        text += L"   " + L10n(L"Reason: ", L"原因：", L"理由: ") + item.reason + L"\r\n";
        if (!item.scanErrorCode.empty()) {
            text += L"   " + L10n(L"Scan error: ", L"扫描错误：", L"スキャンエラー: ") + item.scanErrorCode + L"\r\n";
        }
        text += L"   " + L10n(L"Administrator prompt: ", L"管理员确认：", L"管理者確認: ");
        text += item.mayRequireUac
            ? L10n(L"may appear", L"可能出现", L"表示される可能性あり")
            : L10n(L"not expected", L"预计不会出现", L"想定なし");
        text += L"\r\n";
        if (item.manualOnly) {
            text += L"   " + L10n(
                L"Automatic repair is unavailable now, so this item will use manual steps.\r\n",
                L"当前无法自动修复，此项将进入手动步骤。\r\n",
                L"現在は自動修復を利用できないため、この項目は手動手順を使います。\r\n");
        }
        ++order;
    }
    return text;
}

std::wstring BuildFixProgressText() {
    std::wstring text = L10n(L"Repair progress\r\n", L"修复进度\r\n", L"修復の進行状況\r\n");
    if (g_state.repair.plan.empty()) {
        text += L10n(L"No repair steps are required.\r\n", L"不需要修复步骤。\r\n", L"修復手順は不要です。\r\n");
        return text;
    }

    for (const RepairPlanItem& item : g_state.repair.plan) {
        text += L"• " + item.name + L": " + RepairStepStateName(item.stepState);
        if (!item.resultErrorCode.empty()) {
            text += L" (" + item.resultErrorCode + L")";
        }
        text += L"\r\n";
    }
    if (!g_state.repair.lastMessage.empty()) {
        text += L"\r\n" + g_state.repair.lastMessage + L"\r\n";
    } else if (g_state.repair.active) {
        text += L"\r\n" + L10n(
            L"Please keep this window open while VibeReady waits for WinGet.",
            L"请保持此窗口打开，VibeReady 正在等待 WinGet 完成。",
            L"WinGet の完了を待つ間、このウィンドウを開いたままにしてください。") + L"\r\n";
    }
    return text;
}

std::wstring BuildManualStepsText(RepairTool tool) {
    std::wstring name = RepairToolName(tool);
    std::wstring text = name + L"\r\n";
    if (ActiveLocale() == Locale::ZhCn) {
        text += L"1. 打开官方下载页面。\r\n";
        text += L"2. 下载并运行 Windows 安装器。\r\n";
        text += L"3. 保持默认选项完成安装。\r\n";
        text += L"4. 回到 VibeReady，点击“我已安装，重新扫描”。\r\n";
        if (tool == RepairTool::NodeJs) {
            text += L"Node.js LTS 安装后必须同时包含 node 和 npm。\r\n";
        } else if (tool == RepairTool::Git) {
            text += L"Git 安装后必须能通过 git --version 和 git init。\r\n";
        }
    } else if (ActiveLocale() == Locale::Ja) {
        text += L"1. 公式ダウンロードページを開きます。\r\n";
        text += L"2. Windows インストーラーをダウンロードして実行します。\r\n";
        text += L"3. 既定の選択でインストールを完了します。\r\n";
        text += L"4. VibeReady に戻り、「インストール済み、再スキャン」を選びます。\r\n";
        if (tool == RepairTool::NodeJs) {
            text += L"Node.js LTS のインストール後は node と npm の両方が必要です。\r\n";
        } else if (tool == RepairTool::Git) {
            text += L"Git のインストール後は git --version と git init が成功する必要があります。\r\n";
        }
    } else {
        text += L"1. Open the official download page.\r\n";
        text += L"2. Download and run the Windows installer.\r\n";
        text += L"3. Complete installation with the default options.\r\n";
        text += L"4. Return to VibeReady and choose I installed it, rescan.\r\n";
        if (tool == RepairTool::NodeJs) {
            text += L"Node.js LTS must provide both node and npm after installation.\r\n";
        } else if (tool == RepairTool::Git) {
            text += L"Git must pass both git --version and git init after installation.\r\n";
        }
    }
    text += L"\r\n" + RepairOfficialUrl(tool);
    return text;
}

std::wstring BuildFriendlyStatusText();
std::wstring BuildTechnicalStatusText();

void DrawButtonIfVisible(UiControl control, const std::wstring& text, bool primary, bool loading = false) {
    if (!ControlVisible(control)) {
        return;
    }
    g_state.ui.DrawButton(vibeready::ui::ButtonSpec{
        LayoutFor(control).rect,
        text,
        ControlStateFor(control, loading),
        primary});
}

void DrawHeader() {
    vibeready::ui::Foundation& ui = g_state.ui;
    const vibeready::ui::UiTokens& tokens = ui.Tokens();
    D2D1_SIZE_F size = ui.Size();
    float margin = size.width < 720.0f ? 28.0f : 40.0f;
    float right = std::max(margin + 320.0f, size.width - margin);
    bool hasBack = ControlVisible(UiControl::BackButton);
    float brandTop = hasBack ? 76.0f : 28.0f;
    float brandBottom = hasBack ? 110.0f : 70.0f;
    float titleTop = hasBack ? 116.0f : 84.0f;
    float titleBottom = hasBack ? 144.0f : 112.0f;
    float bodyTop = hasBack ? 152.0f : 120.0f;
    float bodyBottom = hasBack ? 204.0f : 182.0f;

    DrawButtonIfVisible(UiControl::BackButton, BackLabel(), false);

    ui.DrawTextBlock(L"VibeReady", D2D1::RectF(margin, brandTop, right, brandBottom),
        vibeready::ui::TextRole::Display, tokens.colors.text);
    ui.DrawTextBlock(RouteTitle(), D2D1::RectF(margin, titleTop, right, titleBottom),
        vibeready::ui::TextRole::Title, tokens.colors.text);
    ui.DrawTextBlock(RouteBody(), D2D1::RectF(margin, bodyTop, right, bodyBottom),
        vibeready::ui::TextRole::Body, tokens.colors.textMuted);
}

void DrawReadinessRows(const D2D1_RECT_F& rect) {
    vibeready::ui::Foundation& ui = g_state.ui;
    float y = rect.top + 50.0f;
    float rowHeight = 32.0f;

    const struct {
        const wchar_t* label;
        bool ok;
    } rows[] = {
        {L"Windows 10/11", g_state.checks.supportedWindows},
        {L"x64", g_state.checks.x64},
        {L"VS Code", g_state.checks.vscode.state == ToolState::Ready},
        {L"Git", g_state.checks.git.state == ToolState::Ready},
        {L"Node.js", g_state.checks.node.state == ToolState::Ready},
        {L"npm", g_state.checks.npm.state == ToolState::Ready},
    };

    for (const auto& row : rows) {
        D2D1_RECT_F rowRect = D2D1::RectF(rect.left + 18.0f, y, rect.right - 18.0f, y + rowHeight);
        ui.DrawStatusRow(vibeready::ui::StatusRowSpec{rowRect, row.label, ValueFor(row.ok), ToneFor(row.ok)});
        y += rowHeight;
    }
}

const wchar_t* StepLabel(int index) {
    switch (index) {
    case 0:
        return L"Platform support";
    case 1:
        return L"Permissions and folders";
    case 2:
        return L"VS Code";
    case 3:
        return L"Git";
    case 4:
        return L"Node.js and npm";
    case 5:
    default:
        return L"Network and WinGet";
    }
}

std::wstring StepValue(int index) {
    Locale locale = ActiveLocale();
    if (index < g_state.scanStep) {
        if (locale == Locale::ZhCn) {
            return L"已完成";
        }
        if (locale == Locale::Ja) {
            return L"完了";
        }
        return L"Done";
    }
    if (index == g_state.scanStep) {
        if (locale == Locale::ZhCn) {
            return L"进行中";
        }
        if (locale == Locale::Ja) {
            return L"進行中";
        }
        return L"In progress";
    }
    if (locale == Locale::ZhCn) {
        return L"等待中";
    }
    if (locale == Locale::Ja) {
        return L"待機中";
    }
    return L"Pending";
}

void DrawSetupScreen() {
    vibeready::ui::Foundation& ui = g_state.ui;
    const Preferences& preferences = VisiblePreferences();
    const Copy& copy = CurrentCopy();
    const vibeready::ui::UiTokens& tokens = ui.Tokens();
    D2D1_SIZE_F size = ui.Size();
    float margin = size.width < 720.0f ? 28.0f : 40.0f;
    float panelRight = std::min(size.width - margin, margin + 520.0f);
    float panelTop = g_state.route == AppRoute::Settings ? 218.0f : 198.0f;
    D2D1_RECT_F panel = D2D1::RectF(margin, panelTop, panelRight, panelTop + 296.0f);

    ui.DrawSection(vibeready::ui::SectionSpec{panel, RouteTitle()});
    ui.DrawSelect(vibeready::ui::SelectSpec{
        g_state.layout.languageSelect.rect,
        copy.languageLabel,
        Text(preferences.locale).languageName,
        ControlStateFor(UiControl::LanguageSelect)});
    ui.DrawToggle(vibeready::ui::ToggleSpec{
        g_state.layout.telemetryToggle.rect,
        copy.telemetryLabel,
        preferences.telemetryAllowed ? copy.toggleOn : copy.toggleOff,
        preferences.telemetryAllowed,
        ControlStateFor(UiControl::TelemetryToggle)});
    ui.DrawToggle(vibeready::ui::ToggleSpec{
        g_state.layout.themeToggle.rect,
        copy.themeLabel,
        preferences.darkTheme ? copy.toggleOn : copy.toggleOff,
        preferences.darkTheme,
        ControlStateFor(UiControl::ThemeToggle)});
    DrawButtonIfVisible(UiControl::PrimaryButton, copy.savePreferencesAction, true);
    DrawButtonIfVisible(UiControl::SettingsButton, L10n(L"Cancel", L"取消", L"キャンセル"), false);

    if (!g_state.checks.configWritable && g_state.initialized) {
        D2D1_RECT_F note = D2D1::RectF(panel.left + 20.0f, panel.bottom - 42.0f, panel.right - 20.0f, panel.bottom - 16.0f);
        ui.DrawTextBlock(copy.configWarning, note, vibeready::ui::TextRole::Caption, tokens.colors.warning);
    }
}

void DrawStatusSurface(const D2D1_RECT_F& sectionRect) {
    vibeready::ui::Foundation& ui = g_state.ui;
    const Copy& copy = CurrentCopy();
    const vibeready::ui::UiTokens& tokens = ui.Tokens();
    std::wstring title = copy.diagnosticsTitle;
    if (g_state.showTechnicalDetails) {
        title = copy.technicalDetailsLabel;
    } else if (g_state.route == AppRoute::ScanResults) {
        title = copy.scanResultsTitle;
    }
    ui.DrawSection(vibeready::ui::SectionSpec{sectionRect, title});

    D2D1_RECT_F content = D2D1::RectF(sectionRect.left + 18.0f, sectionRect.top + 48.0f, sectionRect.right - 18.0f, sectionRect.bottom - 18.0f);
    if (g_state.showTechnicalDetails) {
        ui.DrawTextBlock(BuildTechnicalStatusText(), content, vibeready::ui::TextRole::Code, tokens.colors.text);
        return;
    }
    if (g_state.route == AppRoute::ScanResults) {
        ui.DrawTextBlock(BuildStatusText(), content, vibeready::ui::TextRole::Code, tokens.colors.text);
        return;
    }

    DrawReadinessRows(sectionRect);
}

void DrawHomeOrResults() {
    vibeready::ui::Foundation& ui = g_state.ui;
    const Copy& copy = CurrentCopy();
    D2D1_SIZE_F size = ui.Size();
    float margin = size.width < 720.0f ? 28.0f : 40.0f;
    float right = std::max(margin + 320.0f, size.width - margin);
    float statusTop = g_state.route == AppRoute::Home ? 322.0f : 212.0f;
    float statusBottom = std::max(statusTop + 248.0f, size.height - 112.0f);
    if (g_state.route == AppRoute::ScanResults) {
        statusBottom = std::max(statusTop + 250.0f, size.height - 104.0f);
    }

    std::wstring primaryText = copy.primaryAction;
    if (g_state.route == AppRoute::ScanResults) {
        primaryText = BuildRepairPlan().empty()
            ? L10n(L"Verify local project", L"验证本地项目", L"ローカルプロジェクトを検証")
            : L10n(L"Review fix plan", L"查看修复计划", L"修復プランを確認");
    }

    ui.DrawButton(vibeready::ui::ButtonSpec{
        g_state.layout.primaryButton.rect,
        primaryText,
        ControlStateFor(UiControl::PrimaryButton),
        true});
    ui.DrawButton(vibeready::ui::ButtonSpec{
        g_state.layout.settingsButton.rect,
        copy.settingsAction,
        ControlStateFor(UiControl::SettingsButton),
        false});
    ui.DrawButton(vibeready::ui::ButtonSpec{
        g_state.layout.technicalDetailsButton.rect,
        g_state.showTechnicalDetails ? copy.hideTechnicalDetailsAction : copy.technicalDetailsAction,
        ControlStateFor(UiControl::TechnicalDetailsButton),
        false});

    DrawStatusSurface(D2D1::RectF(margin, statusTop, right, statusBottom));
}

void DrawScanningScreen() {
    vibeready::ui::Foundation& ui = g_state.ui;
    const Copy& copy = CurrentCopy();
    D2D1_SIZE_F size = ui.Size();
    float margin = size.width < 720.0f ? 28.0f : 40.0f;
    float right = std::max(margin + 320.0f, size.width - margin);
    D2D1_RECT_F section = D2D1::RectF(margin, 210.0f, right, std::max(458.0f, size.height - 112.0f));

    ui.DrawSection(vibeready::ui::SectionSpec{section, copy.scanningTitle});
    float y = section.top + 52.0f;
    for (int i = 0; i < 6; ++i) {
        vibeready::ui::StatusTone tone = vibeready::ui::StatusTone::Neutral;
        if (i < g_state.scanStep) {
            tone = vibeready::ui::StatusTone::Success;
        } else if (i == g_state.scanStep) {
            tone = vibeready::ui::StatusTone::Accent;
        }
        D2D1_RECT_F row = D2D1::RectF(section.left + 18.0f, y, section.right - 18.0f, y + 31.0f);
        ui.DrawStatusRow(vibeready::ui::StatusRowSpec{row, StepLabel(i), StepValue(i), tone});
        y += 31.0f;
    }
    ui.DrawProgressBar(D2D1::RectF(section.left + 18.0f, section.bottom - 38.0f, section.right - 18.0f, section.bottom - 28.0f),
        static_cast<float>(g_state.scanStep) / 6.0f,
        vibeready::ui::ControlState::Loading);
    ui.DrawButton(vibeready::ui::ButtonSpec{
        g_state.layout.primaryButton.rect,
        copy.checkingAction,
        ControlStateFor(UiControl::PrimaryButton, true),
        true});
}

void DrawTextPanel(const std::wstring& title, const std::wstring& text) {
    vibeready::ui::Foundation& ui = g_state.ui;
    const vibeready::ui::UiTokens& tokens = ui.Tokens();
    D2D1_SIZE_F size = ui.Size();
    float margin = size.width < 720.0f ? 28.0f : 40.0f;
    float right = std::max(margin + 320.0f, size.width - margin);
    float bottomReserve = g_state.route == AppRoute::Ready ? 174.0f : 112.0f;
    D2D1_RECT_F section = D2D1::RectF(margin, 210.0f, right, std::max(458.0f, size.height - bottomReserve));
    ui.DrawSection(vibeready::ui::SectionSpec{section, title});
    ui.DrawTextBlock(text, D2D1::RectF(section.left + 18.0f, section.top + 48.0f, section.right - 18.0f, section.bottom - 18.0f),
        vibeready::ui::TextRole::Code, tokens.colors.text);
}

void DrawFixPlanScreen() {
    DrawTextPanel(L10n(L"Planned actions", L"计划操作", L"予定された操作"), BuildRepairPlanText());

    std::wstring primary = HasAutoRepairItems()
        ? L10n(L"Start fix", L"开始修复", L"修復を開始")
        : L10n(L"Manual steps", L"手动步骤", L"手動手順");
    if (g_state.repair.plan.empty()) {
        primary = CurrentCopy().rescanAction;
    }
    DrawButtonIfVisible(UiControl::PrimaryButton, primary, true);
    DrawButtonIfVisible(UiControl::TechnicalDetailsButton, L10n(L"Manual steps", L"手动步骤", L"手動手順"), false);
}

void DrawFixProgressScreen() {
    DrawTextPanel(L10n(L"Repair status", L"修复状态", L"修復ステータス"), BuildFixProgressText());

    std::wstring primary = g_state.repair.active
        ? L10n(L"Fixing...", L"正在修复...", L"修復中...")
        : CurrentCopy().rescanAction;
    if (g_state.repair.failed && !g_state.repair.active) {
        primary = L10n(L"Retry fix", L"重试修复", L"修復を再試行");
    }
    DrawButtonIfVisible(UiControl::PrimaryButton, primary, true, g_state.repair.active);
    DrawButtonIfVisible(UiControl::SettingsButton, L10n(L"Manual steps", L"手动步骤", L"手動手順"), false);
}

void DrawManualStepsScreen() {
    DrawTextPanel(RepairToolName(g_state.repair.manualTool), BuildManualStepsText(g_state.repair.manualTool));

    DrawButtonIfVisible(UiControl::PrimaryButton, L10n(L"I installed it, rescan", L"我已安装，重新扫描", L"インストール済み、再スキャン"), true);
    DrawButtonIfVisible(UiControl::SettingsButton, L10n(L"Open download page", L"打开下载页面", L"ダウンロードページを開く"), false);
    DrawButtonIfVisible(UiControl::TechnicalDetailsButton, L10n(L"Copy steps", L"复制步骤", L"手順をコピー"), false);
}

void DrawVerificationScreen() {
    vibeready::ui::Foundation& ui = g_state.ui;
    DrawTextPanel(RouteTitle(), BuildVerificationText());

    ui.DrawButton(vibeready::ui::ButtonSpec{
        g_state.layout.primaryButton.rect,
        L10n(L"Verifying...", L"正在验证...", L"検証中..."),
        ControlStateFor(UiControl::PrimaryButton, true),
        true});
}

void DrawVerificationFailedScreen() {
    DrawTextPanel(RouteTitle(), BuildVerificationText());

    std::wstring secondary = g_state.verification.errorCode == L"BROWSER_OPEN_FAILED"
        ? L10n(L"Copy local address", L"复制本地地址", L"ローカルアドレスをコピー")
        : L10n(L"Manual steps", L"手动步骤", L"手動手順");
    DrawButtonIfVisible(UiControl::PrimaryButton, L10n(L"Try again", L"重试", L"もう一度試す"), true);
    DrawButtonIfVisible(UiControl::SettingsButton, CurrentCopy().settingsAction, false);
    DrawButtonIfVisible(UiControl::TechnicalDetailsButton, secondary, false);
}

void DrawReadyScreen() {
    DrawTextPanel(L"Ready", BuildReadyText());

    DrawButtonIfVisible(UiControl::PrimaryButton, L10n(L"Open VS Code", L"打开 VS Code", L"VS Code を開く"), true);
    DrawButtonIfVisible(UiControl::SettingsButton, CurrentCopy().settingsAction, false);
    DrawButtonIfVisible(UiControl::TechnicalDetailsButton, L10n(L"Copy prompt", L"复制提示词", L"プロンプトをコピー"), false);
    DrawButtonIfVisible(UiControl::ExitButton, CurrentCopy().rescanAction, false);
}

void DrawMainWindow(HWND hwnd) {
    UpdateLayout(hwnd);
    DrawHeader();

    switch (g_state.route) {
    case AppRoute::LanguageTelemetry:
    case AppRoute::Settings:
        DrawSetupScreen();
        break;
    case AppRoute::Scanning:
        DrawScanningScreen();
        break;
    case AppRoute::FixPlan:
        DrawFixPlanScreen();
        break;
    case AppRoute::FixProgress:
        DrawFixProgressScreen();
        break;
    case AppRoute::ManualSteps:
        DrawManualStepsScreen();
        break;
    case AppRoute::Verifying:
        DrawVerificationScreen();
        break;
    case AppRoute::VerificationFailed:
        DrawVerificationFailedScreen();
        break;
    case AppRoute::Ready:
        DrawReadyScreen();
        break;
    case AppRoute::ScanResults:
    case AppRoute::Home:
        DrawHomeOrResults();
        break;
    case AppRoute::Startup:
    default:
        break;
    }
}

std::wstring BuildFriendlyStatusText() {
    Locale locale = ActiveLocale();
    std::wstring text;
    if (locale == Locale::ZhCn) {
        text += L"环境准备度\r\n";
        text += L"• 平台支持：" + std::wstring((g_state.checks.supportedWindows && g_state.checks.x64) ? L"可继续检查。\r\n" : L"当前系统不支持扫描。\r\n");
        text += L"• 关键组件：\r\n";
        text += std::wstring(L"  - VS Code：") + std::wstring(g_state.checks.vscode.state == ToolState::Ready ? L"已可用\r\n" : L"未检测到\r\n");
        text += std::wstring(L"  - Git：") + std::wstring(g_state.checks.git.state == ToolState::Ready ? L"已可用\r\n" : L"未检测到\r\n");
        text += std::wstring(L"  - Node.js：") + std::wstring(g_state.checks.node.state == ToolState::Ready ? L"已可用\r\n" : L"未检测到\r\n");
        text += std::wstring(L"  - npm：") + std::wstring(g_state.checks.npm.state == ToolState::Ready ? L"已可用\r\n" : L"未检测到\r\n");
        text += L"• 权限与写入：";
        text += std::wstring((g_state.checks.tempWritable && g_state.checks.configWritable) ? L"正常。\r\n" : L"需检查应用目录与临时目录写入权限。\r\n");
        text += L"\r\n建议操作：\r\n";
        if (!g_state.checks.supportedWindows || !g_state.checks.x64) {
            text += L"• 切换到 Windows 10/11 x64 后重试。\r\n";
        }
        if (g_state.checks.vscode.state != ToolState::Ready) {
            text += L"• 安装 VS Code。\r\n";
        }
        if (g_state.checks.git.state != ToolState::Ready) {
            text += L"• 安装 Git。\r\n";
        }
        if (g_state.checks.node.state != ToolState::Ready) {
            text += L"• 安装 Node.js（包含 npm）。\r\n";
        }
        return text;
    }
    if (locale == Locale::Ja) {
        text += L"準備状況\r\n";
        text += L"• 対応可否：";
        text += std::wstring((g_state.checks.supportedWindows && g_state.checks.x64) ? L"環境チェック可能です。\r\n" : L"対応外の環境です。\r\n");
        text += L"• 主要コンポーネント：\r\n";
        text += std::wstring(L"  - VS Code: ") + std::wstring(g_state.checks.vscode.state == ToolState::Ready ? L"利用可能\r\n" : L"未検出\r\n");
        text += std::wstring(L"  - Git: ") + std::wstring(g_state.checks.git.state == ToolState::Ready ? L"利用可能\r\n" : L"未検出\r\n");
        text += std::wstring(L"  - Node.js: ") + std::wstring(g_state.checks.node.state == ToolState::Ready ? L"利用可能\r\n" : L"未検出\r\n");
        text += std::wstring(L"  - npm: ") + std::wstring(g_state.checks.npm.state == ToolState::Ready ? L"利用可能\r\n" : L"未検出\r\n");
        text += L"• 権限/書き込み：";
        text += std::wstring((g_state.checks.tempWritable && g_state.checks.configWritable) ? L"正常です。\r\n" : L"権限設定を確認してください。\r\n");
        text += L"\r\n推奨手順：\r\n";
        if (!g_state.checks.supportedWindows || !g_state.checks.x64) {
            text += L"• Windows 10/11 x64 の端末で再実行してください。\r\n";
        }
        if (g_state.checks.vscode.state != ToolState::Ready) {
            text += L"• VS Code をインストールしてください。\r\n";
        }
        if (g_state.checks.git.state != ToolState::Ready) {
            text += L"• Git をインストールしてください。\r\n";
        }
        if (g_state.checks.node.state != ToolState::Ready) {
            text += L"• Node.js（npm 含む）をインストールしてください。\r\n";
        }
        return text;
    }
    text += L"Environment readiness\r\n";
    text += L"• Platform: ";
    text += std::wstring((g_state.checks.supportedWindows && g_state.checks.x64) ? L"ready for scan.\r\n" : L"unsupported.\r\n");
    text += L"• Tool checks: Git, Node.js, npm, VS Code, WinGet.\r\n";
    text += L"• Writable status: ";
    text += std::wstring((g_state.checks.tempWritable && g_state.checks.configWritable) ? L"normal.\r\n" : L"please allow write access.\r\n");
    text += L"\r\nRecommended actions:\r\n";
    if (!g_state.checks.supportedWindows || !g_state.checks.x64) {
        text += L"• Use Windows 10/11 x64.\r\n";
    }
    if (g_state.checks.vscode.state != ToolState::Ready) {
        text += L"• Install VS Code.\r\n";
    }
    if (g_state.checks.git.state != ToolState::Ready) {
        text += L"• Install Git.\r\n";
    }
    if (g_state.checks.node.state != ToolState::Ready) {
        text += L"• Install Node.js (LTS).\r\n";
    }
    return text;
}

std::wstring BuildTechnicalStatusText() {
    const Copy& copy = CurrentCopy();
    std::wstring text = std::wstring(copy.diagnosticsTitle) + L"\r\n";
    text += std::wstring(L"system: ") + (g_state.checks.systemResult.state == ToolState::Ready ? L"ready" : L"blocked");
    if (!g_state.checks.systemResult.errorCode.empty()) {
        text += L" (" + g_state.checks.systemResult.errorCode + L")";
    }
    text += L"\r\n";
    text += std::wstring(L"OS: ") + g_state.checks.systemResult.detectedVersion + L"\r\n";
    text += std::wstring(L"Windows 10/11: ") + (g_state.checks.supportedWindows ? copy.supported : copy.unsupported) + L"\r\n";
    text += std::wstring(L"x64: ") + (g_state.checks.x64 ? copy.supported : copy.unsupported) + L"\r\n";
    text += std::wstring(L"Permission: ") + (g_state.checks.administrator ? copy.administrator : copy.standardUser) + L"\r\n";
    text += std::wstring(L"UAC: ") + (g_state.checks.mayRequireUacForInstall ? copy.uacMayAppear : copy.uacNotExpected) + L"\r\n";
    text += std::wstring(L"VS Code: ") + (g_state.checks.vscode.state == ToolState::Ready ? copy.supported : copy.unsupported) + L"\r\n";
    text += std::wstring(L"Git: ") + (g_state.checks.git.state == ToolState::Ready ? copy.supported : copy.unsupported) + L"\r\n";
    text += std::wstring(L"Node.js: ") + (g_state.checks.node.state == ToolState::Ready ? copy.supported : copy.unsupported) + L"\r\n";
    text += std::wstring(L"npm: ") + (g_state.checks.npm.state == ToolState::Ready ? copy.supported : copy.unsupported) + L"\r\n";
    text += std::wstring(L"WinGet: ") + (g_state.checks.winget.state == ToolState::Ready ? copy.supported : copy.unsupported) + L"\r\n";
    text += std::wstring(L"Network: ") + (g_state.checks.network.state == ToolState::Ready ? copy.supported : copy.unsupported) + L"\r\n";
    text += std::wstring(L"Temp: ") + (g_state.checks.tempWritable ? copy.supported : copy.unsupported) + L"\r\n";
    text += std::wstring(L"Config: ") + (g_state.checks.configWritable ? copy.supported : copy.unsupported) + L"\r\n";
    text += std::wstring(L"Log: ") + (g_state.checks.logWritable ? copy.supported : copy.unsupported) + L"\r\n";
    text += std::wstring(L"\r\n") + (g_state.preferences.telemetryAllowed ? copy.telemetryOn : copy.telemetryOff);
    return text;
}

std::wstring BuildScanningStatusText() {
    const Copy& copy = CurrentCopy();
    const wchar_t* steps[6] = {
        L"Platform support",
        L"Permissions and folders",
        L"VS Code",
        L"Git",
        L"Node.js and npm",
        L"Network and WinGet",
    };
    std::wstring text = std::wstring(copy.scanningTitle) + L"\r\n";
    text += std::wstring(copy.scanningBody) + L"\r\n\r\n";
    for (int i = 0; i < 6; ++i) {
        text += std::wstring(L"• ") + steps[i] + L": ";
        if (i < g_state.scanStep) {
            text += L"done\r\n";
        } else if (i == g_state.scanStep) {
            text += L"in progress\r\n";
        } else {
            text += L"pending\r\n";
        }
    }
    int percent = g_state.scanStep * 100 / 6;
    text += L"\r\n" + std::to_wstring(percent) + L"%\r\n";
    return text;
}

void RefreshUi() {
    SetWindowTextW(g_state.window, kWindowTitle);
    if (g_state.window) {
        UpdateLayout(g_state.window);
        InvalidateRect(g_state.window, nullptr, FALSE);
    }
}

void PaintMainWindow(HWND hwnd) {
    PAINTSTRUCT paint = {};
    BeginPaint(hwnd, &paint);

    HRESULT beginResult = g_state.ui.BeginDraw(CurrentThemeMode(), paint.hdc);
    if (SUCCEEDED(beginResult)) {
        DrawMainWindow(hwnd);
        HRESULT endResult = g_state.ui.EndDraw();
        if (endResult == D2DERR_RECREATE_TARGET) {
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (FAILED(endResult)) {
            AppendLog(L"Direct2D end draw failed: " + std::to_wstring(static_cast<unsigned long>(endResult)));
        }
    } else {
        AppendLog(L"Direct2D begin draw failed: " + std::to_wstring(static_cast<unsigned long>(beginResult)));
    }
    EndPaint(hwnd, &paint);
}

std::wstring ReadRegistryEnvironmentValue(HKEY root, const wchar_t* subkey, const wchar_t* valueName) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return L"";
    }
    std::wstring value;
    QueryRegistryString(key, valueName, &value);
    RegCloseKey(key);
    return value;
}

void AppendPathPart(std::wstring* path, const std::wstring& part) {
    if (part.empty()) {
        return;
    }
    if (!path->empty() && path->back() != L';') {
        *path += L";";
    }
    *path += part;
}

void RefreshProcessEnvironmentPath() {
    std::wstring combined = EnvironmentPath(L"PATH");
    AppendPathPart(&combined, ReadRegistryEnvironmentValue(
        HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment",
        L"Path"));
    AppendPathPart(&combined, ReadRegistryEnvironmentValue(
        HKEY_CURRENT_USER,
        L"Environment",
        L"Path"));
    if (!combined.empty()) {
        SetEnvironmentVariableW(L"PATH", combined.c_str());
        AppendLog(L"process PATH refreshed from registry");
    }
}

bool AllRequiredToolsReady() {
    return g_state.checks.supportedWindows &&
        g_state.checks.x64 &&
        g_state.checks.tempWritable &&
        g_state.checks.configWritable &&
        g_state.checks.vscode.state == ToolState::Ready &&
        g_state.checks.git.state == ToolState::Ready &&
        g_state.checks.node.state == ToolState::Ready &&
        g_state.checks.npm.state == ToolState::Ready;
}

std::wstring FirstVerificationErrorCode() {
    if (!g_state.checks.supportedWindows) {
        return L"UNSUPPORTED_WINDOWS_VERSION";
    }
    if (!g_state.checks.x64) {
        return L"UNSUPPORTED_ARCHITECTURE";
    }
    if (!g_state.checks.tempWritable) {
        return L"TEMP_DIR_UNWRITABLE";
    }
    if (!g_state.checks.configWritable) {
        return L"CONFIG_DIR_UNWRITABLE";
    }
    if (g_state.checks.vscode.state != ToolState::Ready) {
        return g_state.checks.vscode.errorCode.empty() ? L"VERIFY_FAILED" : g_state.checks.vscode.errorCode;
    }
    if (g_state.checks.git.state != ToolState::Ready) {
        return g_state.checks.git.errorCode.empty() ? L"VERIFY_FAILED" : g_state.checks.git.errorCode;
    }
    if (g_state.checks.node.state != ToolState::Ready) {
        return g_state.checks.node.errorCode.empty() ? L"VERIFY_FAILED" : g_state.checks.node.errorCode;
    }
    if (g_state.checks.npm.state != ToolState::Ready) {
        return g_state.checks.npm.errorCode.empty() ? L"VERIFY_FAILED" : g_state.checks.npm.errorCode;
    }
    return L"VERIFY_FAILED";
}

std::wstring ResolveCommandPath(const wchar_t* command) {
    DWORD needed = SearchPathW(nullptr, command, nullptr, 0, nullptr, nullptr);
    if (needed == 0) {
        return L"";
    }
    std::wstring path(static_cast<size_t>(needed) + 1, L'\0');
    DWORD written = SearchPathW(nullptr, command, nullptr, static_cast<DWORD>(path.size()), path.data(), nullptr);
    if (written == 0 || static_cast<size_t>(written) >= path.size()) {
        return L"";
    }
    path.resize(written);
    return path;
}

void TerminateProcessTree(PROCESS_INFORMATION* process) {
    if (!process || !process->hProcess) {
        return;
    }
    DWORD exitCode = 0;
    if (GetExitCodeProcess(process->hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
        DWORD taskkillExit = 1;
        std::wstring commandLine = L"taskkill.exe /PID " + std::to_wstring(process->dwProcessId) + L" /T /F";
        RunProcessWithTimeout(commandLine, L"", 5000, &taskkillExit);
        if (GetExitCodeProcess(process->hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
            TerminateProcess(process->hProcess, 1);
        }
    }
    if (process->hThread) {
        CloseHandle(process->hThread);
    }
    CloseHandle(process->hProcess);
    *process = PROCESS_INFORMATION{};
}

void StopVerificationServer() {
    TerminateProcessTree(&g_state.verification.serverProcess);
    g_state.verification.serviceStarted = false;
}

bool WriteLocalTestProject(std::wstring* projectDir) {
    if (g_state.checks.configDir.empty()) {
        return false;
    }
    CreateDirectoryW(g_state.checks.configDir.c_str(), nullptr);
    std::wstring dir = JoinPath(g_state.checks.configDir, L"local-test-project");
    DWORD attributes = GetFileAttributesW(dir.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        DeleteDirectoryTree(dir);
    }
    if (!CreateDirectoryW(dir.c_str(), nullptr)) {
        return false;
    }

    std::wstring packageJson =
        L"{\r\n"
        L"  \"name\": \"vibeready-local-test\",\r\n"
        L"  \"version\": \"1.0.0\",\r\n"
        L"  \"private\": true,\r\n"
        L"  \"scripts\": {\r\n"
        L"    \"dev\": \"node server.js\"\r\n"
        L"  }\r\n"
        L"}\r\n";
    std::wstring serverJs =
        L"const http = require('http');\n"
        L"const fs = require('fs');\n"
        L"const path = require('path');\n"
        L"const port = Number(process.env.VIBEREADY_PORT || '5173');\n"
        L"const page = fs.readFileSync(path.join(__dirname, 'index.html'));\n"
        L"const server = http.createServer((req, res) => {\n"
        L"  res.writeHead(200, {'content-type': 'text/html; charset=utf-8'});\n"
        L"  res.end(page);\n"
        L"});\n"
        L"server.listen(port, '127.0.0.1', () => {\n"
        L"  console.log(`VibeReady test server ready http://127.0.0.1:${port}`);\n"
        L"});\n";
    std::wstring indexHtml =
        L"<!doctype html>\n"
        L"<html lang=\"en\">\n"
        L"<head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
        L"<title>VibeReady local test</title>\n"
        L"<style>body{font-family:Segoe UI,Arial,sans-serif;margin:48px;background:#f6f7f9;color:#171a1f}main{max-width:720px}button{padding:10px 16px;border:0;background:#176b70;color:white;border-radius:6px}</style>\n"
        L"</head><body><main><h1>VibeReady local test is running</h1><p>Your local Node.js and npm workflow is ready for web vibe coding.</p><button>Ready</button></main></body></html>\n";

    if (!WriteTextFile(JoinPath(dir, L"package.json"), packageJson, false) ||
        !WriteTextFile(JoinPath(dir, L"server.js"), serverJs, false) ||
        !WriteTextFile(JoinPath(dir, L"index.html"), indexHtml, false)) {
        DeleteDirectoryTree(dir);
        return false;
    }

    *projectDir = dir;
    return true;
}

bool ProbeLocalHttp(DWORD port) {
    HINTERNET session = WinHttpOpen(L"VibeReady/0.1", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        return false;
    }
    WinHttpSetTimeouts(session, 500, 500, 500, 500);
    HINTERNET connection = WinHttpConnect(session, L"127.0.0.1", static_cast<INTERNET_PORT>(port), 0);
    if (!connection) {
        WinHttpCloseHandle(session);
        return false;
    }
    HINTERNET request = WinHttpOpenRequest(connection, L"GET", L"/", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!request) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }
    BOOL ok = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr);
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    bool ready = ok &&
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX) &&
        statusCode == 200;
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return ready;
}

bool StartNpmDevServer(DWORD port, PROCESS_INFORMATION* process) {
    std::wstring commandLine = L"cmd.exe /d /s /c \"set VIBEREADY_PORT=" + std::to_wstring(port) + L"&& npm.cmd run dev\"";
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    BOOL created = CreateProcessW(
        nullptr,
        commandLine.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        g_state.verification.projectDir.c_str(),
        &startup,
        process);
    return created != FALSE;
}

bool StartAndVerifyLocalServer() {
    DWORD seed = GetTickCount();
    for (DWORD attempt = 0; attempt < 8; ++attempt) {
        DWORD port = 51230 + ((seed + attempt) % 700);
        PROCESS_INFORMATION process = {};
        if (!StartNpmDevServer(port, &process)) {
            continue;
        }
        g_state.verification.serviceStarted = true;
        g_state.verification.port = port;
        g_state.verification.localUrl = L"http://127.0.0.1:" + std::to_wstring(port);
        for (int probe = 0; probe < 28; ++probe) {
            if (ProbeLocalHttp(port)) {
                g_state.verification.serverProcess = process;
                g_state.verification.serviceResponded = true;
                return true;
            }
            DWORD exitCode = 0;
            if (GetExitCodeProcess(process.hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
                break;
            }
            Sleep(250);
        }
        TerminateProcessTree(&process);
    }
    return false;
}

bool OpenDefaultBrowserToLocalPage() {
    if (g_state.verification.localUrl.empty()) {
        return false;
    }
    HINSTANCE result = ShellExecuteW(g_state.window, L"open", g_state.verification.localUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

std::wstring FindVscodeExecutablePath() {
    if (!g_state.checks.vscode.executablePath.empty() && FileExists(g_state.checks.vscode.executablePath)) {
        return g_state.checks.vscode.executablePath;
    }
    VscodeScanResult latest = RunVscodeScan();
    g_state.checks.vscode = latest;
    if (!latest.executablePath.empty() && FileExists(latest.executablePath)) {
        return latest.executablePath;
    }
    std::wstring codeCmd = ResolveCommandPath(L"code.cmd");
    if (!codeCmd.empty()) {
        return codeCmd;
    }
    return ResolveCommandPath(L"code.exe");
}

bool OpenProjectFolderFallback() {
    HINSTANCE result = ShellExecuteW(g_state.window, L"open", g_state.verification.projectDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    g_state.verification.folderFallbackOpened = reinterpret_cast<INT_PTR>(result) > 32;
    return g_state.verification.folderFallbackOpened;
}

bool OpenVscodeProject(bool allowFolderFallback) {
    if (g_state.verification.projectDir.empty()) {
        return false;
    }
    std::wstring vscode = FindVscodeExecutablePath();
    if (!vscode.empty()) {
        std::wstring parameter = QuoteArgument(g_state.verification.projectDir);
        HINSTANCE result = ShellExecuteW(g_state.window, L"open", vscode.c_str(), parameter.c_str(), nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) > 32) {
            g_state.verification.vscodeOpened = true;
            return true;
        }
    }
    if (allowFolderFallback) {
        OpenProjectFolderFallback();
    }
    return false;
}

void FailVerification(const std::wstring& errorCode, const std::wstring& message) {
    if (g_state.window) {
        KillTimer(g_state.window, kVerificationTimerId);
    }
    g_state.verification.active = false;
    g_state.verification.completed = true;
    g_state.verification.succeeded = false;
    g_state.verification.failed = true;
    g_state.verification.errorCode = errorCode.empty() ? L"VERIFY_FAILED" : errorCode;
    g_state.verification.lastMessage = message;
    g_state.route = AppRoute::VerificationFailed;
    AppendLog(L"verification_completed overall_state=error error_code=" + g_state.verification.errorCode);
    RefreshUi();
}

void CompleteVerification() {
    if (g_state.window) {
        KillTimer(g_state.window, kVerificationTimerId);
    }
    g_state.verification.active = false;
    g_state.verification.completed = true;
    g_state.verification.succeeded = true;
    g_state.verification.failed = false;
    g_state.verification.errorCode.clear();
    g_state.verification.lastMessage = L10n(
        L"Local verification passed. The Ready page uses only local templates.",
        L"本地验证已通过。Ready 页只使用本地提示词模板。",
        L"ローカル検証に合格しました。Ready ページはローカルテンプレートのみを使用します。");
    g_state.route = AppRoute::Ready;
    AppendLog(L"verification_completed overall_state=ready");
    AppendLog(L"ready_reached verified_tool_count=4");
    RefreshUi();
}

void AdvanceVerificationFlow() {
    if (!g_state.verification.active) {
        if (g_state.window) {
            KillTimer(g_state.window, kVerificationTimerId);
        }
        return;
    }

    switch (g_state.verification.step) {
    case 0:
        g_state.verification.lastMessage = L10n(
            L"Refreshing installed tool visibility.",
            L"正在刷新已安装工具的可见性。",
            L"インストール済みツールの認識を更新しています。");
        RefreshUi();
        UpdateWindow(g_state.window);
        RefreshProcessEnvironmentPath();
        g_state.checks = RunStartupChecks();
        if (!AllRequiredToolsReady()) {
            FailVerification(FirstVerificationErrorCode(), L10n(
                L"VibeReady still cannot start one required tool. Retry after reopening VibeReady if an installer just finished.",
                L"VibeReady 仍无法启动某个必需工具。如果安装刚完成，请重新打开 VibeReady 后重试。",
                L"必須ツールの一部をまだ起動できません。インストール直後の場合は VibeReady を再起動してから再試行してください。"));
            return;
        }
        g_state.verification.step = 1;
        break;
    case 1:
        g_state.verification.lastMessage = L10n(
            L"Creating a local test project in VibeReady app data.",
            L"正在 VibeReady 应用数据目录创建本地测试项目。",
            L"VibeReady のアプリデータにローカルテストプロジェクトを作成しています。");
        RefreshUi();
        UpdateWindow(g_state.window);
        if (!WriteLocalTestProject(&g_state.verification.projectDir)) {
            FailVerification(L"VERIFY_FAILED", L10n(
                L"VibeReady could not create the local test project.",
                L"VibeReady 无法创建本地测试项目。",
                L"ローカルテストプロジェクトを作成できませんでした。"));
            return;
        }
        g_state.verification.projectCreated = true;
        g_state.verification.step = 2;
        break;
    case 2:
        g_state.verification.lastMessage = L10n(
            L"Starting npm run dev and waiting for the local page.",
            L"正在启动 npm run dev，并等待本地页面响应。",
            L"npm run dev を起動し、ローカルページの応答を待っています。");
        RefreshUi();
        UpdateWindow(g_state.window);
        if (!StartAndVerifyLocalServer()) {
            FailVerification(L"LOCAL_SERVER_FAILED", L10n(
                L"The local Node test server did not respond in time.",
                L"本地 Node 测试服务未及时响应。",
                L"ローカル Node テストサーバーが時間内に応答しませんでした。"));
            return;
        }
        g_state.verification.step = 3;
        break;
    case 3:
        g_state.verification.lastMessage = L10n(
            L"Opening the local page with the default browser.",
            L"正在使用默认浏览器打开本地页面。",
            L"既定のブラウザーでローカルページを開いています。");
        RefreshUi();
        UpdateWindow(g_state.window);
        if (!OpenDefaultBrowserToLocalPage()) {
            FailVerification(L"BROWSER_OPEN_FAILED", L10n(
                L"The local server is running. Copy the local address and open it in any browser.",
                L"本地服务已经运行。请复制本地地址，并用任意浏览器打开。",
                L"ローカルサーバーは実行中です。ローカルアドレスをコピーして任意のブラウザーで開いてください。"));
            return;
        }
        g_state.verification.browserOpened = true;
        g_state.verification.step = 4;
        break;
    case 4:
        g_state.verification.lastMessage = L10n(
            L"Opening the test project in VS Code.",
            L"正在用 VS Code 打开测试项目。",
            L"VS Code でテストプロジェクトを開いています。");
        RefreshUi();
        UpdateWindow(g_state.window);
        if (!OpenVscodeProject(true)) {
            FailVerification(L"VSCODE_OPEN_FAILED", L10n(
                L"VS Code did not open the test project automatically. The project folder fallback was offered.",
                L"VS Code 未能自动打开测试项目，已尝试提供项目文件夹作为备用入口。",
                L"VS Code でテストプロジェクトを自動的に開けませんでした。代わりにプロジェクトフォルダーを開こうとしました。"));
            return;
        }
        g_state.verification.step = 5;
        break;
    default:
        CompleteVerification();
        return;
    }

    RefreshUi();
}

void StartVerificationFlow() {
    StopVerificationServer();
    g_state.verification = VerificationState{};
    g_state.verification.active = true;
    g_state.verification.step = 0;
    g_state.route = AppRoute::Verifying;
    g_state.showTechnicalDetails = false;
    AppendLog(L"verification_started");
    SetTimer(g_state.window, kVerificationTimerId, 250, nullptr);
    RefreshUi();
}

void FailRepairItem(RepairPlanItem* item, const std::wstring& errorCode, DWORD exitCode = 0) {
    item->stepState = RepairStepState::Failed;
    item->resultErrorCode = errorCode;
    item->exitCode = exitCode;
    g_state.repair.failed = true;
    g_state.repair.active = false;
    g_state.repair.completed = true;
    g_state.repair.manualTool = item->tool;
    g_state.repair.lastMessage = L10n(
        L"Automatic repair failed. Use manual steps or retry after the environment changes.",
        L"自动修复失败。请使用手动步骤，或在环境变化后重试。",
        L"自動修復に失敗しました。手動手順を使うか、環境変更後に再試行してください。");
    AppendLog(L"repair failed for " + item->name + L" error=" + errorCode + L" exit=" + std::to_wstring(exitCode));
}

void RunRepairStep(RepairPlanItem* item) {
    if (item->manualOnly) {
        item->stepState = RepairStepState::Manual;
        return;
    }

    item->stepState = RepairStepState::Running;
    g_state.repair.lastMessage = L10n(L"Installing ", L"正在安装 ", L"インストール中 ") + item->name + L"...";
    RefreshUi();
    UpdateWindow(g_state.window);

    if (g_state.checks.winget.state != ToolState::Ready) {
        FailRepairItem(item, L"WINGET_UNAVAILABLE");
        return;
    }
    if (g_state.checks.network.state != ToolState::Ready) {
        FailRepairItem(item, L"NETWORK_UNAVAILABLE");
        return;
    }

    AppendLog(L"repair started for " + item->name + L" package=" + item->packageId);
    DWORD exitCode = 1;
    ProcessRunStatus status = RunWingetInstall(item->packageId, &exitCode);
    if (status == ProcessRunStatus::TimedOut) {
        FailRepairItem(item, L"TOOL_TIMEOUT", exitCode);
        return;
    }
    if (status != ProcessRunStatus::Started || exitCode != 0) {
        FailRepairItem(item, InstallErrorFromExitCode(exitCode), exitCode);
        return;
    }

    std::wstring verifyError;
    if (!RefreshScanForRepairTool(item->tool, &verifyError)) {
        FailRepairItem(item, verifyError.empty() ? L"VERIFY_FAILED" : verifyError, exitCode);
        return;
    }

    item->stepState = RepairStepState::Succeeded;
    item->resultErrorCode.clear();
    item->exitCode = exitCode;
    g_state.repair.lastMessage = item->name + L10n(L" is installed and verified.", L" 已安装并通过验证。", L" はインストールされ、確認済みです。");
    AppendLog(L"repair succeeded for " + item->name);
}

void FinishRepairFlowIfNeeded() {
    if (g_state.repair.failed || g_state.repair.currentIndex < g_state.repair.plan.size()) {
        return;
    }
    g_state.repair.active = false;
    g_state.repair.completed = true;
    g_state.repair.lastMessage = L10n(
        L"Automatic repair finished. Rescan to verify the full environment.",
        L"自动修复已完成。请重新扫描以验证完整环境。",
        L"自動修復が完了しました。環境全体を確認するため再スキャンしてください。");
    KillTimer(g_state.window, kRepairTimerId);
    AppendLog(L"repair flow completed");
}

void AdvanceRepairFlow() {
    if (!g_state.repair.active) {
        KillTimer(g_state.window, kRepairTimerId);
        return;
    }
    if (g_state.repair.currentIndex >= g_state.repair.plan.size()) {
        FinishRepairFlowIfNeeded();
        RefreshUi();
        return;
    }

    RepairPlanItem& item = g_state.repair.plan[g_state.repair.currentIndex];
    RunRepairStep(&item);
    ++g_state.repair.currentIndex;
    FinishRepairFlowIfNeeded();
    RefreshUi();
}

void StartRepairFlow() {
    if (g_state.repair.plan.empty()) {
        EnterScanResults();
        RefreshUi();
        return;
    }
    if (!HasAutoRepairItems()) {
        EnterManualSteps(FirstManualRepairTool());
        RefreshUi();
        return;
    }

    for (RepairPlanItem& item : g_state.repair.plan) {
        item.stepState = item.manualOnly ? RepairStepState::Manual : RepairStepState::Pending;
        item.resultErrorCode.clear();
        item.exitCode = 0;
    }
    g_state.repair.currentIndex = 0;
    g_state.repair.active = true;
    g_state.repair.completed = false;
    g_state.repair.failed = false;
    g_state.repair.lastMessage = L10n(
        L"Starting approved WinGet repair.",
        L"正在启动已批准的 WinGet 修复。",
        L"承認済みの WinGet 修復を開始しています。");
    g_state.route = AppRoute::FixProgress;
    SetTimer(g_state.window, kRepairTimerId, 250, nullptr);
    RefreshUi();
}

void OpenManualDownloadPage() {
    std::wstring url = RepairOfficialUrl(g_state.repair.manualTool);
    HINSTANCE result = ShellExecuteW(g_state.window, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        AppendLog(L"manual download page open failed");
        MessageBoxW(g_state.window, url.c_str(), kWindowTitle, MB_ICONINFORMATION | MB_OK);
    } else {
        AppendLog(L"manual download page opened for " + RepairToolName(g_state.repair.manualTool));
    }
}

bool CopyTextToClipboard(const std::wstring& text) {
    if (!OpenClipboard(g_state.window)) {
        return false;
    }
    EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory) {
        CloseClipboard();
        return false;
    }
    void* target = GlobalLock(memory);
    if (!target) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    memcpy(target, text.c_str(), bytes);
    GlobalUnlock(memory);
    if (!SetClipboardData(CF_UNICODETEXT, memory)) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    CloseClipboard();
    return true;
}

void CopyManualSteps() {
    std::wstring steps = BuildManualStepsText(g_state.repair.manualTool);
    if (CopyTextToClipboard(steps)) {
        AppendLog(L"manual steps copied for " + RepairToolName(g_state.repair.manualTool));
        MessageBoxW(g_state.window,
            L10n(L"Manual steps copied.", L"手动步骤已复制。", L"手動手順をコピーしました。").c_str(),
            kWindowTitle, MB_ICONINFORMATION | MB_OK);
    } else {
        MessageBoxW(g_state.window,
            L10n(L"Could not copy the steps.", L"无法复制步骤。", L"手順をコピーできませんでした。").c_str(),
            kWindowTitle, MB_ICONERROR | MB_OK);
    }
}

void CopyLocalAddress() {
    if (!g_state.verification.localUrl.empty() && CopyTextToClipboard(g_state.verification.localUrl)) {
        AppendLog(L"local test address copied");
        MessageBoxW(g_state.window,
            L10n(L"Local address copied.", L"本地地址已复制。", L"ローカルアドレスをコピーしました。").c_str(),
            kWindowTitle, MB_ICONINFORMATION | MB_OK);
        return;
    }
    MessageBoxW(g_state.window,
        L10n(L"Could not copy the local address.", L"无法复制本地地址。", L"ローカルアドレスをコピーできませんでした。").c_str(),
        kWindowTitle, MB_ICONERROR | MB_OK);
}

void CopyReadyPrompt() {
    std::wstring prompt = BuildNextPromptTemplate();
    if (CopyTextToClipboard(prompt)) {
        AppendLog(L"ready prompt copied");
        MessageBoxW(g_state.window,
            L10n(L"Prompt copied.", L"提示词已复制。", L"プロンプトをコピーしました。").c_str(),
            kWindowTitle, MB_ICONINFORMATION | MB_OK);
        return;
    }
    MessageBoxW(g_state.window,
        L10n(L"Could not copy the prompt.", L"无法复制提示词。", L"プロンプトをコピーできませんでした。").c_str(),
        kWindowTitle, MB_ICONERROR | MB_OK);
}

void ActivateControl(UiControl control) {
    if (!ControlEnabled(control)) {
        return;
    }

    switch (control) {
    case UiControl::BackButton:
        if (NavigateBack()) {
            RefreshUi();
        }
        break;
    case UiControl::LanguageSelect:
        EditablePreferences().locale = NextLocale(EditablePreferences().locale, 1);
        EditablePreferences().saved = false;
        AppendLog(L"language preference changed");
        RefreshUi();
        break;
    case UiControl::TelemetryToggle:
        EditablePreferences().telemetryAllowed = !EditablePreferences().telemetryAllowed;
        EditablePreferences().saved = false;
        AppendLog(EditablePreferences().telemetryAllowed ? L"telemetry consent allowed" : L"telemetry consent declined");
        RefreshUi();
        break;
    case UiControl::ThemeToggle:
        EditablePreferences().darkTheme = !EditablePreferences().darkTheme;
        EditablePreferences().saved = false;
        AppendLog(EditablePreferences().darkTheme ? L"dark theme enabled" : L"dark theme disabled");
        RefreshUi();
        break;
    case UiControl::SettingsButton:
        if (g_state.route == AppRoute::Settings) {
            NavigateBack();
        } else if (g_state.route == AppRoute::FixProgress) {
            EnterManualSteps(FirstManualRepairTool());
        } else if (g_state.route == AppRoute::ManualSteps) {
            OpenManualDownloadPage();
        } else if (g_state.route == AppRoute::VerificationFailed) {
            EnterSettings(g_state.route);
        } else if (g_state.route == AppRoute::Ready) {
            EnterSettings(g_state.route);
        } else if (g_state.route == AppRoute::Home || g_state.route == AppRoute::ScanResults) {
            EnterSettings(g_state.route);
        } else {
            EnterSettings(AppRoute::Home);
        }
        RefreshUi();
        break;
    case UiControl::TechnicalDetailsButton:
        if (g_state.route == AppRoute::FixPlan) {
            EnterManualSteps(FirstManualRepairTool());
        } else if (g_state.route == AppRoute::ManualSteps) {
            CopyManualSteps();
        } else if (g_state.route == AppRoute::VerificationFailed) {
            if (g_state.verification.errorCode == L"BROWSER_OPEN_FAILED") {
                CopyLocalAddress();
            } else {
                EnterManualSteps(FirstManualRepairTool());
            }
        } else if (g_state.route == AppRoute::Ready) {
            CopyReadyPrompt();
        } else {
            g_state.showTechnicalDetails = !g_state.showTechnicalDetails;
        }
        RefreshUi();
        break;
    case UiControl::PrimaryButton:
        if (g_state.route == AppRoute::LanguageTelemetry) {
            if (SavePreferences()) {
                EnterHome();
                AppendLog(L"preferences saved and entered home");
                RefreshUi();
            } else {
                MessageBoxW(g_state.window, CurrentCopy().unsupportedLanguageTelemetry, kWindowTitle, MB_ICONERROR | MB_OK);
            }
        } else if (g_state.route == AppRoute::Settings) {
            AppRoute returnRoute = g_state.settingsReturnRoute;
            if (SavePreferences()) {
                g_state.route = returnRoute;
                g_state.settingsDraft = g_state.preferences;
                AppendLog(L"settings saved and returned to source route");
                RefreshUi();
            } else {
                MessageBoxW(g_state.window, CurrentCopy().unsupportedLanguageTelemetry, kWindowTitle, MB_ICONERROR | MB_OK);
            }
        } else if (g_state.route == AppRoute::Home) {
            EnterScanning();
            SetTimer(g_state.window, kScanTimerId, 500, nullptr);
            AppendLog(L"primary environment check started");
            RefreshUi();
        } else if (g_state.route == AppRoute::ScanResults) {
            std::vector<RepairPlanItem> plan = BuildRepairPlan();
            if (!plan.empty()) {
                EnterFixPlan();
                AppendLog(L"repair plan reviewed");
                RefreshUi();
                break;
            }
            StartVerificationFlow();
            AppendLog(L"local verification requested from scan results");
        } else if (g_state.route == AppRoute::FixPlan) {
            StartRepairFlow();
            AppendLog(L"repair flow requested");
        } else if (g_state.route == AppRoute::FixProgress) {
            if (g_state.repair.failed) {
                StartRepairFlow();
                AppendLog(L"repair flow retry requested");
            } else if (g_state.repair.completed) {
                StartVerificationFlow();
                AppendLog(L"post-repair verification started");
            }
        } else if (g_state.route == AppRoute::ManualSteps) {
            StartVerificationFlow();
            AppendLog(L"manual install verification started");
        } else if (g_state.route == AppRoute::VerificationFailed) {
            StartVerificationFlow();
            AppendLog(L"verification retry requested");
        } else if (g_state.route == AppRoute::Ready) {
            if (!OpenVscodeProject(true)) {
                MessageBoxW(g_state.window,
                    L10n(L"VS Code could not open automatically. The test project folder was opened instead.",
                        L"无法自动打开 VS Code。已尝试改为打开测试项目文件夹。",
                        L"VS Code を自動で開けませんでした。代わりにテストプロジェクトフォルダーを開こうとしました。").c_str(),
                    kWindowTitle, MB_ICONWARNING | MB_OK);
            }
        }
        break;
    case UiControl::ExitButton:
        if (g_state.route == AppRoute::Ready) {
            EnterScanning();
            SetTimer(g_state.window, kScanTimerId, 500, nullptr);
            AppendLog(L"ready page rescan started");
            RefreshUi();
        } else {
            PostMessageW(g_state.window, WM_CLOSE, 0, 0);
        }
        break;
    case UiControl::None:
    default:
        break;
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        g_state.window = hwnd;
        g_state.dpi = static_cast<float>(GetDpiForWindow(hwnd));
        if (FAILED(g_state.ui.Initialize(hwnd))) {
            MessageBoxW(hwnd, L"VibeReady could not initialize Direct2D.", kWindowTitle, MB_ICONERROR | MB_OK);
            return -1;
        }
        g_state.ui.SetDpi(g_state.dpi);
        UpdateLayout(hwnd);
        SetTimer(hwnd, kStartupTimerId, 50, nullptr);
        return 0;

    case WM_TIMER:
        if (wparam == kStartupTimerId) {
            KillTimer(hwnd, kStartupTimerId);
            g_state.checks = RunStartupChecks();
            LoadPreferences();
            AppendLog(L"startup checks completed");
            if (!ArePreferencesComplete()) {
                EnterLanguageTelemetry();
            } else {
                EnterHome();
            }
            g_state.initialized = true;
            RefreshUi();
            if (!g_state.checks.supportedWindows || !g_state.checks.x64) {
                const Copy& copy = CurrentCopy();
                MessageBoxW(hwnd, copy.unsupportedSystemBody, copy.unsupportedSystemTitle, MB_ICONERROR | MB_OK);
            } else if (g_state.route == AppRoute::Home) {
                AppendLog(L"entered home route");
            }
        }
        if (wparam == kScanTimerId) {
            if (g_state.route != AppRoute::Scanning) {
                return 0;
            }
            if (g_state.scanStep >= 5) {
                KillTimer(hwnd, kScanTimerId);
                g_state.scanningActive = false;
                g_state.scanStep = 6;
                AppendLog(L"read-only environment scan started");
                g_state.checks = RunStartupChecks();
                AppendLog(L"read-only environment scan completed");
                EnterScanResults();
                RefreshUi();
                AppendLog(L"scan flow finished");
            } else {
                ++g_state.scanStep;
                RefreshUi();
            }
        }
        if (wparam == kRepairTimerId) {
            AdvanceRepairFlow();
        }
        if (wparam == kVerificationTimerId) {
            AdvanceVerificationFlow();
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) {
            g_state.ui.Resize(LOWORD(lparam), HIWORD(lparam));
            RefreshUi();
        }
        return 0;

    case WM_DPICHANGED: {
        g_state.dpi = static_cast<float>(HIWORD(wparam));
        g_state.ui.SetDpi(g_state.dpi);
        RECT* suggested = reinterpret_cast<RECT*>(lparam);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
            suggested->right - suggested->left, suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        RefreshUi();
        return 0;
    }

    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        RefreshUi();
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lparam) == HTCLIENT && g_state.hoveredControl != UiControl::None) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        break;

    case WM_MOUSEMOVE: {
        if (!g_state.trackingMouse) {
            TRACKMOUSEEVENT event = {};
            event.cbSize = sizeof(event);
            event.dwFlags = TME_LEAVE;
            event.hwndTrack = hwnd;
            TrackMouseEvent(&event);
            g_state.trackingMouse = true;
        }
        UiControl hovered = HitTest(PointFromLparam(lparam));
        if (hovered != g_state.hoveredControl) {
            g_state.hoveredControl = hovered;
            RefreshUi();
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        g_state.trackingMouse = false;
        if (g_state.hoveredControl != UiControl::None) {
            g_state.hoveredControl = UiControl::None;
            RefreshUi();
        }
        return 0;

    case WM_LBUTTONDOWN: {
        SetFocus(hwnd);
        UiControl pressed = HitTest(PointFromLparam(lparam));
        if (pressed != UiControl::None) {
            g_state.pressedControl = pressed;
            g_state.focusedControl = pressed;
            SetCapture(hwnd);
            RefreshUi();
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        UiControl pressed = g_state.pressedControl;
        UiControl released = HitTest(PointFromLparam(lparam));
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        g_state.pressedControl = UiControl::None;
        RefreshUi();
        if (pressed != UiControl::None && pressed == released) {
            ActivateControl(pressed);
        }
        return 0;
    }

    case WM_CANCELMODE:
        g_state.pressedControl = UiControl::None;
        RefreshUi();
        return 0;

    case WM_SETFOCUS:
        EnsureFocusedControl();
        RefreshUi();
        return 0;

    case WM_KILLFOCUS:
        RefreshUi();
        return 0;

    case WM_GETDLGCODE:
        return DLGC_WANTARROWS | DLGC_WANTCHARS | DLGC_WANTTAB;

    case WM_KEYDOWN:
        if (wparam == VK_TAB) {
            MoveFocus((GetKeyState(VK_SHIFT) & 0x8000) ? -1 : 1);
            RefreshUi();
            return 0;
        }
        if (wparam == VK_RETURN || wparam == VK_SPACE) {
            ActivateControl(g_state.focusedControl);
            return 0;
        }
        if (wparam == VK_ESCAPE || ((GetKeyState(VK_MENU) & 0x8000) && wparam == VK_LEFT)) {
            if (NavigateBack()) {
                RefreshUi();
                return 0;
            }
        }
        if (g_state.focusedControl == UiControl::LanguageSelect &&
            (wparam == VK_LEFT || wparam == VK_UP || wparam == VK_RIGHT || wparam == VK_DOWN)) {
            int direction = (wparam == VK_LEFT || wparam == VK_UP) ? -1 : 1;
            EditablePreferences().locale = NextLocale(EditablePreferences().locale, direction);
            EditablePreferences().saved = false;
            RefreshUi();
            return 0;
        }
        break;

    case WM_PAINT:
        PaintMainWindow(hwnd);
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lparam);
        info->ptMinTrackSize.x = MulDiv(720, static_cast<int>(g_state.dpi), 96);
        info->ptMinTrackSize.y = MulDiv(560, static_cast<int>(g_state.dpi), 96);
        return 0;
    }

    case WM_DESTROY:
        StopVerificationServer();
        AppendLog(L"shutdown");
        g_state.ui.DiscardDeviceResources();
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool RegisterMainWindowClass(HINSTANCE instance) {
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClassName;

    return RegisterClassExW(&windowClass) != 0;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HANDLE mutex = CreateMutexW(nullptr, TRUE, kSingleInstanceMutex);
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        FocusExistingInstance();
        CloseHandle(mutex);
        return 0;
    }

    g_state.instance = instance;

    if (!RegisterMainWindowClass(instance)) {
        MessageBoxW(nullptr, L"VibeReady could not start.", kWindowTitle, MB_ICONERROR | MB_OK);
        if (mutex) {
            CloseHandle(mutex);
        }
        return 1;
    }

    HWND window = CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        MulDiv(900, static_cast<int>(GetDpiForSystem()), 96),
        MulDiv(680, static_cast<int>(GetDpiForSystem()), 96),
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!window) {
        MessageBoxW(nullptr, L"VibeReady could not create its main window.", kWindowTitle, MB_ICONERROR | MB_OK);
        if (mutex) {
            CloseHandle(mutex);
        }
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (mutex) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }

    return static_cast<int>(message.wParam);
}
