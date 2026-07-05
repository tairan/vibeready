#include "telemetry.h"

#include <windows.h>
#include <winhttp.h>
#include <objbase.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <utility>
#include <cwctype>

namespace vibeready::telemetry {
namespace {

constexpr size_t kMaxQueueEvents = 200;
constexpr uintmax_t kMaxQueueBytes = 256 * 1024;
constexpr size_t kMaxBatchEvents = 20;

struct TelemetryState {
    Context context;
    std::wstring installationId;
    std::wstring sessionId;
    bool initialized = false;
    bool flushRunning = false;
    std::mutex mutex;
};

TelemetryState g_state;

const std::set<std::wstring> kCommonFields = {
    L"event_name",
    L"event_version",
    L"occurred_at",
    L"app_version",
    L"installation_id",
    L"session_id",
    L"os_name",
    L"os_major_version",
    L"architecture",
    L"language",
    L"consent_state",
};

const std::map<std::wstring, std::set<std::wstring>> kEventFields = {
    {L"app_opened", {L"launch_result"}},
    {L"startup_check_completed", {L"overall_state", L"error_code", L"duration_ms"}},
    {L"language_selected", {L"selected_language", L"source"}},
    {L"telemetry_consent_changed", {L"new_consent_state", L"source"}},
    {L"scan_started", {L"scan_id"}},
    {L"tool_check_completed", {L"scan_id", L"tool", L"state", L"error_code", L"duration_ms"}},
    {L"scan_completed", {L"scan_id", L"overall_state", L"missing_required_count", L"unusable_required_count", L"unsupported_count", L"duration_ms"}},
    {L"repair_plan_created", {L"scan_id", L"repairable_count", L"manual_count", L"requires_uac_possible"}},
    {L"repair_started", {L"repair_id", L"tool", L"method"}},
    {L"repair_step_completed", {L"repair_id", L"tool", L"result", L"error_code", L"duration_ms"}},
    {L"repair_completed", {L"repair_id", L"overall_state", L"completed_count", L"failed_count", L"duration_ms"}},
    {L"verification_started", {L"verification_id"}},
    {L"verification_completed", {L"verification_id", L"overall_state", L"error_code", L"duration_ms"}},
    {L"ready_reached", {L"verified_tool_count", L"duration_from_open_ms"}},
};

const std::vector<std::wstring> kDisallowedFieldTerms = {
    L"full_path",
    L"path",
    L"folder_path",
    L"username",
    L"account_name",
    L"environment_variable",
    L"env",
    L"project_name",
    L"project_content",
    L"source_code",
    L"prompt",
    L"file_content",
    L"directory_listing",
    L"email",
    L"secret",
    L"api_key",
    L"token",
    L"credential",
    L"cookie",
    L"authorization_header",
    L"raw_command_output",
    L"raw_log",
    L"crash_dump",
    L"screenshot",
    L"clipboard",
};

std::wstring ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });
    return value;
}

bool ContainsInsensitive(const std::wstring& value, const std::wstring& term) {
    return ToLower(value).find(ToLower(term)) != std::wstring::npos;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return "";
    }
    int needed = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) {
        return "";
    }
    std::string result(static_cast<size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), needed, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return L"";
    }
    int needed = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    if (needed <= 0) {
        return L"";
    }
    std::wstring result(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), needed);
    return result;
}

std::filesystem::path QueuePath() {
    return std::filesystem::path(g_state.context.appDataDir) / L"telemetry-queue.jsonl";
}

std::filesystem::path InstallationIdPath() {
    return std::filesystem::path(g_state.context.appDataDir) / L"installation.id";
}

