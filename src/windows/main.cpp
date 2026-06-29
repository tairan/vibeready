#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>

#include "ui_foundation.h"

#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kWindowClassName[] = L"VibeReadyMainWindow";
constexpr wchar_t kWindowTitle[] = L"VibeReady";
constexpr wchar_t kSingleInstanceMutex[] = L"Local\\VibeReadySingleInstance";
constexpr UINT_PTR kStartupTimerId = 1;
constexpr UINT_PTR kScanTimerId = 2;

enum class AppRoute {
    Startup,
    LanguageTelemetry,
    Home,
    Scanning,
    ScanResults,
};

enum class UiControl {
    None,
    LanguageSelect,
    TelemetryToggle,
    ThemeToggle,
    PrimaryButton,
    SettingsButton,
    TechnicalDetailsButton,
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

struct ResultPageItem {
    std::wstring name;
    ToolState state = ToolState::Error;
    std::wstring errorCode;
    std::wstring reason;
    std::wstring nextStep;
    std::wstring detectedVersion;
    ResultCategory category = ResultCategory::Manual;
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
    UiControlLayout languageSelect;
    UiControlLayout telemetryToggle;
    UiControlLayout themeToggle;
    UiControlLayout primaryButton;
    UiControlLayout settingsButton;
    UiControlLayout technicalDetailsButton;
};

struct AppState {
    HINSTANCE instance = nullptr;
    HWND window = nullptr;
    vibeready::ui::Foundation ui;
    UiLayout layout;
    StartupChecks checks;
    Preferences preferences;
    AppRoute route = AppRoute::Startup;
    int scanStep = 0;
    bool scanningActive = false;
    bool showTechnicalDetails = false;
    bool initialized = false;
    float dpi = 96.0f;
    UiControl hoveredControl = UiControl::None;
    UiControl pressedControl = UiControl::None;
    UiControl focusedControl = UiControl::None;
    bool trackingMouse = false;
};

AppState g_state;

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

void SavePreferences() {
    if (!g_state.checks.configWritable) {
        g_state.preferences.saved = false;
        return;
    }

    std::wstring path = ConfigPath();
    BOOL languageOk = WritePrivateProfileStringW(L"preferences", L"language", LocaleCode(g_state.preferences.locale).c_str(), path.c_str());
    BOOL telemetryOk = WritePrivateProfileStringW(L"preferences", L"telemetry", g_state.preferences.telemetryAllowed ? L"1" : L"0", path.c_str());
    BOOL themeOk = WritePrivateProfileStringW(L"preferences", L"darkTheme", g_state.preferences.darkTheme ? L"1" : L"0", path.c_str());
    g_state.preferences.saved = languageOk && telemetryOk && themeOk;
    if (g_state.preferences.saved) {
        g_state.preferences.languageKnown = true;
        g_state.preferences.telemetryKnown = true;
        g_state.preferences.themeKnown = true;
    }
}

bool ArePreferencesComplete() {
    return g_state.preferences.languageKnown && g_state.preferences.telemetryKnown && g_state.preferences.themeKnown;
}

void EnterLanguageTelemetry() {
    g_state.route = AppRoute::LanguageTelemetry;
    g_state.showTechnicalDetails = false;
}

void EnterHome() {
    g_state.route = AppRoute::Home;
    g_state.showTechnicalDetails = false;
}

void EnterScanResults() {
    g_state.route = AppRoute::ScanResults;
    g_state.showTechnicalDetails = false;
}

void EnterScanning() {
    g_state.route = AppRoute::Scanning;
    g_state.scanStep = 0;
    g_state.scanningActive = true;
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
    return g_state.preferences.darkTheme ? vibeready::ui::ThemeMode::Dark : vibeready::ui::ThemeMode::Light;
}

UiControlLayout& MutableLayout(UiControl control) {
    switch (control) {
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
        UiControl::LanguageSelect,
        UiControl::TelemetryToggle,
        UiControl::ThemeToggle,
        UiControl::PrimaryButton,
        UiControl::SettingsButton,
        UiControl::TechnicalDetailsButton,
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

    if (g_state.route == AppRoute::LanguageTelemetry) {
        float panelLeft = margin;
        float panelRight = std::min(right, panelLeft + 520.0f);
        SetControl(&g_state.layout.languageSelect, D2D1::RectF(panelLeft + 20.0f, 242.0f, panelRight - 20.0f, 286.0f), true);
        SetControl(&g_state.layout.telemetryToggle, D2D1::RectF(panelLeft + 20.0f, 300.0f, panelRight - 20.0f, 344.0f), true);
        SetControl(&g_state.layout.themeToggle, D2D1::RectF(panelLeft + 20.0f, 358.0f, panelRight - 20.0f, 402.0f), true);
        SetControl(&g_state.layout.primaryButton, D2D1::RectF(panelLeft + 20.0f, 426.0f, panelLeft + 260.0f, 470.0f), true);
    } else if (g_state.route == AppRoute::Scanning) {
        SetControl(&g_state.layout.primaryButton, D2D1::RectF(margin, bottomActionY, margin + 220.0f, bottomActionY + 44.0f), true, false);
    } else if (g_state.route == AppRoute::Home || g_state.route == AppRoute::ScanResults) {
        float buttonY = g_state.route == AppRoute::Home ? 250.0f : bottomActionY;
        SetControl(&g_state.layout.primaryButton, D2D1::RectF(margin, buttonY, margin + 306.0f, buttonY + 44.0f), true);
        SetControl(&g_state.layout.settingsButton, D2D1::RectF(margin + 324.0f, buttonY, margin + 448.0f, buttonY + 44.0f), true);
        SetControl(&g_state.layout.technicalDetailsButton, D2D1::RectF(std::max(margin + 466.0f, right - 190.0f), buttonY, right, buttonY + 44.0f), true);
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

std::wstring RouteTitle() {
    const Copy& copy = Text(g_state.preferences.locale);
    switch (g_state.route) {
    case AppRoute::Startup:
        return copy.startupTitle;
    case AppRoute::LanguageTelemetry:
        return copy.languageTelemetryTitle;
    case AppRoute::Scanning:
        return copy.scanningTitle;
    case AppRoute::ScanResults:
        return copy.scanResultsTitle;
    case AppRoute::Home:
    default:
        return L"VibeReady";
    }
}

std::wstring RouteBody() {
    const Copy& copy = Text(g_state.preferences.locale);
    switch (g_state.route) {
    case AppRoute::Startup:
        return std::wstring(copy.startupBody) + L" " + copy.startupHint;
    case AppRoute::LanguageTelemetry:
        return copy.languageTelemetryBody;
    case AppRoute::Scanning:
        return copy.scanningBody;
    case AppRoute::ScanResults:
        return copy.scanResultsBody;
    case AppRoute::Home:
    default:
        return copy.homeBody;
    }
}

vibeready::ui::StatusTone ToneFor(bool ok) {
    return ok ? vibeready::ui::StatusTone::Success : vibeready::ui::StatusTone::Warning;
}

std::wstring ValueFor(bool ok) {
    const Copy& copy = Text(g_state.preferences.locale);
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
    const Copy& copy = Text(g_state.preferences.locale);
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
    const Copy& copy = Text(g_state.preferences.locale);
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
    const Copy& copy = Text(g_state.preferences.locale);
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
    const Copy& copy = Text(g_state.preferences.locale);
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

std::wstring BuildFriendlyStatusText();
std::wstring BuildTechnicalStatusText();

void DrawHeader() {
    vibeready::ui::Foundation& ui = g_state.ui;
    const vibeready::ui::UiTokens& tokens = ui.Tokens();
    D2D1_SIZE_F size = ui.Size();
    float margin = size.width < 720.0f ? 28.0f : 40.0f;
    float right = std::max(margin + 320.0f, size.width - margin);

    ui.DrawTextBlock(L"VibeReady", D2D1::RectF(margin, 28.0f, right, 70.0f),
        vibeready::ui::TextRole::Display, tokens.colors.text);
    ui.DrawTextBlock(RouteTitle(), D2D1::RectF(margin, 84.0f, right, 112.0f),
        vibeready::ui::TextRole::Title, tokens.colors.text);
    ui.DrawTextBlock(RouteBody(), D2D1::RectF(margin, 120.0f, right, 182.0f),
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
    Locale locale = g_state.preferences.locale;
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
    const Copy& copy = Text(g_state.preferences.locale);
    const vibeready::ui::UiTokens& tokens = ui.Tokens();
    D2D1_SIZE_F size = ui.Size();
    float margin = size.width < 720.0f ? 28.0f : 40.0f;
    float panelRight = std::min(size.width - margin, margin + 520.0f);
    D2D1_RECT_F panel = D2D1::RectF(margin, 198.0f, panelRight, 494.0f);

    ui.DrawSection(vibeready::ui::SectionSpec{panel, copy.languageTelemetryTitle});
    ui.DrawSelect(vibeready::ui::SelectSpec{
        g_state.layout.languageSelect.rect,
        copy.languageLabel,
        copy.languageName,
        ControlStateFor(UiControl::LanguageSelect)});
    ui.DrawToggle(vibeready::ui::ToggleSpec{
        g_state.layout.telemetryToggle.rect,
        copy.telemetryLabel,
        g_state.preferences.telemetryAllowed ? copy.toggleOn : copy.toggleOff,
        g_state.preferences.telemetryAllowed,
        ControlStateFor(UiControl::TelemetryToggle)});
    ui.DrawToggle(vibeready::ui::ToggleSpec{
        g_state.layout.themeToggle.rect,
        copy.themeLabel,
        g_state.preferences.darkTheme ? copy.toggleOn : copy.toggleOff,
        g_state.preferences.darkTheme,
        ControlStateFor(UiControl::ThemeToggle)});
    ui.DrawButton(vibeready::ui::ButtonSpec{
        g_state.layout.primaryButton.rect,
        copy.savePreferencesAction,
        ControlStateFor(UiControl::PrimaryButton),
        true});

    if (!g_state.checks.configWritable && g_state.initialized) {
        D2D1_RECT_F note = D2D1::RectF(panel.left + 20.0f, panel.bottom - 42.0f, panel.right - 20.0f, panel.bottom - 16.0f);
        ui.DrawTextBlock(copy.configWarning, note, vibeready::ui::TextRole::Caption, tokens.colors.warning);
    }
}

void DrawStatusSurface(const D2D1_RECT_F& sectionRect) {
    vibeready::ui::Foundation& ui = g_state.ui;
    const Copy& copy = Text(g_state.preferences.locale);
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
    const Copy& copy = Text(g_state.preferences.locale);
    D2D1_SIZE_F size = ui.Size();
    float margin = size.width < 720.0f ? 28.0f : 40.0f;
    float right = std::max(margin + 320.0f, size.width - margin);
    float statusTop = g_state.route == AppRoute::Home ? 322.0f : 212.0f;
    float statusBottom = std::max(statusTop + 248.0f, size.height - 112.0f);
    if (g_state.route == AppRoute::ScanResults) {
        statusBottom = std::max(statusTop + 250.0f, size.height - 104.0f);
    }

    ui.DrawButton(vibeready::ui::ButtonSpec{
        g_state.layout.primaryButton.rect,
        g_state.route == AppRoute::ScanResults ? copy.rescanAction : copy.primaryAction,
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
    const Copy& copy = Text(g_state.preferences.locale);
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

void DrawMainWindow(HWND hwnd) {
    UpdateLayout(hwnd);
    DrawHeader();

    switch (g_state.route) {
    case AppRoute::LanguageTelemetry:
        DrawSetupScreen();
        break;
    case AppRoute::Scanning:
        DrawScanningScreen();
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
    Locale locale = g_state.preferences.locale;
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
    const Copy& copy = Text(g_state.preferences.locale);
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
    const Copy& copy = Text(g_state.preferences.locale);
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

void ActivateControl(UiControl control) {
    if (!ControlEnabled(control)) {
        return;
    }

    switch (control) {
    case UiControl::LanguageSelect:
        g_state.preferences.locale = NextLocale(g_state.preferences.locale, 1);
        g_state.preferences.saved = false;
        AppendLog(L"language preference changed");
        RefreshUi();
        break;
    case UiControl::TelemetryToggle:
        g_state.preferences.telemetryAllowed = !g_state.preferences.telemetryAllowed;
        g_state.preferences.saved = false;
        AppendLog(g_state.preferences.telemetryAllowed ? L"telemetry consent allowed" : L"telemetry consent declined");
        RefreshUi();
        break;
    case UiControl::ThemeToggle:
        g_state.preferences.darkTheme = !g_state.preferences.darkTheme;
        g_state.preferences.saved = false;
        AppendLog(g_state.preferences.darkTheme ? L"dark theme enabled" : L"dark theme disabled");
        RefreshUi();
        break;
    case UiControl::SettingsButton:
        EnterLanguageTelemetry();
        RefreshUi();
        break;
    case UiControl::TechnicalDetailsButton:
        g_state.showTechnicalDetails = !g_state.showTechnicalDetails;
        RefreshUi();
        break;
    case UiControl::PrimaryButton:
        if (g_state.route == AppRoute::LanguageTelemetry) {
            SavePreferences();
            if (g_state.preferences.saved) {
                EnterHome();
                AppendLog(L"preferences saved and entered home");
                RefreshUi();
            } else {
                MessageBoxW(g_state.window, Text(g_state.preferences.locale).unsupportedLanguageTelemetry, kWindowTitle, MB_ICONERROR | MB_OK);
            }
        } else if (g_state.route == AppRoute::Home) {
            EnterScanning();
            SetTimer(g_state.window, kScanTimerId, 500, nullptr);
            AppendLog(L"primary environment check started");
            RefreshUi();
        } else if (g_state.route == AppRoute::ScanResults) {
            EnterScanning();
            SetTimer(g_state.window, kScanTimerId, 500, nullptr);
            RefreshUi();
            AppendLog(L"read-only environment rescan started");
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
                const Copy& copy = Text(g_state.preferences.locale);
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
        if (g_state.focusedControl == UiControl::LanguageSelect &&
            (wparam == VK_LEFT || wparam == VK_UP || wparam == VK_RIGHT || wparam == VK_DOWN)) {
            int direction = (wparam == VK_LEFT || wparam == VK_UP) ? -1 : 1;
            g_state.preferences.locale = NextLocale(g_state.preferences.locale, direction);
            g_state.preferences.saved = false;
            RefreshUi();
            return 0;
        }
        if (wparam == VK_ESCAPE && g_state.route == AppRoute::LanguageTelemetry && ArePreferencesComplete()) {
            EnterHome();
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
