#include "ui_foundation.h"

#include <algorithm>

namespace vibeready::ui {
namespace {

D2D1_COLOR_F Rgb(unsigned int rgb, float alpha = 1.0f) {
    return D2D1_COLOR_F{
        static_cast<float>((rgb >> 16) & 0xff) / 255.0f,
        static_cast<float>((rgb >> 8) & 0xff) / 255.0f,
        static_cast<float>(rgb & 0xff) / 255.0f,
        alpha};
}

ThemeColors LightColors() {
    return ThemeColors{
        Rgb(0xf6f7f9),
        Rgb(0xffffff),
        Rgb(0xeef2f5),
        Rgb(0xd7dee6),
        Rgb(0x171a1f),
        Rgb(0x59636f),
        Rgb(0x176b70),
        Rgb(0xffffff),
        Rgb(0x16734d),
        Rgb(0xa66300),
        Rgb(0xb42318),
        Rgb(0x2f7df6),
        Rgb(0xb7c0ca)};
}

ThemeColors DarkColors() {
    return ThemeColors{
        Rgb(0x111418),
        Rgb(0x1b2026),
        Rgb(0x252b33),
        Rgb(0x39424d),
        Rgb(0xf0f3f5),
        Rgb(0xb7c0c8),
        Rgb(0x5bb8b2),
        Rgb(0x071416),
        Rgb(0x65c08f),
        Rgb(0xe2a23a),
        Rgb(0xef6b62),
        Rgb(0x84a9ff),
        Rgb(0x606b78)};
}

D2D1_COLOR_F SysColor(int index) {
    COLORREF color = GetSysColor(index);
    return D2D1_COLOR_F{
        static_cast<float>(GetRValue(color)) / 255.0f,
        static_cast<float>(GetGValue(color)) / 255.0f,
        static_cast<float>(GetBValue(color)) / 255.0f,
        1.0f};
}

ThemeColors HighContrastColors() {
    return ThemeColors{
        SysColor(COLOR_WINDOW),
        SysColor(COLOR_WINDOW),
        SysColor(COLOR_BTNFACE),
        SysColor(COLOR_WINDOWTEXT),
        SysColor(COLOR_WINDOWTEXT),
        SysColor(COLOR_GRAYTEXT),
        SysColor(COLOR_HIGHLIGHT),
        SysColor(COLOR_HIGHLIGHTTEXT),
        SysColor(COLOR_HIGHLIGHT),
        SysColor(COLOR_HIGHLIGHT),
        SysColor(COLOR_HIGHLIGHT),
        SysColor(COLOR_HIGHLIGHT),
        SysColor(COLOR_GRAYTEXT)};
}

UiTokens TokensFor(ThemeMode mode) {
    UiTokens tokens;
    if (mode == ThemeMode::HighContrast) {
        tokens.colors = HighContrastColors();
    } else if (mode == ThemeMode::Dark) {
        tokens.colors = DarkColors();
    } else {
        tokens.colors = LightColors();
    }
    return tokens;
}

D2D1_ROUNDED_RECT Rounded(const D2D1_RECT_F& rect, float radius) {
    return D2D1::RoundedRect(rect, radius, radius);
}

D2D1_RECT_F Inset(D2D1_RECT_F rect, float dx, float dy) {
    rect.left += dx;
    rect.right -= dx;
    rect.top += dy;
    rect.bottom -= dy;
    return rect;
}

D2D1_RECT_F Offset(D2D1_RECT_F rect, float dx, float dy) {
    rect.left += dx;
    rect.right += dx;
    rect.top += dy;
    rect.bottom += dy;
    return rect;
}

DWRITE_FONT_WEIGHT WeightFor(TextRole role) {
    switch (role) {
    case TextRole::Display:
    case TextRole::Title:
    case TextRole::Button:
        return DWRITE_FONT_WEIGHT_SEMI_BOLD;
    case TextRole::Code:
    case TextRole::Caption:
    case TextRole::Body:
    default:
        return DWRITE_FONT_WEIGHT_NORMAL;
    }
}

float FontSizeFor(const UiTokens& tokens, TextRole role) {
    switch (role) {
    case TextRole::Display:
        return tokens.fontDisplay;
    case TextRole::Title:
        return tokens.fontTitle;
    case TextRole::Caption:
        return tokens.fontCaption;
    case TextRole::Code:
        return tokens.fontCode;
    case TextRole::Button:
    case TextRole::Body:
    default:
        return tokens.fontBody;
    }
}

const wchar_t* FontFamilyFor(TextRole role) {
    return role == TextRole::Code ? L"Cascadia Mono" : L"Segoe UI";
}

}  // namespace

HRESULT Foundation::Initialize(HWND hwnd) {
    hwnd_ = hwnd;
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf());
    if (FAILED(hr)) {
        return hr;
    }
    return DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf()));
}

