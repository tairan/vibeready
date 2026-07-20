#pragma once

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>
#include <wrl/client.h>

#include <string>

namespace vibeready::ui {

enum class ThemeMode {
    Light,
    Dark,
    HighContrast,
};

enum class ControlState {
    Normal,
    Hover,
    Pressed,
    Disabled,
    Focused,
    Loading,
};

enum class TextRole {
    Display,
    Title,
    Body,
    Caption,
    Code,
    Button,
};

enum class StatusTone {
    Neutral,
    Accent,
    Success,
    Warning,
    Danger,
};

struct ThemeColors {
    D2D1_COLOR_F background;
    D2D1_COLOR_F surface;
    D2D1_COLOR_F surfaceAlt;
    D2D1_COLOR_F border;
    D2D1_COLOR_F text;
    D2D1_COLOR_F textMuted;
    D2D1_COLOR_F accent;
    D2D1_COLOR_F accentText;
    D2D1_COLOR_F success;
    D2D1_COLOR_F warning;
    D2D1_COLOR_F danger;
    D2D1_COLOR_F focus;
    D2D1_COLOR_F disabled;
};

struct UiTokens {
    ThemeColors colors;
    float spaceXs = 6.0f;
    float spaceSm = 10.0f;
    float spaceMd = 16.0f;
    float spaceLg = 24.0f;
    float radiusSm = 6.0f;
    float radiusMd = 8.0f;
    float borderWidth = 1.0f;
    float controlHeight = 44.0f;
    float compactControlHeight = 36.0f;
    float fontDisplay = 34.0f;
    float fontTitle = 20.0f;
    float fontBody = 14.5f;
    float fontCaption = 12.5f;
    float fontCode = 12.5f;
};

struct SectionSpec {
    D2D1_RECT_F rect;
    std::wstring title;
};

struct ButtonSpec {
    D2D1_RECT_F rect;
    std::wstring text;
    ControlState state = ControlState::Normal;
    bool primary = false;
};

struct ToggleSpec {
    D2D1_RECT_F rect;
    std::wstring label;
    std::wstring value;
    bool on = false;
    ControlState state = ControlState::Normal;
};

struct SelectSpec {
    D2D1_RECT_F rect;
    std::wstring label;
    std::wstring value;
    ControlState state = ControlState::Normal;
};

struct StatusRowSpec {
    D2D1_RECT_F rect;
    std::wstring label;
    std::wstring value;
    StatusTone tone = StatusTone::Neutral;
    ControlState state = ControlState::Normal;
};

class Foundation {
public:
    HRESULT Initialize(HWND hwnd);
    void SetDpi(float dpi);
    void Resize(UINT width, UINT height);
    void DiscardDeviceResources();

    HRESULT BeginDraw(ThemeMode themeMode, HDC dc);
    HRESULT EndDraw();

    D2D1_SIZE_F Size() const;
    const UiTokens& Tokens() const;

    void DrawTextBlock(const std::wstring& text, const D2D1_RECT_F& rect, TextRole role, const D2D1_COLOR_F& color,
        DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_LEADING,
        DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment = DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    void DrawSection(const SectionSpec& spec);
    void DrawButton(const ButtonSpec& spec);
    void DrawToggle(const ToggleSpec& spec);
    void DrawSelect(const SelectSpec& spec);
    void DrawStatusRow(const StatusRowSpec& spec);
    void DrawProgressBar(const D2D1_RECT_F& rect, float progress, ControlState state);

private:
    HRESULT EnsureDeviceResources();
    HRESULT CreateBrush(const D2D1_COLOR_F& color, ID2D1SolidColorBrush** brush);
    HRESULT CreateTextFormat(TextRole role, DWRITE_TEXT_ALIGNMENT alignment, DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment,
        IDWriteTextFormat** format);
    D2D1_COLOR_F ToneColor(StatusTone tone) const;
    D2D1_COLOR_F Muted(D2D1_COLOR_F color, float alpha) const;

    HWND hwnd_ = nullptr;
    float dpi_ = 96.0f;
    ThemeMode themeMode_ = ThemeMode::Light;
    UiTokens tokens_ = {};
    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    D2D1_SIZE_F size_ = D2D1::SizeF(0.0f, 0.0f);
    Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> renderTarget_;
};

}  // namespace vibeready::ui
