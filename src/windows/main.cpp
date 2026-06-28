#include <windows.h>
#include <shlobj.h>

#include <string>
#include <vector>

namespace {

constexpr wchar_t kWindowClassName[] = L"VibeReadyMainWindow";
constexpr wchar_t kWindowTitle[] = L"VibeReady";
constexpr wchar_t kSingleInstanceMutex[] = L"Local\\VibeReadySingleInstance";
constexpr UINT_PTR kStartupTimerId = 1;
constexpr int kLanguageComboControlId = 101;
constexpr int kTelemetryCheckControlId = 102;
constexpr int kPrimaryButtonControlId = 103;
constexpr int kStatusTextControlId = 104;

enum class Locale {
    En,
    ZhCn,
    Ja,
};

enum class ToolState {
    Ready,
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

struct Copy {
    const wchar_t* languageName;
    const wchar_t* startupTitle;
    const wchar_t* startupBody;
    const wchar_t* homeBody;
    const wchar_t* trustNote;
    const wchar_t* primaryAction;
    const wchar_t* telemetryLabel;
    const wchar_t* telemetryOn;
    const wchar_t* telemetryOff;
    const wchar_t* settingsSaved;
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
};

const Copy& Text(Locale locale) {
    static const Copy en = {
        L"English",
        L"Starting VibeReady",
        L"Checking whether this Windows device can run the portable client.",
        L"Check this device before starting web vibe coding work.",
        L"Scan phase is read-only. VibeReady will not install tools or modify the system until you confirm a fix.",
        L"Check web vibe coding environment",
        L"Send anonymous usage data",
        L"Telemetry: allowed",
        L"Telemetry: off",
        L"Preferences saved.",
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
        L"Administrator confirmation is not expected for the current scan."
    };
    static const Copy zh = {
        L"简体中文",
        L"正在启动 VibeReady",
        L"正在检查这台 Windows 设备是否可以运行免安装客户端。",
        L"开始网页 vibe coding 工作前，先检查这台设备。",
        L"扫描阶段只读。除非你确认修复，VibeReady 不会安装工具或修改系统。",
        L"检查网页 vibe coding 环境",
        L"发送匿名使用数据",
        L"遥测：允许",
        L"遥测：关闭",
        L"偏好设置已保存。",
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
        L"当前扫描预计不会要求管理员确认。"
    };
    static const Copy ja = {
        L"日本語",
        L"VibeReady を起動中",
        L"この Windows デバイスでポータブルクライアントを実行できるか確認しています。",
        L"Web vibe coding 作業を始める前に、このデバイスをチェックします。",
        L"スキャン段階は読み取り専用です。修復を確認するまで、VibeReady はツールのインストールやシステム変更を行いません。",
        L"Web vibe coding 環境をチェック",
        L"匿名の利用データを送信する",
        L"テレメトリー: 許可",
        L"テレメトリー: オフ",
        L"設定を保存しました。",
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
        L"現在のスキャンでは管理者確認は不要です。"
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
    bool loaded = false;
    bool saved = false;
};

struct AppState {
    HINSTANCE instance = nullptr;
    HWND window = nullptr;
    HWND languageCombo = nullptr;
    HWND telemetryCheck = nullptr;
    HWND primaryButton = nullptr;
    HWND statusText = nullptr;
    StartupChecks checks;
    Preferences preferences;
    bool initialized = false;
};

AppState g_state;

HMENU ControlId(int id) {
    return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
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

    wchar_t locale[32] = {};
    GetPrivateProfileStringW(L"preferences", L"language", L"", locale, 32, path.c_str());
    if (locale[0] != L'\0') {
        g_state.preferences.locale = LocaleFromCode(locale);
        g_state.preferences.loaded = true;
    }

    int telemetry = GetPrivateProfileIntW(L"preferences", L"telemetry", -1, path.c_str());
    if (telemetry >= 0) {
        g_state.preferences.telemetryAllowed = telemetry == 1;
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
    g_state.preferences.saved = languageOk && telemetryOk;
}

StartupChecks RunStartupChecks() {
    StartupChecks checks;
    checks.systemResult = RunSystemScan(&checks.systemDetails);
    checks.supportedWindows = checks.systemDetails.isWindows10Or11;
    checks.x64 = checks.systemDetails.isX64;
    checks.administrator = checks.systemDetails.isAdministrator;
    checks.mayRequireUacForInstall = checks.systemDetails.mayRequireUacForInstall;

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

void AddLanguageItems(HWND combo) {
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"简体中文"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"日本語"));
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

std::wstring CheckLine(const wchar_t* name, bool ok) {
    const Copy& copy = Text(g_state.preferences.locale);
    return std::wstring(L"  ") + name + L": " + (ok ? copy.supported : copy.unsupported) + L"\r\n";
}

std::wstring StateName(ToolState state) {
    switch (state) {
    case ToolState::Ready:
        return L"ready";
    case ToolState::Unsupported:
        return L"unsupported";
    case ToolState::Error:
    default:
        return L"error";
    }
}

std::wstring BuildStatusText() {
    const Copy& copy = Text(g_state.preferences.locale);
    std::wstring text = std::wstring(copy.diagnosticsTitle) + L"\r\n";
    text += L"  system: " + StateName(g_state.checks.systemResult.state);
    if (!g_state.checks.systemResult.errorCode.empty()) {
        text += L" (" + g_state.checks.systemResult.errorCode + L")";
    }
    text += L"\r\n";
    text += L"  OS: " + g_state.checks.systemResult.detectedVersion + L"\r\n";
    text += CheckLine(L"Windows 10/11", g_state.checks.supportedWindows);
    text += CheckLine(L"x64", g_state.checks.x64);
    text += std::wstring(L"  Permission: ") + (g_state.checks.administrator ? copy.administrator : copy.standardUser) + L"\r\n";
    text += std::wstring(L"  UAC: ") + (g_state.checks.mayRequireUacForInstall ? copy.uacMayAppear : copy.uacNotExpected) + L"\r\n";
    text += CheckLine(L"Temp", g_state.checks.tempWritable);
    text += CheckLine(L"Config", g_state.checks.configWritable);
    text += CheckLine(L"Log", g_state.checks.logWritable);
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

void RefreshUi() {
    const Copy& copy = Text(g_state.preferences.locale);
    SetWindowTextW(g_state.window, kWindowTitle);
    SetWindowTextW(g_state.telemetryCheck, copy.telemetryLabel);
    SendMessageW(g_state.telemetryCheck, BM_SETCHECK, g_state.preferences.telemetryAllowed ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_state.languageCombo, CB_SETCURSEL, LocaleIndex(g_state.preferences.locale), 0);
    SetWindowTextW(g_state.primaryButton, copy.primaryAction);
    SetWindowTextW(g_state.statusText, BuildStatusText().c_str());
    InvalidateRect(g_state.window, nullptr, TRUE);
}

void PaintMainWindow(HWND hwnd) {
    PAINTSTRUCT paint;
    HDC dc = BeginPaint(hwnd, &paint);
    RECT rect;
    GetClientRect(hwnd, &rect);

    const Copy& copy = Text(g_state.preferences.locale);
    SetBkMode(dc, TRANSPARENT);

    HFONT titleFont = CreateFontW(34, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT bodyFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(dc, titleFont));

    RECT titleRect{32, 28, rect.right - 220, 72};
    DrawTextW(dc, L"VibeReady", -1, &titleRect, DT_LEFT | DT_TOP | DT_SINGLELINE);

    SelectObject(dc, bodyFont);
    RECT bodyRect{32, 86, rect.right - 32, 150};
    DrawTextW(dc, copy.homeBody, -1, &bodyRect, DT_LEFT | DT_TOP | DT_WORDBREAK);

    RECT trustRect{32, 156, rect.right - 32, 230};
    DrawTextW(dc, copy.trustNote, -1, &trustRect, DT_LEFT | DT_TOP | DT_WORDBREAK);

    SelectObject(dc, oldFont);
    DeleteObject(titleFont);
    DeleteObject(bodyFont);
    EndPaint(hwnd, &paint);
}

void LayoutControls(HWND hwnd) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    MoveWindow(g_state.languageCombo, rect.right - 184, 28, 152, 120, TRUE);
    MoveWindow(g_state.telemetryCheck, 32, 236, 360, 28, TRUE);
    MoveWindow(g_state.primaryButton, 32, 280, 312, 40, TRUE);
    MoveWindow(g_state.statusText, 32, 340, rect.right - 64, rect.bottom - 368, TRUE);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        g_state.window = hwnd;
        g_state.languageCombo = CreateWindowExW(0, L"COMBOBOX", nullptr,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            0, 0, 0, 0, hwnd, ControlId(kLanguageComboControlId), g_state.instance, nullptr);
        g_state.telemetryCheck = CreateWindowExW(0, L"BUTTON", nullptr,
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, 0, 0, hwnd, ControlId(kTelemetryCheckControlId), g_state.instance, nullptr);
        g_state.primaryButton = CreateWindowExW(0, L"BUTTON", nullptr,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, ControlId(kPrimaryButtonControlId), g_state.instance, nullptr);
        g_state.statusText = CreateWindowExW(0, L"STATIC", nullptr,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 0, 0, hwnd, ControlId(kStatusTextControlId), g_state.instance, nullptr);

        AddLanguageItems(g_state.languageCombo);
        SetTimer(hwnd, kStartupTimerId, 50, nullptr);
        return 0;

    case WM_TIMER:
        if (wparam == kStartupTimerId) {
            KillTimer(hwnd, kStartupTimerId);
            g_state.checks = RunStartupChecks();
            LoadPreferences();
            SavePreferences();
            AppendLog(L"startup checks completed");
            g_state.initialized = true;
            RefreshUi();
            if (!g_state.checks.supportedWindows || !g_state.checks.x64) {
                const Copy& copy = Text(g_state.preferences.locale);
                MessageBoxW(hwnd, copy.unsupportedSystemBody, copy.unsupportedSystemTitle, MB_ICONERROR | MB_OK);
            }
        }
        return 0;

    case WM_SIZE:
        LayoutControls(hwnd);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wparam) == kLanguageComboControlId && HIWORD(wparam) == CBN_SELCHANGE) {
            int index = static_cast<int>(SendMessageW(g_state.languageCombo, CB_GETCURSEL, 0, 0));
            g_state.preferences.locale = LocaleFromIndex(index);
            SavePreferences();
            AppendLog(L"language preference changed");
            RefreshUi();
            return 0;
        }
        if (LOWORD(wparam) == kTelemetryCheckControlId && HIWORD(wparam) == BN_CLICKED) {
            g_state.preferences.telemetryAllowed = SendMessageW(g_state.telemetryCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            SavePreferences();
            AppendLog(g_state.preferences.telemetryAllowed ? L"telemetry consent allowed" : L"telemetry consent declined");
            RefreshUi();
            return 0;
        }
        if (LOWORD(wparam) == kPrimaryButtonControlId && HIWORD(wparam) == BN_CLICKED) {
            AppendLog(L"primary environment check entry clicked");
            MessageBoxW(hwnd, Text(g_state.preferences.locale).trustNote, kWindowTitle, MB_ICONINFORMATION | MB_OK);
            return 0;
        }
        break;

    case WM_PAINT:
        PaintMainWindow(hwnd);
        return 0;

    case WM_DESTROY:
        AppendLog(L"shutdown");
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
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClassName;

    return RegisterClassExW(&windowClass) != 0;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
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
        760,
        560,
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