std::string JsonEscapeUtf8(const std::wstring& value) {
    std::string input = WideToUtf8(value);
    std::string output;
    output.reserve(input.size() + 8);
    for (unsigned char ch : input) {
        switch (ch) {
        case '"':
            output += "\\\"";
            break;
        case '\\':
            output += "\\\\";
            break;
        case '\b':
            output += "\\b";
            break;
        case '\f':
            output += "\\f";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            if (ch < 0x20) {
                char buffer[8] = {};
                wsprintfA(buffer, "\\u%04x", ch);
                output += buffer;
            } else {
                output.push_back(static_cast<char>(ch));
            }
            break;
        }
    }
    return output;
}

std::wstring UtcNowIso8601() {
    SYSTEMTIME now = {};
    GetSystemTime(&now);
    wchar_t buffer[32] = {};
    wsprintfW(buffer, L"%04u-%02u-%02uT%02u:%02u:%02uZ",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    return buffer;
}

bool HasDisallowedFieldName(const std::wstring& fieldName) {
    for (const std::wstring& term : kDisallowedFieldTerms) {
        if (ContainsInsensitive(fieldName, term)) {
            return true;
        }
    }
    return false;
}

bool HasSensitiveValue(const std::wstring& value) {
    if (value.find(L":\\") != std::wstring::npos ||
        value.find(L"\\\\") != std::wstring::npos ||
        value.find(L"@") != std::wstring::npos ||
        ContainsInsensitive(value, L"ghp_") ||
        ContainsInsensitive(value, L"github_pat_") ||
        ContainsInsensitive(value, L"bearer ") ||
        ContainsInsensitive(value, L"api_key") ||
        ContainsInsensitive(value, L"secret") ||
        ContainsInsensitive(value, L"token")) {
        return true;
    }
    return false;
}

bool IsFieldAllowed(const std::wstring& eventName, const Field& field) {
    if (HasDisallowedFieldName(field.name) || HasSensitiveValue(field.value)) {
        return false;
    }
    auto eventIt = kEventFields.find(eventName);
    if (eventIt == kEventFields.end()) {
        return false;
    }
    return kCommonFields.count(field.name) > 0 || eventIt->second.count(field.name) > 0;
}

std::wstring ReadTextFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return L"";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    while (!content.empty() && (content.back() == '\r' || content.back() == '\n')) {
        content.pop_back();
    }
    return Utf8ToWide(content);
}

void WriteTextFile(const std::filesystem::path& path, const std::wstring& value) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    std::string utf8 = WideToUtf8(value);
    file.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
}

bool EnsureInstallationIdLocked() {
    if (!g_state.installationId.empty()) {
        return true;
    }
    if (g_state.context.appDataDir.empty()) {
        return false;
    }
    std::filesystem::create_directories(g_state.context.appDataDir);
    std::wstring existing = ReadTextFile(InstallationIdPath());
    if (!existing.empty()) {
        g_state.installationId = existing;
        return true;
    }
    g_state.installationId = NewId();
    if (g_state.installationId.empty()) {
        return false;
    }
    WriteTextFile(InstallationIdPath(), g_state.installationId);
    return true;
}

std::vector<std::string> ReadQueueLinesLocked() {
    std::vector<std::string> lines;
    std::ifstream file(QueuePath(), std::ios::binary);
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

void WriteQueueLinesLocked(const std::vector<std::string>& lines) {
    std::filesystem::create_directories(std::filesystem::path(g_state.context.appDataDir));
    std::ofstream file(QueuePath(), std::ios::binary | std::ios::trunc);
    for (const std::string& line : lines) {
        file << line << "\n";
    }
}

void TrimQueueLocked() {
    std::vector<std::string> lines = ReadQueueLinesLocked();
    bool changed = false;
    while (lines.size() > kMaxQueueEvents) {
        lines.erase(lines.begin());
        changed = true;
    }

    uintmax_t size = 0;
    for (const std::string& line : lines) {
        size += line.size() + 1;
    }
    while (size > kMaxQueueBytes && !lines.empty()) {
        size -= lines.front().size() + 1;
        lines.erase(lines.begin());
        changed = true;
    }
    if (changed) {
        WriteQueueLinesLocked(lines);
    }
}

std::string FieldJson(const Field& field) {
    std::string result = "\"" + JsonEscapeUtf8(field.name) + "\":";
    switch (field.type) {
    case FieldType::Number:
        result += WideToUtf8(field.value);
        break;
    case FieldType::Boolean:
        result += (field.value == L"true") ? "true" : "false";
        break;
    case FieldType::String:
    default:
        result += "\"" + JsonEscapeUtf8(field.value) + "\"";
        break;
    }
    return result;
}

bool IsEndpointAllowed(const std::wstring& endpoint) {
    if (endpoint.empty()) {
        return false;
    }
    std::wstring lower = ToLower(endpoint);
    return lower.rfind(L"https://", 0) == 0 ||
        lower.rfind(L"http://127.0.0.1", 0) == 0 ||
        lower.rfind(L"http://localhost", 0) == 0;
}

bool BuildBatchBody(const std::vector<std::string>& lines, std::string* body) {
    if (lines.empty()) {
        return false;
    }
    *body = "{\"events\":[";
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            *body += ",";
        }
        *body += lines[i];
    }
    *body += "]}";
    return true;
}