void Foundation::SetDpi(float dpi) {
    dpi_ = dpi > 0.0f ? dpi : 96.0f;
    if (renderTarget_) {
        renderTarget_->SetDpi(dpi_, dpi_);
    }
}

void Foundation::Resize(UINT width, UINT height) {
    size_ = D2D1::SizeF(static_cast<float>(width) * 96.0f / dpi_, static_cast<float>(height) * 96.0f / dpi_);
}

void Foundation::DiscardDeviceResources() {
    renderTarget_.Reset();
}

HRESULT Foundation::EnsureDeviceResources() {
    if (renderTarget_) {
        return S_OK;
    }
    if (!hwnd_ || !d2dFactory_) {
        return E_FAIL;
    }

    D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), dpi_, dpi_);
    HRESULT hr = d2dFactory_->CreateDCRenderTarget(&properties, renderTarget_.GetAddressOf());
    if (SUCCEEDED(hr)) {
        renderTarget_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    }
    return hr;
}

HRESULT Foundation::BeginDraw(ThemeMode themeMode, HDC dc) {
    themeMode_ = themeMode;
    tokens_ = TokensFor(themeMode_);
    HRESULT hr = EnsureDeviceResources();
    if (FAILED(hr)) {
        return hr;
    }

    RECT rect = {};
    GetClientRect(hwnd_, &rect);
    hr = renderTarget_->BindDC(dc, &rect);
    if (FAILED(hr)) {
        return hr;
    }
    renderTarget_->SetDpi(dpi_, dpi_);
    size_ = renderTarget_->GetSize();
    renderTarget_->BeginDraw();
    renderTarget_->SetTransform(D2D1::Matrix3x2F::Identity());
    renderTarget_->Clear(tokens_.colors.background);
    return S_OK;
}

HRESULT Foundation::EndDraw() {
    if (!renderTarget_) {
        return E_FAIL;
    }
    HRESULT hr = renderTarget_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
    }
    return hr;
}

D2D1_SIZE_F Foundation::Size() const {
    return size_;
}

const UiTokens& Foundation::Tokens() const {
    return tokens_;
}

HRESULT Foundation::CreateBrush(const D2D1_COLOR_F& color, ID2D1SolidColorBrush** brush) {
    if (!renderTarget_) {
        return E_FAIL;
    }
    return renderTarget_->CreateSolidColorBrush(color, brush);
}

HRESULT Foundation::CreateTextFormat(TextRole role, DWRITE_TEXT_ALIGNMENT alignment,
    DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment, IDWriteTextFormat** format) {
    if (!dwriteFactory_) {
        return E_FAIL;
    }
    HRESULT hr = dwriteFactory_->CreateTextFormat(
        FontFamilyFor(role),
        nullptr,
        WeightFor(role),
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        FontSizeFor(tokens_, role),
        L"",
        format);
    if (FAILED(hr)) {
        return hr;
    }
    (*format)->SetTextAlignment(alignment);
    (*format)->SetParagraphAlignment(paragraphAlignment);
    (*format)->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    return S_OK;
}

