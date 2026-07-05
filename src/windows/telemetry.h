#pragma once

#include <string>
#include <vector>

namespace vibeready::telemetry {

enum class FieldType {
    String,
    Number,
    Boolean,
};

struct Field {
    std::wstring name;
    std::wstring value;
    FieldType type = FieldType::String;
};

struct Context {
    std::wstring appDataDir;
    std::wstring endpoint;
    std::wstring appVersion;
    std::wstring osMajorVersion;
    std::wstring architecture;
    std::wstring language;
    bool consentEnabled = false;
};

Field Text(std::wstring name, std::wstring value);
Field Number(std::wstring name, long long value);
Field Boolean(std::wstring name, bool value);

void Initialize(const Context& context);
void UpdateContext(const Context& context);
void SetConsent(bool enabled);
void ClearQueue();
void Track(const std::wstring& eventName, const std::vector<Field>& fields = {});
void FlushAsync();
std::wstring NewId();

}  // namespace vibeready::telemetry