bool PostBatch(const std::wstring& endpoint, const std::vector<std::string>& lines, DWORD* statusCode) {
    *statusCode = 0;
    std::string body;
    if (!BuildBatchBody(lines, &body)) {
        return false;
    }

    URL_COMPONENTSW components = {};
    components.dwStructSize = sizeof(components);
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    components.lpszHostName = host;
    components.dwHostNameLength = ARRAYSIZE(host);
    components.lpszUrlPath = path;
    components.dwUrlPathLength = ARRAYSIZE(path);
    if (!WinHttpCrackUrl(endpoint.c_str(), 0, 0, &components)) {
        return false;
    }

    bool secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    HINTERNET session = WinHttpOpen(L"VibeReady/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        return false;
    }
    WinHttpSetTimeouts(session, 3000, 3000, 3000, 3000);

    HINTERNET connection = WinHttpConnect(session, std::wstring(host, components.dwHostNameLength).c_str(), components.nPort, 0);
    if (!connection) {
        WinHttpCloseHandle(session);
        return false;
    }

    std::wstring requestPath(path, components.dwUrlPathLength);
    if (requestPath.empty()) {
        requestPath = L"/";
    }
    HINTERNET request = WinHttpOpenRequest(connection, L"POST", requestPath.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
    if (!request) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    const wchar_t headers[] = L"Content-Type: application/json\r\n";
    BOOL ok = WinHttpSendRequest(request, headers, static_cast<DWORD>(-1), body.data(),
                  static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) &&
        WinHttpReceiveResponse(request, nullptr);
    if (ok) {
        DWORD statusSize = sizeof(*statusCode);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return ok;
}

DWORD WINAPI FlushThreadProc(void*) {
    Context context;
    std::vector<std::string> lines;
    {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        context = g_state.context;
        if (!context.consentEnabled || !IsEndpointAllowed(context.endpoint)) {
            g_state.flushRunning = false;
            return 0;
        }
        lines = ReadQueueLinesLocked();
        if (lines.empty()) {
            g_state.flushRunning = false;
            return 0;
        }
    }

    std::vector<std::string> batch;
    batch.reserve(kMaxBatchEvents);
    for (size_t i = 0; i < std::min(kMaxBatchEvents, lines.size()); ++i) {
        batch.push_back(lines[i]);
    }

    DWORD status = 0;
    bool posted = PostBatch(context.endpoint, batch, &status);
    bool removeBatch = posted && ((status >= 200 && status < 300) || (status >= 400 && status < 500));

    {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        if (removeBatch) {
            std::vector<std::string> current = ReadQueueLinesLocked();
            if (current.size() >= batch.size()) {
                current.erase(current.begin(), current.begin() + static_cast<std::ptrdiff_t>(batch.size()));
                WriteQueueLinesLocked(current);
            }
        }
        g_state.flushRunning = false;
    }
    return 0;
}

}  // namespace

Field Text(std::wstring name, std::wstring value) {
    return Field{std::move(name), std::move(value), FieldType::String};
}

Field Number(std::wstring name, long long value) {
    return Field{std::move(name), std::to_wstring(value), FieldType::Number};
}

Field Boolean(std::wstring name, bool value) {
    return Field{std::move(name), value ? L"true" : L"false", FieldType::Boolean};
}

std::wstring NewId() {
    GUID guid = {};
    if (FAILED(CoCreateGuid(&guid))) {
        return L"";
    }
    wchar_t buffer[48] = {};
    if (StringFromGUID2(guid, buffer, ARRAYSIZE(buffer)) == 0) {
        return L"";
    }
    std::wstring value(buffer);
    if (value.size() >= 2 && value.front() == L'{' && value.back() == L'}') {
        value = value.substr(1, value.size() - 2);
    }
    return ToLower(value);
}

void Initialize(const Context& context) {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    g_state.context = context;
    g_state.sessionId = NewId();
    g_state.initialized = true;
    if (g_state.context.consentEnabled) {
        EnsureInstallationIdLocked();
    } else {
        ClearQueue();
    }
}

void UpdateContext(const Context& context) {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    g_state.context = context;
    if (g_state.context.consentEnabled) {
        EnsureInstallationIdLocked();
    }
}

void SetConsent(bool enabled) {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    g_state.context.consentEnabled = enabled;
    if (enabled) {
        EnsureInstallationIdLocked();
    } else {
        ClearQueue();
    }
}

void ClearQueue() {
    if (g_state.context.appDataDir.empty()) {
        return;
    }
    std::error_code ignored;
    std::filesystem::remove(QueuePath(), ignored);
}

void Track(const std::wstring& eventName, const std::vector<Field>& fields) {
    {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        if (!g_state.initialized || !g_state.context.consentEnabled || !EnsureInstallationIdLocked()) {
            return;
        }
        if (kEventFields.find(eventName) == kEventFields.end()) {
            return;
        }

        std::vector<Field> allFields = {
            Number(L"event_version", 1),
            Text(L"occurred_at", UtcNowIso8601()),
            Text(L"app_version", g_state.context.appVersion),
            Text(L"installation_id", g_state.installationId),
            Text(L"session_id", g_state.sessionId),
            Text(L"os_name", L"windows"),
            Text(L"consent_state", L"enabled"),
        };
        if (!g_state.context.osMajorVersion.empty()) {
            allFields.push_back(Text(L"os_major_version", g_state.context.osMajorVersion));
        }
        if (!g_state.context.architecture.empty()) {
            allFields.push_back(Text(L"architecture", g_state.context.architecture));
        }
        if (!g_state.context.language.empty()) {
            allFields.push_back(Text(L"language", g_state.context.language));
        }
        for (const Field& field : fields) {
            if (!IsFieldAllowed(eventName, field)) {
                return;
            }
            allFields.push_back(field);
        }

        std::string line = "{\"event_name\":\"" + JsonEscapeUtf8(eventName) + "\"";
        for (const Field& field : allFields) {
            line += ",";
            line += FieldJson(field);
        }
        line += "}";

        std::filesystem::create_directories(g_state.context.appDataDir);
        std::ofstream queue(QueuePath(), std::ios::binary | std::ios::app);
        queue << line << "\n";
        TrimQueueLocked();
    }
    FlushAsync();
}

void FlushAsync() {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    if (g_state.flushRunning || !g_state.context.consentEnabled || !IsEndpointAllowed(g_state.context.endpoint)) {
        return;
    }
    g_state.flushRunning = true;
    HANDLE thread = CreateThread(nullptr, 0, FlushThreadProc, nullptr, 0, nullptr);
    if (!thread) {
        g_state.flushRunning = false;
        return;
    }
    CloseHandle(thread);
}

}  // namespace vibeready::telemetry