D2D1_COLOR_F Foundation::ToneColor(StatusTone tone) const {
    switch (tone) {
    case StatusTone::Accent:
        return tokens_.colors.accent;
    case StatusTone::Success:
        return tokens_.colors.success;
    case StatusTone::Warning:
        return tokens_.colors.warning;
    case StatusTone::Danger:
        return tokens_.colors.danger;
    case StatusTone::Neutral:
    default:
        return tokens_.colors.textMuted;
    }
}

D2D1_COLOR_F Foundation::Muted(D2D1_COLOR_F color, float alpha) const {
    color.a *= alpha;
    return color;
}

void Foundation::DrawTextBlock(const std::wstring& text, const D2D1_RECT_F& rect, TextRole role, const D2D1_COLOR_F& color,
    DWRITE_TEXT_ALIGNMENT alignment, DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment) {
    if (text.empty() || !renderTarget_) {
        return;
    }
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    if (FAILED(CreateBrush(color, brush.GetAddressOf())) ||
        FAILED(CreateTextFormat(role, alignment, paragraphAlignment, format.GetAddressOf()))) {
        return;
    }
    renderTarget_->DrawTextW(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        format.Get(),
        rect,
        brush.Get(),
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void Foundation::DrawSection(const SectionSpec& spec) {
    if (!renderTarget_) {
        return;
    }
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> surface;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> border;
    if (FAILED(CreateBrush(tokens_.colors.surface, surface.GetAddressOf())) ||
        FAILED(CreateBrush(tokens_.colors.border, border.GetAddressOf()))) {
        return;
    }

    renderTarget_->FillRoundedRectangle(Rounded(spec.rect, tokens_.radiusMd), surface.Get());
    renderTarget_->DrawRoundedRectangle(Rounded(spec.rect, tokens_.radiusMd), border.Get(), tokens_.borderWidth);

    if (!spec.title.empty()) {
        D2D1_RECT_F titleRect = Inset(spec.rect, tokens_.spaceMd, tokens_.spaceSm);
        titleRect.bottom = titleRect.top + 30.0f;
        DrawTextBlock(spec.title, titleRect, TextRole::Title, tokens_.colors.text);
    }
}

void Foundation::DrawButton(const ButtonSpec& spec) {
    if (!renderTarget_) {
        return;
    }

    D2D1_COLOR_F fill = spec.primary ? tokens_.colors.accent : tokens_.colors.surfaceAlt;
    D2D1_COLOR_F text = spec.primary ? tokens_.colors.accentText : tokens_.colors.text;
    D2D1_COLOR_F border = spec.primary ? tokens_.colors.accent : tokens_.colors.border;

    switch (spec.state) {
    case ControlState::Hover:
        fill = spec.primary ? Muted(tokens_.colors.accent, 0.88f) : tokens_.colors.surface;
        break;
    case ControlState::Pressed:
        fill = spec.primary ? Muted(tokens_.colors.accent, 0.72f) : tokens_.colors.border;
        break;
    case ControlState::Focused:
        border = tokens_.colors.focus;
        break;
    case ControlState::Disabled:
        fill = tokens_.colors.surfaceAlt;
        text = tokens_.colors.disabled;
        border = tokens_.colors.border;
        break;
    case ControlState::Loading:
        fill = spec.primary ? Muted(tokens_.colors.accent, 0.64f) : tokens_.colors.surfaceAlt;
        text = spec.primary ? tokens_.colors.accentText : tokens_.colors.textMuted;
        border = tokens_.colors.focus;
        break;
    case ControlState::Normal:
    default:
        break;
    }

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> fillBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> borderBrush;
    if (FAILED(CreateBrush(fill, fillBrush.GetAddressOf())) || FAILED(CreateBrush(border, borderBrush.GetAddressOf()))) {
        return;
    }

    D2D1_RECT_F rect = spec.state == ControlState::Pressed ? Offset(spec.rect, 0.0f, 1.0f) : spec.rect;
    renderTarget_->FillRoundedRectangle(Rounded(rect, tokens_.radiusSm), fillBrush.Get());
    renderTarget_->DrawRoundedRectangle(Rounded(rect, tokens_.radiusSm), borderBrush.Get(), tokens_.borderWidth);

    DrawTextBlock(spec.text, Inset(rect, tokens_.spaceMd, 0.0f), TextRole::Button, text,
        DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    if (spec.state == ControlState::Focused || spec.state == ControlState::Loading) {
        D2D1_RECT_F focusRect = Inset(rect, 3.0f, 3.0f);
        renderTarget_->DrawRoundedRectangle(Rounded(focusRect, tokens_.radiusSm), borderBrush.Get(), 1.0f);
    }
    if (spec.state == ControlState::Loading) {
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> dotBrush;
        if (SUCCEEDED(CreateBrush(text, dotBrush.GetAddressOf()))) {
            float cy = (rect.top + rect.bottom) * 0.5f;
            float start = rect.left + tokens_.spaceMd;
            for (int i = 0; i < 3; ++i) {
                renderTarget_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(start + i * 8.0f, cy), 2.0f, 2.0f), dotBrush.Get());
            }
        }
    }
}

void Foundation::DrawToggle(const ToggleSpec& spec) {
    if (!renderTarget_) {
        return;
    }

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> fillBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> borderBrush;
    if (FAILED(CreateBrush(tokens_.colors.surfaceAlt, fillBrush.GetAddressOf())) ||
        FAILED(CreateBrush(spec.state == ControlState::Focused ? tokens_.colors.focus : tokens_.colors.border, borderBrush.GetAddressOf()))) {
        return;
    }

    renderTarget_->FillRoundedRectangle(Rounded(spec.rect, tokens_.radiusSm), fillBrush.Get());
    renderTarget_->DrawRoundedRectangle(Rounded(spec.rect, tokens_.radiusSm), borderBrush.Get(), tokens_.borderWidth);

    D2D1_RECT_F labelRect = Inset(spec.rect, tokens_.spaceMd, 0.0f);
    labelRect.right -= 94.0f;
    DrawTextBlock(spec.label, labelRect, TextRole::Body,
        spec.state == ControlState::Disabled ? tokens_.colors.disabled : tokens_.colors.text,
        DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    D2D1_RECT_F valueRect = labelRect;
    valueRect.left = std::max(labelRect.left, spec.rect.right - 172.0f);
    valueRect.right = spec.rect.right - 78.0f;
    DrawTextBlock(spec.value, valueRect, TextRole::Caption,
        spec.state == ControlState::Disabled ? tokens_.colors.disabled : tokens_.colors.textMuted,
        DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    D2D1_RECT_F track{spec.rect.right - 64.0f, spec.rect.top + 10.0f, spec.rect.right - 18.0f, spec.rect.bottom - 10.0f};
    D2D1_COLOR_F trackColor = spec.on ? tokens_.colors.accent : tokens_.colors.border;
    if (spec.state == ControlState::Disabled) {
        trackColor = tokens_.colors.disabled;
    } else if (spec.state == ControlState::Pressed) {
        trackColor = Muted(trackColor, 0.72f);
    }
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> trackBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> knobBrush;
    if (FAILED(CreateBrush(trackColor, trackBrush.GetAddressOf())) ||
        FAILED(CreateBrush(tokens_.colors.surface, knobBrush.GetAddressOf()))) {
        return;
    }
    renderTarget_->FillRoundedRectangle(Rounded(track, 12.0f), trackBrush.Get());
    float knobX = spec.on ? track.right - 12.0f : track.left + 12.0f;
    renderTarget_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(knobX, (track.top + track.bottom) * 0.5f), 8.0f, 8.0f), knobBrush.Get());
}

void Foundation::DrawSelect(const SelectSpec& spec) {
    if (!renderTarget_) {
        return;
    }

    D2D1_COLOR_F fill = spec.state == ControlState::Pressed ? tokens_.colors.border : tokens_.colors.surfaceAlt;
    D2D1_COLOR_F border = spec.state == ControlState::Focused ? tokens_.colors.focus : tokens_.colors.border;
    if (spec.state == ControlState::Hover) {
        fill = tokens_.colors.surface;
    }

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> fillBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> borderBrush;
    if (FAILED(CreateBrush(fill, fillBrush.GetAddressOf())) || FAILED(CreateBrush(border, borderBrush.GetAddressOf()))) {
        return;
    }
    renderTarget_->FillRoundedRectangle(Rounded(spec.rect, tokens_.radiusSm), fillBrush.Get());
    renderTarget_->DrawRoundedRectangle(Rounded(spec.rect, tokens_.radiusSm), borderBrush.Get(), tokens_.borderWidth);

    D2D1_RECT_F labelRect = Inset(spec.rect, tokens_.spaceMd, 0.0f);
    labelRect.right = labelRect.left + 108.0f;
    DrawTextBlock(spec.label, labelRect, TextRole::Caption, tokens_.colors.textMuted,
        DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    D2D1_RECT_F valueRect = Inset(spec.rect, tokens_.spaceMd, 0.0f);
    valueRect.left += 116.0f;
    valueRect.right -= 34.0f;
    DrawTextBlock(spec.value, valueRect, TextRole::Body, tokens_.colors.text,
        DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> arrowBrush;
    if (SUCCEEDED(CreateBrush(tokens_.colors.textMuted, arrowBrush.GetAddressOf()))) {
        float cx = spec.rect.right - 24.0f;
        float cy = (spec.rect.top + spec.rect.bottom) * 0.5f;
        renderTarget_->DrawLine(D2D1::Point2F(cx - 5.0f, cy - 2.0f), D2D1::Point2F(cx, cy + 4.0f), arrowBrush.Get(), 1.6f);
        renderTarget_->DrawLine(D2D1::Point2F(cx, cy + 4.0f), D2D1::Point2F(cx + 5.0f, cy - 2.0f), arrowBrush.Get(), 1.6f);
    }
}

void Foundation::DrawStatusRow(const StatusRowSpec& spec) {
    if (!renderTarget_) {
        return;
    }

    D2D1_COLOR_F tone = ToneColor(spec.tone);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> lineBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> dotBrush;
    if (FAILED(CreateBrush(tokens_.colors.border, lineBrush.GetAddressOf())) ||
        FAILED(CreateBrush(tone, dotBrush.GetAddressOf()))) {
        return;
    }

    renderTarget_->DrawLine(D2D1::Point2F(spec.rect.left, spec.rect.bottom),
        D2D1::Point2F(spec.rect.right, spec.rect.bottom), lineBrush.Get(), 1.0f);
    float cy = (spec.rect.top + spec.rect.bottom) * 0.5f;
    renderTarget_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(spec.rect.left + 8.0f, cy), 4.0f, 4.0f), dotBrush.Get());

    D2D1_RECT_F labelRect{spec.rect.left + 22.0f, spec.rect.top, spec.rect.right - 148.0f, spec.rect.bottom};
    D2D1_RECT_F valueRect{std::max(spec.rect.left + 156.0f, spec.rect.right - 144.0f), spec.rect.top, spec.rect.right, spec.rect.bottom};
    DrawTextBlock(spec.label, labelRect, TextRole::Body, tokens_.colors.text,
        DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    DrawTextBlock(spec.value, valueRect, TextRole::Caption, tone,
        DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}

void Foundation::DrawProgressBar(const D2D1_RECT_F& rect, float progress, ControlState state) {
    if (!renderTarget_) {
        return;
    }
    float clamped = std::max(0.0f, std::min(1.0f, progress));
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> trackBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> fillBrush;
    D2D1_COLOR_F fill = state == ControlState::Disabled ? tokens_.colors.disabled : tokens_.colors.accent;
    if (FAILED(CreateBrush(tokens_.colors.surfaceAlt, trackBrush.GetAddressOf())) ||
        FAILED(CreateBrush(fill, fillBrush.GetAddressOf()))) {
        return;
    }
    renderTarget_->FillRoundedRectangle(Rounded(rect, 5.0f), trackBrush.Get());
    D2D1_RECT_F fillRect = rect;
    fillRect.right = fillRect.left + (rect.right - rect.left) * clamped;
    renderTarget_->FillRoundedRectangle(Rounded(fillRect, 5.0f), fillBrush.Get());
}

}  // namespace vibeready::ui
