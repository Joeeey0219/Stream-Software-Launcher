#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <stdexcept>
#include <string>
#include <vector>

#include "launcher.hpp"
#include "autostart.hpp"
#include "update_check.hpp"
#include "color_math.hpp"

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif

using stream_launcher::Settings;
using stream_launcher::ThemeMode;

namespace {

enum ControlId {
    Obs = 100, Vts, Onecomme, MixIt, Rtmp,
    BrowseObs = 200, BrowseVts, BrowseOnecomme, BrowseMixIt, BrowseRtmp,
    AdminObs = 300, SteamMode, DirectMode, SyncOnecomme, SyncMixIt, SyncRtmp, SyncTwitch,
    Launch = 400, About, TwitchOnly,
    TitleLabel = 500, VersionLabel, CoreLabel, OptionalLabel, VtsHintLabel,
    AutoStart = 600, ThemeSystem, ThemeLight, ThemeDark, UpdateCheck, AboutApply,
    ColorPicker, LanguageZh, LanguageEn, ColorHexEdit, ColorApply, ColorCancel,
};

HINSTANCE instance = nullptr;
HFONT font = nullptr;
HFONT title_font = nullptr;
HFONT section_font = nullptr;
HFONT small_font = nullptr;
HBRUSH background_brush = nullptr;
HBRUSH card_brush = nullptr;
HBRUSH input_brush = nullptr;
std::array<HWND, 5> edits{};
std::array<HWND, 7> checks{};
HWND about_window = nullptr;
HWND color_window = nullptr;
bool dark_theme = false;
ThemeMode theme_mode = ThemeMode::system;
ThemeMode pending_theme_mode = ThemeMode::system;
bool english_ui = false;
bool pending_english_ui = false;
COLORREF picker_color = RGB(44, 93, 204);
double picker_hue = 0.0;
double picker_sat = 0.0;
double picker_value = 0.0;
bool picker_drag_square = false;
bool picker_drag_hue = false;
bool picker_edit_updating = false;
std::array<std::uint32_t, 256 * 160> picker_square_pixels{};
std::array<std::uint32_t, 256 * 18> picker_hue_pixels{};
double picker_rendered_hue = -1.0;
bool picker_hue_pixels_ready = false;

COLORREF background_color = RGB(244, 247, 251);
COLORREF card_color = RGB(255, 255, 255);
COLORREF input_color = RGB(255, 255, 255);
COLORREF text_color = RGB(28, 39, 58);
COLORREF muted_color = RGB(101, 115, 136);
COLORREF accent_color = RGB(44, 93, 204);
COLORREF pending_accent_color = RGB(44, 93, 204);
COLORREF border_color = RGB(219, 227, 238);

const wchar_t* loc(const wchar_t* zh, const wchar_t* en) { return english_ui ? en : zh; }

bool system_dark_theme() {
    DWORD light = 1;
    DWORD size = sizeof(light);
    const LSTATUS result = RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    return result == ERROR_SUCCESS && light == 0;
}

bool effective_dark(ThemeMode mode) {
    return mode == ThemeMode::dark || (mode == ThemeMode::system && system_dark_theme());
}

std::wstring color_hex(COLORREF color) {
    wchar_t result[8]{};
    swprintf_s(result, L"#%02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));
    return result;
}

COLORREF parse_color(const std::wstring& hex) {
    if (hex.size() != 7 || hex[0] != L'#') return RGB(44, 93, 204);
    unsigned int rgb = 0;
    if (swscanf_s(hex.c_str() + 1, L"%06X", &rgb) != 1) return RGB(44, 93, 204);
    return RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
}

bool valid_color_hex(const std::wstring& hex) {
    if (hex.size() != 7 || hex[0] != L'#') return false;
    return std::all_of(hex.begin() + 1, hex.end(), [](wchar_t digit) {
        return (digit >= L'0' && digit <= L'9') || (digit >= L'a' && digit <= L'f') ||
               (digit >= L'A' && digit <= L'F');
    });
}

COLORREF hsv_color(double hue, double saturation, double value) {
    const auto rgb = stream_launcher::hsv_rgb(hue, saturation, value);
    return RGB(rgb.r, rgb.g, rgb.b);
}

void color_hsv(COLORREF color, double& hue, double& saturation, double& value) {
    const double r = GetRValue(color) / 255.0;
    const double g = GetGValue(color) / 255.0;
    const double b = GetBValue(color) / 255.0;
    const double maximum = std::max({r, g, b});
    const double minimum = std::min({r, g, b});
    const double delta = maximum - minimum;
    value = maximum;
    saturation = maximum == 0.0 ? 0.0 : delta / maximum;
    if (delta == 0.0) hue = 0.0;
    else if (maximum == r) hue = 60.0 * std::fmod((g - b) / delta + 6.0, 6.0);
    else if (maximum == g) hue = 60.0 * ((b - r) / delta + 2.0);
    else hue = 60.0 * ((r - g) / delta + 4.0);
}

void set_palette(bool dark) {
    background_color = dark ? RGB(17, 23, 33) : RGB(244, 247, 251);
    card_color = dark ? RGB(30, 39, 53) : RGB(255, 255, 255);
    input_color = dark ? RGB(39, 49, 65) : RGB(255, 255, 255);
    text_color = dark ? RGB(238, 242, 248) : RGB(28, 39, 58);
    muted_color = dark ? RGB(170, 184, 204) : RGB(101, 115, 136);
    border_color = dark ? RGB(68, 82, 103) : RGB(219, 227, 238);
    if (background_brush) DeleteObject(background_brush);
    if (card_brush) DeleteObject(card_brush);
    if (input_brush) DeleteObject(input_brush);
    background_brush = CreateSolidBrush(background_color);
    card_brush = CreateSolidBrush(card_color);
    input_brush = CreateSolidBrush(input_color);
}

void style_title_bar(HWND window) {
    const BOOL enabled = dark_theme ? TRUE : FALSE;
    DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &enabled, sizeof(enabled));
    const COLORREF caption = dark_theme ? RGB(30, 39, 53) : RGB(255, 255, 255);
    const COLORREF title = dark_theme ? RGB(238, 242, 248) : RGB(28, 39, 58);
    DwmSetWindowAttribute(window, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
    DwmSetWindowAttribute(window, DWMWA_TEXT_COLOR, &title, sizeof(title));
}

void repaint_children(HWND window) {
    if (!window) return;
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

std::wstring text_of(HWND item);
bool save(HWND owner);

HFONT make_font(int size, int weight = FW_NORMAL) {
    return CreateFontW(-size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}

void use_font(HWND item, HFONT chosen) {
    SendMessageW(item, WM_SETFONT, reinterpret_cast<WPARAM>(chosen), TRUE);
}

HWND control(HWND parent, const wchar_t* type, const wchar_t* title, DWORD style,
             int x, int y, int width, int height, int id = 0) {
    HWND item = CreateWindowExW(0, type, title, WS_CHILD | WS_VISIBLE | style,
                                x, y, width, height, parent,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    if (item && font) use_font(item, font);
    return item;
}

void paint_card(HDC dc, int left, int top, int right, int bottom) {
    HPEN pen = CreatePen(PS_SOLID, 1, border_color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, card_brush);
    RoundRect(dc, left, top, right, bottom, 16, 16);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

void paint_button(const DRAWITEMSTRUCT& item) {
    const COLORREF current_accent = item.CtlID == ColorApply ? picker_color :
        GetParent(item.hwndItem) == about_window ? pending_accent_color : accent_color;
    if ((item.CtlID >= ThemeSystem && item.CtlID <= ThemeDark) ||
        item.CtlID == LanguageZh || item.CtlID == LanguageEn) {
        FillRect(item.hDC, &item.rcItem, card_brush);
        const bool selected = item.CtlID == ThemeSystem ? pending_theme_mode == ThemeMode::system :
                              item.CtlID == ThemeLight ? pending_theme_mode == ThemeMode::light :
                              item.CtlID == ThemeDark ? pending_theme_mode == ThemeMode::dark :
                              item.CtlID == LanguageZh ? !pending_english_ui : pending_english_ui;
        const COLORREF fill = selected ? current_accent : input_color;
        HBRUSH brush = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, 1, selected ? current_accent : border_color);
        HGDIOBJ old_brush = SelectObject(item.hDC, brush);
        HGDIOBJ old_pen = SelectObject(item.hDC, pen);
        RECT area = item.rcItem;
        InflateRect(&area, -2, -2);
        RoundRect(item.hDC, area.left, area.top, area.right, area.bottom, 10, 10);
        SelectObject(item.hDC, old_brush);
        SelectObject(item.hDC, old_pen);
        DeleteObject(brush);
        DeleteObject(pen);
        SetBkMode(item.hDC, TRANSPARENT);
        const double luminance = 0.2126 * GetRValue(fill) + 0.7152 * GetGValue(fill) + 0.0722 * GetBValue(fill);
        SetTextColor(item.hDC, selected ? (luminance > 150 ? RGB(18, 24, 34) : RGB(255, 255, 255)) : text_color);
        HGDIOBJ old_font = SelectObject(item.hDC, font);
        const std::wstring caption = text_of(item.hwndItem);
        DrawTextW(item.hDC, caption.c_str(), -1, &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(item.hDC, old_font);
        if (item.itemState & ODS_FOCUS) DrawFocusRect(item.hDC, &area);
        return;
    }
    if ((item.CtlID >= AdminObs && item.CtlID <= SyncTwitch) || item.CtlID == AutoStart) {
        FillRect(item.hDC, &item.rcItem, card_brush);
        const bool selected = GetWindowLongPtrW(item.hwndItem, GWLP_USERDATA) != 0;
        const bool radio = item.CtlID == SteamMode || item.CtlID == DirectMode;
        RECT mark{item.rcItem.left + 2, item.rcItem.top + 6, item.rcItem.left + 19, item.rcItem.top + 23};
        HBRUSH brush = CreateSolidBrush(selected ? current_accent : input_color);
        HPEN pen = CreatePen(PS_SOLID, 1, selected ? current_accent : border_color);
        HGDIOBJ old_brush = SelectObject(item.hDC, brush);
        HGDIOBJ old_pen = SelectObject(item.hDC, pen);
        if (radio) Ellipse(item.hDC, mark.left, mark.top, mark.right, mark.bottom);
        else RoundRect(item.hDC, mark.left, mark.top, mark.right, mark.bottom, 4, 4);
        if (selected) {
            const COLORREF ink = (0.2126 * GetRValue(current_accent) + 0.7152 * GetGValue(current_accent) +
                                   0.0722 * GetBValue(current_accent)) > 150 ? RGB(18, 24, 34) : RGB(255, 255, 255);
            HPEN inner_pen = CreatePen(PS_SOLID, 2, ink);
            SelectObject(item.hDC, inner_pen);
            if (radio) {
                HBRUSH inner_brush = CreateSolidBrush(ink);
                SelectObject(item.hDC, inner_brush);
                Ellipse(item.hDC, mark.left + 5, mark.top + 5, mark.right - 5, mark.bottom - 5);
                SelectObject(item.hDC, brush);
                DeleteObject(inner_brush);
            } else {
                MoveToEx(item.hDC, mark.left + 4, mark.top + 9, nullptr);
                LineTo(item.hDC, mark.left + 7, mark.top + 12);
                LineTo(item.hDC, mark.left + 13, mark.top + 5);
            }
            SelectObject(item.hDC, pen);
            DeleteObject(inner_pen);
        }
        SelectObject(item.hDC, old_pen);
        SelectObject(item.hDC, old_brush);
        DeleteObject(pen);
        DeleteObject(brush);
        RECT label = item.rcItem;
        label.left += 27;
        SetBkMode(item.hDC, TRANSPARENT);
        SetTextColor(item.hDC, text_color);
        HGDIOBJ old_font = SelectObject(item.hDC, font);
        const std::wstring caption = text_of(item.hwndItem);
        DrawTextW(item.hDC, caption.c_str(), -1, &label, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(item.hDC, old_font);
        if (item.itemState & ODS_FOCUS) DrawFocusRect(item.hDC, &item.rcItem);
        return;
    }
    const bool primary = item.CtlID == Launch || item.CtlID == ColorApply;
    const bool swatch = item.CtlID == ColorPicker;
    const COLORREF fill = swatch ? pending_accent_color : (primary ? current_accent : card_color);
    const COLORREF outline = primary || swatch ? fill : border_color;
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, outline);
    HGDIOBJ old_brush = SelectObject(item.hDC, brush);
    HGDIOBJ old_pen = SelectObject(item.hDC, pen);
    RECT rect = item.rcItem;
    if (item.itemState & ODS_SELECTED) OffsetRect(&rect, 0, 1);
    RoundRect(item.hDC, rect.left, rect.top, rect.right, rect.bottom, 12, 12);
    SetBkMode(item.hDC, TRANSPARENT);
    const double brightness = 0.2126 * GetRValue(fill) + 0.7152 * GetGValue(fill) + 0.0722 * GetBValue(fill);
    SetTextColor(item.hDC, primary || swatch ? (brightness > 150 ? RGB(18, 24, 34) : RGB(255, 255, 255)) : text_color);
    HGDIOBJ old_font = SelectObject(item.hDC, font);
    std::wstring caption = text_of(item.hwndItem);
    DrawTextW(item.hDC, caption.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(item.hDC, old_font);
    SelectObject(item.hDC, old_brush);
    SelectObject(item.hDC, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
    if (item.itemState & ODS_FOCUS) {
        RECT focus = item.rcItem;
        InflateRect(&focus, -4, -4);
        DrawFocusRect(item.hDC, &focus);
    }
}

std::wstring text_of(HWND item) {
    const int length = GetWindowTextLengthW(item);
    std::wstring text(length + 1, L'\0');
    GetWindowTextW(item, text.data(), length + 1);
    text.resize(length);
    return text;
}

bool checked(HWND item) {
    return GetWindowLongPtrW(item, GWLP_USERDATA) != 0;
}

void set_checked(HWND item, bool value) {
    SetWindowLongPtrW(item, GWLP_USERDATA, value ? 1 : 0);
    InvalidateRect(item, nullptr, TRUE);
}

bool browse(HWND owner, HWND edit) {
    std::array<wchar_t, 32768> path{};
    const std::wstring current = text_of(edit);
    if (current.size() < path.size()) current.copy(path.data(), current.size());
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = english_ui ? L"Executable (*.exe)\0*.exe\0All files (*.*)\0*.*\0\0"
                                    : L"執行檔 (*.exe)\0*.exe\0所有檔案 (*.*)\0*.*\0\0";
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&dialog)) return false;
    SetWindowTextW(edit, path.data());
    return true;
}

Settings from_ui() {
    Settings settings;
    settings.obs_path = text_of(edits[0]);
    settings.vts_path = text_of(edits[1]);
    settings.onecomme_path = text_of(edits[2]);
    settings.mix_it_path = text_of(edits[3]);
    settings.rtmp_path = text_of(edits[4]);
    settings.admin_obs = checked(checks[0]);
    settings.vts_steam = checked(checks[1]);
    settings.sync_onecomme = checked(checks[3]);
    settings.sync_mix_it = checked(checks[4]);
    settings.sync_rtmp = checked(checks[5]);
    settings.sync_twitch = checked(checks[6]);
    settings.theme_mode = theme_mode;
    settings.english = english_ui;
    settings.accent_hex = color_hex(accent_color);
    return settings;
}

void to_ui(const Settings& settings) {
    theme_mode = settings.theme_mode;
    dark_theme = effective_dark(theme_mode);
    english_ui = settings.english;
    accent_color = parse_color(settings.accent_hex);
    set_palette(dark_theme);
    for (size_t index = 0; index < edits.size(); ++index) {
        const std::wstring* paths[] = {&settings.obs_path, &settings.vts_path, &settings.onecomme_path,
                                       &settings.mix_it_path, &settings.rtmp_path};
        SetWindowTextW(edits[index], paths[index]->c_str());
    }
    set_checked(checks[0], settings.admin_obs);
    set_checked(checks[1], settings.vts_steam);
    set_checked(checks[2], !settings.vts_steam);
    set_checked(checks[3], settings.sync_onecomme);
    set_checked(checks[4], settings.sync_mix_it);
    set_checked(checks[5], settings.sync_rtmp);
    set_checked(checks[6], settings.sync_twitch);
    EnableWindow(edits[1], !settings.vts_steam);
    EnableWindow(GetDlgItem(GetParent(edits[1]), BrowseVts), !settings.vts_steam);
}

bool save(HWND owner) {
    std::wstring error;
    if (!stream_launcher::save_settings(from_ui(), error)) {
        const std::wstring detail = std::wstring(loc(L"設定沒有儲存成功：\n", L"Couldn't save your settings:\n")) +
                                    error + loc(L"\n請檢查後再試一次喔！", L"\nPlease check and try again.");
        MessageBoxW(owner, detail.c_str(), loc(L"錯誤", L"Error"), MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}

std::wstring join(const std::vector<std::wstring>& lines) {
    std::wstring result;
    for (const auto& line : lines) {
        if (!result.empty()) result += L"\n";
        result += line;
    }
    return result;
}

void refresh_main_text(HWND window) {
    SetWindowTextW(GetDlgItem(window, TitleLabel), loc(L"直播啟動器", L"Stream Launcher"));
    const std::wstring version = std::wstring(loc(L"版本 ", L"Version ")) + stream_launcher::kVersion;
    SetWindowTextW(GetDlgItem(window, VersionLabel), version.c_str());
    SetWindowTextW(GetDlgItem(window, CoreLabel), loc(L"主要程式", L"Main apps"));
    SetWindowTextW(GetDlgItem(window, OptionalLabel), loc(L"選用工具", L"Optional tools"));
    for (int id = BrowseObs; id <= BrowseRtmp; ++id)
        SetWindowTextW(GetDlgItem(window, id), loc(L"瀏覽", L"Browse"));
    SetWindowTextW(GetDlgItem(window, AdminObs), loc(L"以管理員身分啟動 OBS", L"Run OBS as administrator"));
    SetWindowTextW(GetDlgItem(window, SteamMode), loc(L"透過 Steam 啟動", L"Launch through Steam"));
    SetWindowTextW(GetDlgItem(window, DirectMode), loc(L"直接執行 exe", L"Run executable directly"));
    SetWindowTextW(GetDlgItem(window, VtsHintLabel),
                   loc(L"Steam 模式需先登入 Steam；直接模式可能無法使用 VNet。",
                       L"Sign in to Steam first; direct mode may disable VNet."));
    SetWindowTextW(GetDlgItem(window, SyncRtmp), loc(L"RTMP 工具", L"RTMP tool"));
    SetWindowTextW(GetDlgItem(window, SyncTwitch), loc(L"同步啟動 Twitch 伺服器測試", L"Also launch Twitch server test"));
    SetWindowTextW(GetDlgItem(window, About), loc(L"關於 / 設定", L"About / Settings"));
    SetWindowTextW(GetDlgItem(window, TwitchOnly), loc(L"Twitch 測試", L"Twitch test"));
    SetWindowTextW(GetDlgItem(window, Launch), loc(L"準備直播", L"Start stream"));
}

void make_ui(HWND window) {
    font = make_font(16);
    title_font = make_font(27, FW_SEMIBOLD);
    section_font = make_font(18, FW_SEMIBOLD);
    small_font = make_font(14);
    background_brush = CreateSolidBrush(background_color);
    card_brush = CreateSolidBrush(card_color);

    HWND title = control(window, L"STATIC", L"直播啟動器", 0, 30, 24, 400, 38, TitleLabel);
    use_font(title, title_font);
    const std::wstring version = std::wstring(L"版本 ") + stream_launcher::kVersion;
    HWND version_label = control(window, L"STATIC", version.c_str(), SS_RIGHT, 790, 37, 118, 24, VersionLabel);
    use_font(version_label, small_font);

    HWND core = control(window, L"STATIC", L"主要程式", 0, 50, 126, 300, 28, CoreLabel);
    use_font(core, section_font);
    control(window, L"STATIC", L"OBS", 0, 52, 157, 105, 25);
    edits[0] = control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
                       180, 151, 610, 32, Obs);
    control(window, L"BUTTON", L"瀏覽", BS_OWNERDRAW | WS_TABSTOP, 802, 151, 86, 32, BrowseObs);
    checks[0] = control(window, L"BUTTON", L"以管理員身分啟動 OBS", BS_OWNERDRAW | WS_TABSTOP,
                        180, 192, 330, 28, AdminObs);

    control(window, L"STATIC", L"VTube Studio", 0, 52, 252, 125, 25);
    edits[1] = control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
                       180, 246, 610, 32, Vts);
    control(window, L"BUTTON", L"瀏覽", BS_OWNERDRAW | WS_TABSTOP, 802, 246, 86, 32, BrowseVts);
    checks[1] = control(window, L"BUTTON", L"透過 Steam 啟動", BS_OWNERDRAW | WS_GROUP | WS_TABSTOP,
                        180, 286, 220, 28, SteamMode);
    checks[2] = control(window, L"BUTTON", L"直接執行 exe", BS_OWNERDRAW | WS_TABSTOP,
                        420, 286, 210, 28, DirectMode);
    HWND hint = control(window, L"STATIC", L"Steam 模式需先登入 Steam；直接模式可能無法使用 VNet。",
                        0, 180, 324, 675, 25, VtsHintLabel);
    use_font(hint, small_font);

    HWND optional = control(window, L"STATIC", L"選用工具", 0, 50, 402, 300, 28, OptionalLabel);
    use_font(optional, section_font);
    checks[3] = control(window, L"BUTTON", L"onecomme", BS_OWNERDRAW | WS_TABSTOP,
                        52, 446, 116, 30, SyncOnecomme);
    edits[2] = control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
                       174, 445, 229, 32, Onecomme);
    control(window, L"BUTTON", L"瀏覽", BS_OWNERDRAW | WS_TABSTOP, 413, 445, 74, 32, BrowseOnecomme);
    checks[4] = control(window, L"BUTTON", L"Mix-it", BS_OWNERDRAW | WS_TABSTOP,
                        510, 446, 95, 30, SyncMixIt);
    edits[3] = control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
                       612, 445, 189, 32, MixIt);
    control(window, L"BUTTON", L"瀏覽", BS_OWNERDRAW | WS_TABSTOP, 812, 445, 76, 32, BrowseMixIt);
    checks[5] = control(window, L"BUTTON", L"RTMP 工具", BS_OWNERDRAW | WS_TABSTOP,
                        52, 494, 116, 30, SyncRtmp);
    edits[4] = control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
                       174, 493, 229, 32, Rtmp);
    control(window, L"BUTTON", L"瀏覽", BS_OWNERDRAW | WS_TABSTOP, 413, 493, 74, 32, BrowseRtmp);
    checks[6] = control(window, L"BUTTON", L"同步啟動 Twitch 伺服器測試", BS_OWNERDRAW | WS_TABSTOP,
                        510, 494, 350, 30, SyncTwitch);

    control(window, L"BUTTON", L"關於 / 設定", BS_OWNERDRAW | WS_TABSTOP, 500, 582, 136, 40, About);
    control(window, L"BUTTON", L"Twitch 測試", BS_OWNERDRAW | WS_TABSTOP, 646, 582, 122, 40, TwitchOnly);
    control(window, L"BUTTON", L"準備直播", BS_OWNERDRAW | WS_TABSTOP, 778, 582, 130, 40, Launch);
    to_ui(stream_launcher::load_settings());
    refresh_main_text(window);
    style_title_bar(window);
}

void update_picker(HWND window, bool update_hex) {
    picker_color = hsv_color(picker_hue, picker_sat, picker_value);
    if (update_hex) {
        picker_edit_updating = true;
        const std::wstring hex = color_hex(picker_color);
        SetWindowTextW(GetDlgItem(window, ColorHexEdit), hex.c_str());
        picker_edit_updating = false;
    }
    InvalidateRect(window, nullptr, FALSE);
    if (update_hex) InvalidateRect(GetDlgItem(window, ColorApply), nullptr, TRUE);
}

void paint_picker(HWND window, HDC dc) {
    RECT client{};
    GetClientRect(window, &client);
    FillRect(dc, &client, card_brush);
    BITMAPINFO bitmap{};
    bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth = 256;
    bitmap.bmiHeader.biHeight = -160;
    bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32;
    bitmap.bmiHeader.biCompression = BI_RGB;
    if (picker_rendered_hue != picker_hue) {
        for (int y = 0; y < 160; ++y)
            for (int x = 0; x < 256; ++x)
                picker_square_pixels[y * 256 + x] = stream_launcher::dib_pixel(
                    stream_launcher::hsv_rgb(picker_hue, x / 255.0, 1.0 - y / 159.0));
        picker_rendered_hue = picker_hue;
    }
    StretchDIBits(dc, 24, 42, 256, 160, 0, 0, 256, 160,
                  picker_square_pixels.data(), &bitmap, DIB_RGB_COLORS, SRCCOPY);
    bitmap.bmiHeader.biHeight = -18;
    if (!picker_hue_pixels_ready) {
        for (int y = 0; y < 18; ++y)
            for (int x = 0; x < 256; ++x)
                picker_hue_pixels[y * 256 + x] = stream_launcher::dib_pixel(
                    stream_launcher::hsv_rgb(x * 359.0 / 255.0, 1.0, 1.0));
        picker_hue_pixels_ready = true;
    }
    StretchDIBits(dc, 24, 224, 256, 18, 0, 0, 256, 18,
                  picker_hue_pixels.data(), &bitmap, DIB_RGB_COLORS, SRCCOPY);
    HPEN marker_outer = CreatePen(PS_SOLID, 2, RGB(17, 23, 33));
    HPEN marker_inner = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    HGDIOBJ old_pen = SelectObject(dc, marker_outer);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    const int marker_x = 24 + static_cast<int>(std::lround(picker_sat * 255));
    const int marker_y = 42 + static_cast<int>(std::lround((1.0 - picker_value) * 159));
    Ellipse(dc, marker_x - 8, marker_y - 8, marker_x + 8, marker_y + 8);
    SelectObject(dc, marker_inner);
    Ellipse(dc, marker_x - 6, marker_y - 6, marker_x + 6, marker_y + 6);
    const int hue_x = 24 + static_cast<int>(std::lround(picker_hue * 255 / 359.0));
    SelectObject(dc, marker_outer);
    RoundRect(dc, hue_x - 5, 219, hue_x + 6, 247, 5, 5);
    SelectObject(dc, marker_inner);
    RoundRect(dc, hue_x - 3, 221, hue_x + 4, 245, 4, 4);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(marker_outer);
    DeleteObject(marker_inner);
    HBRUSH preview = CreateSolidBrush(picker_color);
    HPEN outline = CreatePen(PS_SOLID, 1, border_color);
    old_brush = SelectObject(dc, preview);
    old_pen = SelectObject(dc, outline);
    RoundRect(dc, 302, 42, 386, 202, 14, 14);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(preview);
    DeleteObject(outline);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, text_color);
    HGDIOBJ old_font = SelectObject(dc, font);
    RECT label{24, 12, 385, 35};
    DrawTextW(dc, loc(L"拖曳選色，或輸入色碼", L"Drag to choose or enter a hex code"),
              -1, &label, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT hex_label{24, 259, 210, 280};
    DrawTextW(dc, loc(L"色碼", L"Hex color"), -1, &hex_label, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, old_font);
}

LRESULT CALLBACK color_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        picker_color = pending_accent_color;
        color_hsv(picker_color, picker_hue, picker_sat, picker_value);
        picker_edit_updating = true;
        const std::wstring hex = color_hex(picker_color);
        control(window, L"EDIT", hex.c_str(), WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
                24, 283, 170, 32, ColorHexEdit);
        picker_edit_updating = false;
        control(window, L"BUTTON", loc(L"取消", L"Cancel"), BS_OWNERDRAW | WS_TABSTOP,
                210, 330, 84, 36, ColorCancel);
        control(window, L"BUTTON", loc(L"選用", L"Use color"), BS_OWNERDRAW | WS_TABSTOP,
                302, 330, 84, 36, ColorApply);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;
        HDC buffer = CreateCompatibleDC(dc);
        HBITMAP surface = buffer ? CreateCompatibleBitmap(dc, width, height) : nullptr;
        if (buffer && surface) {
            HGDIOBJ old_surface = SelectObject(buffer, surface);
            paint_picker(window, buffer);
            BitBlt(dc, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
            SelectObject(buffer, old_surface);
        } else {
            paint_picker(window, dc);
        }
        if (surface) DeleteObject(surface);
        if (buffer) DeleteDC(buffer);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetBkColor(dc, input_color);
        SetTextColor(dc, text_color);
        return reinterpret_cast<LRESULT>(input_brush);
    }
    case WM_DRAWITEM:
        paint_button(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam));
        return TRUE;
    case WM_LBUTTONDOWN:
    case WM_MOUSEMOVE: {
        const int x = static_cast<short>(LOWORD(lparam));
        const int y = static_cast<short>(HIWORD(lparam));
        if (message == WM_LBUTTONDOWN) {
            picker_drag_square = x >= 24 && x <= 279 && y >= 42 && y <= 201;
            picker_drag_hue = x >= 24 && x <= 279 && y >= 224 && y <= 241;
            if (picker_drag_square || picker_drag_hue) SetCapture(window);
        }
        if (picker_drag_square) {
            picker_sat = std::clamp((x - 24) / 255.0, 0.0, 1.0);
            picker_value = 1.0 - std::clamp((y - 42) / 159.0, 0.0, 1.0);
            update_picker(window, false);
        } else if (picker_drag_hue) {
            picker_hue = std::clamp((x - 24) * 359.0 / 255.0, 0.0, 359.0);
            update_picker(window, false);
        }
        return 0;
    }
    case WM_LBUTTONUP:
        if (picker_drag_square || picker_drag_hue) update_picker(window, true);
        if (GetCapture() == window) ReleaseCapture();
        picker_drag_square = picker_drag_hue = false;
        return 0;
    case WM_CAPTURECHANGED:
        if (picker_drag_square || picker_drag_hue) update_picker(window, true);
        picker_drag_square = picker_drag_hue = false;
        return 0;
    case WM_COMMAND: {
        const int id = LOWORD(wparam);
        if (id == ColorHexEdit && HIWORD(wparam) == EN_CHANGE && !picker_edit_updating) {
            const std::wstring hex = text_of(GetDlgItem(window, ColorHexEdit));
            if (valid_color_hex(hex)) {
                picker_color = parse_color(hex);
                color_hsv(picker_color, picker_hue, picker_sat, picker_value);
                InvalidateRect(window, nullptr, FALSE);
                InvalidateRect(GetDlgItem(window, ColorApply), nullptr, TRUE);
            }
        } else if (id == ColorApply && HIWORD(wparam) == BN_CLICKED) {
            const std::wstring hex = text_of(GetDlgItem(window, ColorHexEdit));
            if (!valid_color_hex(hex)) {
                MessageBoxW(window, loc(L"色碼格式不對喔！\n請輸入 #RRGGBB 格式的色碼。",
                                        L"That color code doesn't look right.\nPlease use #RRGGBB format."),
                            loc(L"警告", L"Warning"), MB_OK | MB_ICONWARNING);
                return 0;
            }
            pending_accent_color = parse_color(hex);
            HWND owner = GetWindow(window, GW_OWNER);
            if (owner && IsWindow(owner)) {
                SetWindowTextW(GetDlgItem(owner, ColorPicker), color_hex(pending_accent_color).c_str());
                InvalidateRect(GetDlgItem(owner, ColorPicker), nullptr, TRUE);
            }
            DestroyWindow(window);
            if (owner && IsWindow(owner)) repaint_children(owner);
        } else if (id == ColorCancel && HIWORD(wparam) == BN_CLICKED) {
            DestroyWindow(window);
        }
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY: {
        HWND owner = GetWindow(window, GW_OWNER);
        if (owner && IsWindow(owner)) {
            EnableWindow(owner, TRUE);
            SetForegroundWindow(owner);
        }
        color_window = nullptr;
        return 0;
    }
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

void show_color_dialog(HWND owner) {
    if (color_window && IsWindow(color_window)) {
        SetForegroundWindow(color_window);
        return;
    }
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW info{};
        info.cbSize = sizeof(info);
        info.lpfnWndProc = color_proc;
        info.hInstance = instance;
        info.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
        info.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        info.lpszClassName = L"StreamLauncherColor";
        if (!RegisterClassExW(&info)) return;
        registered = true;
    }
    RECT parent{};
    GetWindowRect(owner, &parent);
    const int width = 420;
    const int height = 420;
    color_window = CreateWindowExW(0, L"StreamLauncherColor", loc(L"自訂主色", L"Accent color"),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        parent.left + ((parent.right - parent.left) - width) / 2,
        parent.top + ((parent.bottom - parent.top) - height) / 2,
        width, height, owner, nullptr, instance, nullptr);
    if (!color_window) return;
    style_title_bar(color_window);
    EnableWindow(owner, FALSE);
    ShowWindow(color_window, SW_SHOW);
    SetForegroundWindow(color_window);
}

LRESULT CALLBACK about_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        const std::wstring heading = std::wstring(L"Stream Launcher ") + stream_launcher::kVersion;
        HWND title = control(window, L"STATIC", heading.c_str(), 0, 18, 18, 450, 35);
        use_font(title, section_font);
        control(window, L"STATIC", L"© 2026 Joeeey0219.", 0, 18, 60, 450, 24);
        control(window, L"STATIC", loc(L"Twitch 伺服器測試軟體 - TwitchTest by R1ch (MIT License)",
                                         L"Twitch server test - TwitchTest by R1ch (MIT License)"), 0, 18, 99, 465, 24);
        control(window, L"STATIC", loc(L"軟體 icon 圖片 by 慕尹 Moon_yin",
                                         L"App icon artwork by 慕尹 Moon_yin"), 0, 18, 130, 465, 24);
        control(window, L"STATIC", L"", SS_ETCHEDHORZ, 18, 171, 460, 2);
        control(window, L"STATIC", loc(L"有訂製需求歡迎來電洽詢(*´∀`)~♥",
                                         L"For custom requests, feel free to contact me (*´∀`)~♥"), 0, 18, 192, 465, 26);
        control(window, L"STATIC", loc(L"聯繫方式：\r\nDiscord : joey0219\r\nGitHub : Joeeey0219",
                                         L"Contact:\r\nDiscord : joey0219\r\nGitHub : Joeeey0219"), 0, 18, 225, 450, 72);
        control(window, L"STATIC", L"", SS_ETCHEDHORZ, 18, 310, 460, 2);
        HWND theme_label = control(window, L"STATIC", loc(L"外觀主題：", L"Appearance:"), 0, 18, 337, 110, 27);
        use_font(theme_label, section_font);
        pending_theme_mode = theme_mode;
        control(window, L"BUTTON", loc(L"跟隨設備", L"System"), BS_OWNERDRAW | WS_TABSTOP,
                128, 329, 116, 36, ThemeSystem);
        control(window, L"BUTTON", loc(L"明亮", L"Light"), BS_OWNERDRAW | WS_TABSTOP,
                244, 329, 116, 36, ThemeLight);
        control(window, L"BUTTON", loc(L"深色", L"Dark"), BS_OWNERDRAW | WS_TABSTOP,
                360, 329, 116, 36, ThemeDark);
        pending_accent_color = accent_color;
        control(window, L"STATIC", loc(L"自訂主色：", L"Accent color:"), 0, 18, 385, 110, 26);
        const std::wstring swatch = color_hex(pending_accent_color);
        control(window, L"BUTTON", swatch.c_str(), BS_OWNERDRAW | WS_TABSTOP, 128, 377, 174, 36, ColorPicker);
        control(window, L"STATIC", loc(L"語言：", L"Language:"), 0, 18, 433, 110, 26);
        pending_english_ui = english_ui;
        control(window, L"BUTTON", L"繁體中文", BS_OWNERDRAW | WS_TABSTOP, 128, 425, 174, 36, LanguageZh);
        control(window, L"BUTTON", L"English", BS_OWNERDRAW | WS_TABSTOP, 302, 425, 174, 36, LanguageEn);
        HWND startup = control(window, L"BUTTON", loc(L"登入 Windows 時啟動啟動器", L"Launch launcher when signing in to Windows"),
                               BS_OWNERDRAW | WS_TABSTOP, 18, 491, 460, 30, AutoStart);
        set_checked(startup, stream_launcher::autostart_enabled());
        HWND hint = control(window, L"STATIC", loc(L"只開啟啟動器，不會自動啟動 OBS、VTS 或其他工具。",
                                                  L"Only opens the launcher; OBS, VTS and tools stay closed."),
                            0, 40, 523, 438, 26);
        use_font(hint, small_font);
        control(window, L"BUTTON", loc(L"檢查更新", L"Check updates"), BS_OWNERDRAW | WS_TABSTOP, 346, 563, 132, 32, UpdateCheck);
        control(window, L"BUTTON", loc(L"套用", L"Apply"), BS_OWNERDRAW | WS_TABSTOP, 346, 603, 132, 32, AboutApply);
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT area{};
        GetClientRect(window, &area);
        FillRect(reinterpret_cast<HDC>(wparam), &area, card_brush);
        return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetBkColor(dc, card_color);
        SetTextColor(dc, text_color);
        return reinterpret_cast<LRESULT>(card_brush);
    }
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetBkColor(dc, card_color);
        SetTextColor(dc, text_color);
        return reinterpret_cast<LRESULT>(card_brush);
    }
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetBkColor(dc, input_color);
        SetTextColor(dc, text_color);
        return reinterpret_cast<LRESULT>(input_brush);
    }
    case WM_DRAWITEM:
        paint_button(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam));
        return TRUE;
    case WM_COMMAND: {
        const int id = LOWORD(wparam);
        HWND owner = GetWindow(window, GW_OWNER);
        if (id == AboutApply) {
            const ThemeMode old_mode = theme_mode;
            const bool old_dark = dark_theme;
            const bool old_english = english_ui;
            const COLORREF old_accent = accent_color;
            theme_mode = pending_theme_mode;
            dark_theme = effective_dark(theme_mode);
            english_ui = pending_english_ui;
            accent_color = pending_accent_color;
            if (!owner || save(owner)) {
                DestroyWindow(window);
                set_palette(dark_theme);
                if (owner) {
                    refresh_main_text(owner);
                    style_title_bar(owner);
                    repaint_children(owner);
                }
            } else {
                theme_mode = old_mode;
                dark_theme = old_dark;
                english_ui = old_english;
                accent_color = old_accent;
            }
        } else if (id == ColorPicker) {
            show_color_dialog(window);
        } else if (id == UpdateCheck) {
            HWND button = GetDlgItem(window, UpdateCheck);
            SetWindowTextW(button, loc(L"檢查中...", L"Checking..."));
            UpdateWindow(button);
            const auto result = stream_launcher::check_for_updates(english_ui);
            SetWindowTextW(button, loc(L"檢查更新", L"Check updates"));
            if (result.status == stream_launcher::UpdateStatus::newer) {
                if (MessageBoxW(window, result.message.c_str(), loc(L"更新可用", L"Update available"), MB_YESNO | MB_ICONINFORMATION) == IDYES) {
                    ShellExecuteW(window, L"open", L"https://github.com/Joeeey0219/Stream-Software-Launcher/releases/latest",
                                  nullptr, nullptr, SW_SHOWNORMAL);
                }
            } else {
                const UINT icon = result.status == stream_launcher::UpdateStatus::error ? MB_ICONERROR : MB_ICONINFORMATION;
                const wchar_t* title = result.status == stream_launcher::UpdateStatus::error
                    ? loc(L"錯誤", L"Error")
                    : result.status == stream_launcher::UpdateStatus::current
                    ? loc(L"已是最新版本", L"Up to date")
                    : result.status == stream_launcher::UpdateStatus::development_ahead
                    ? loc(L"目前版本較新", L"Version ahead")
                    : loc(L"尚無公開版本", L"No public release");
                MessageBoxW(window, result.message.c_str(), title, MB_OK | icon);
            }
        } else if (id == AutoStart && HIWORD(wparam) == BN_CLICKED) {
            HWND item = GetDlgItem(window, AutoStart);
            const bool wanted = !checked(item);
            set_checked(item, wanted);
            std::wstring error;
            if (!stream_launcher::set_autostart(wanted, error, english_ui)) {
                set_checked(item, !wanted);
                const std::wstring detail = std::wstring(loc(L"開機啟動設定失敗：\n", L"Couldn't set startup:\n")) + error;
                MessageBoxW(window, detail.c_str(), loc(L"錯誤", L"Error"), MB_OK | MB_ICONERROR);
            }
        } else if (id >= ThemeSystem && id <= ThemeDark && HIWORD(wparam) == BN_CLICKED) {
            pending_theme_mode = id == ThemeSystem ? ThemeMode::system :
                                 id == ThemeLight ? ThemeMode::light : ThemeMode::dark;
            for (int choice = ThemeSystem; choice <= ThemeDark; ++choice)
                InvalidateRect(GetDlgItem(window, choice), nullptr, TRUE);
        } else if ((id == LanguageZh || id == LanguageEn) && HIWORD(wparam) == BN_CLICKED) {
            pending_english_ui = id == LanguageEn;
            InvalidateRect(GetDlgItem(window, LanguageZh), nullptr, TRUE);
            InvalidateRect(GetDlgItem(window, LanguageEn), nullptr, TRUE);
        }
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY: {
        HWND owner = GetWindow(window, GW_OWNER);
        if (owner && IsWindow(owner)) {
            EnableWindow(owner, TRUE);
            SetForegroundWindow(owner);
        }
        about_window = nullptr;
        return 0;
    }
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

void show_about(HWND owner) {
    if (about_window && IsWindow(about_window)) {
        SetForegroundWindow(about_window);
        return;
    }
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW info{};
        info.cbSize = sizeof(info);
        info.lpfnWndProc = about_proc;
        info.hInstance = instance;
        info.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
        info.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        info.hbrBackground = nullptr;
        info.lpszClassName = L"StreamLauncherAbout";
        if (!RegisterClassExW(&info)) return;
        registered = true;
    }
    RECT parent{};
    GetWindowRect(owner, &parent);
    const int width = 500;
    const int height = 690;
    const int x = parent.left + ((parent.right - parent.left) - width) / 2;
    const int y = parent.top + ((parent.bottom - parent.top) - height) / 2;
    about_window = CreateWindowExW(0, L"StreamLauncherAbout", loc(L"關於 / 設定", L"About / Settings"),
                                   WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                   x, y, width, height, owner, nullptr, instance, nullptr);
    if (!about_window) return;
    style_title_bar(about_window);
    EnableWindow(owner, FALSE);
    ShowWindow(about_window, SW_SHOW);
    SetForegroundWindow(about_window);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        try {
            make_ui(window);
        } catch (const std::exception&) {
            MessageBoxW(window, L"設定檔讀取失敗，請檢查 settings.ini 的 UTF-8 編碼喔！\nFailed to read settings.ini. Please check its UTF-8 encoding.",
                        L"錯誤 / Error", MB_ICONERROR);
            return -1;
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        if (theme_mode == ThemeMode::system) {
            const bool next = system_dark_theme();
            if (next != dark_theme) {
                dark_theme = next;
                set_palette(dark_theme);
                style_title_bar(window);
                repaint_children(window);
                if (about_window && IsWindow(about_window)) {
                    style_title_bar(about_window);
                    repaint_children(about_window);
                }
                if (color_window && IsWindow(color_window)) {
                    style_title_bar(color_window);
                    repaint_children(color_window);
                }
            }
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        FillRect(dc, &client, background_brush);
        paint_card(dc, 24, 108, 912, 367);
        paint_card(dc, 24, 385, 912, 555);
        HPEN line = CreatePen(PS_SOLID, 1, border_color);
        HGDIOBJ previous = SelectObject(dc, line);
        MoveToEx(dc, 50, 231, nullptr);
        LineTo(dc, 886, 231);
        MoveToEx(dc, 24, 568, nullptr);
        LineTo(dc, 912, 568);
        SelectObject(dc, previous);
        DeleteObject(line);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        const int id = GetDlgCtrlID(reinterpret_cast<HWND>(lparam));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, id == VersionLabel || id == VtsHintLabel
                              ? muted_color : text_color);
        return reinterpret_cast<LRESULT>(id == TitleLabel || id == VersionLabel
                                             ? background_brush : card_brush);
    }
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetBkColor(dc, card_color);
        SetTextColor(dc, text_color);
        return reinterpret_cast<LRESULT>(card_brush);
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetBkColor(dc, input_color);
        SetTextColor(dc, text_color);
        return reinterpret_cast<LRESULT>(input_brush);
    }
    case WM_DRAWITEM:
        paint_button(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam));
        return TRUE;
    case WM_COMMAND: {
        const int id = LOWORD(wparam);
        if (id >= BrowseObs && id <= BrowseRtmp) {
            if (browse(window, edits[id - BrowseObs])) save(window);
        }
        else if (id == SteamMode || id == DirectMode) {
            if (HIWORD(wparam) == BN_CLICKED) {
                set_checked(checks[1], id == SteamMode);
                set_checked(checks[2], id == DirectMode);
                EnableWindow(edits[1], id == DirectMode);
                EnableWindow(GetDlgItem(window, BrowseVts), id == DirectMode);
                save(window);
            }
        } else if (id >= AdminObs && id <= SyncTwitch && HIWORD(wparam) == BN_CLICKED) {
            HWND choice = GetDlgItem(window, id);
            set_checked(choice, !checked(choice));
            save(window);
        } else if (id >= Obs && id <= Rtmp && HIWORD(wparam) == EN_KILLFOCUS) {
            save(window);
        } else if (id == About) {
            show_about(window);
        } else if (id == TwitchOnly && save(window)) {
            const auto result = stream_launcher::launch_twitch_only(english_ui);
            if (!result.errors.empty()) {
                const std::wstring detail = std::wstring(loc(L"Twitch 測試軟體沒能啟動：\n", L"TwitchTest couldn't start:\n")) + join(result.errors);
                MessageBoxW(window, detail.c_str(), loc(L"錯誤", L"Error"), MB_OK | MB_ICONERROR);
            } else {
                MessageBoxW(window, loc(L"已送出 Twitch 測試軟體啟動指令！(*´∀`)~♥",
                                        L"TwitchTest launch requested! (*´∀`)~♥"),
                            loc(L"成功", L"Success"), MB_OK | MB_ICONINFORMATION);
            }
        } else if (id == Launch && save(window)) {
            const auto result = stream_launcher::launch_all(from_ui());
            if (!result.errors.empty()) {
                const bool only_missing_paths = result.missing_path && !result.launch_failure;
                std::wstring detail = std::wstring(only_missing_paths
                    ? loc(L"請先設定以下程式的路徑：\n", L"Please set the paths for these apps:\n")
                    : loc(L"有些程式沒能啟動：\n", L"Some apps couldn't start:\n")) + join(result.errors);
                if (!result.submitted.empty())
                    detail += loc(L"\n\n其他程式已送出啟動指令。", L"\n\nLaunch requests for the other apps were sent.");
                else if (only_missing_paths)
                    detail += loc(L"\n設定好再試一次喔！", L"\nSet them up and try again!");
                MessageBoxW(window, detail.c_str(), loc(only_missing_paths ? L"警告" : L"錯誤",
                                                            only_missing_paths ? L"Warning" : L"Error"),
                            MB_OK | (only_missing_paths ? MB_ICONWARNING : MB_ICONERROR));
            } else {
                const std::wstring detail = std::wstring(loc(
                    L"正在啟動已選取的程式！\n啟動速度會受電腦效能影響，可能要等一下喔！\n祝直播順利 (*´∀`)~♥\n\n已送出啟動指令：\n",
                    L"Starting your stream apps!\nIt may take a moment, depending on your computer.\nHave a great stream! (*´∀`)~♥\n\nLaunch requests sent for:\n")) +
                    join(result.submitted);
                MessageBoxW(window, detail.c_str(), loc(L"成功", L"Success"), MB_OK | MB_ICONINFORMATION);
            }
        }
        return 0;
    }
    case WM_CLOSE:
        if (save(window)) DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        DeleteObject(card_brush);
        DeleteObject(background_brush);
        DeleteObject(input_brush);
        DeleteObject(small_font);
        DeleteObject(section_font);
        DeleteObject(title_font);
        DeleteObject(font);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE application, HINSTANCE, PWSTR, int show) {
    instance = application;
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    window_class.hIconSm = window_class.hIcon;
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    window_class.lpszClassName = L"StreamLauncherWindow";
    if (!RegisterClassExW(&window_class)) return 1;
    const std::wstring title = std::wstring(L"Stream Launcher ") + stream_launcher::kVersion;
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT area{0, 0, 936, 640};
    AdjustWindowRect(&area, style, FALSE);
    const int width = area.right - area.left;
    const int height = area.bottom - area.top;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    HWND window = CreateWindowExW(0, window_class.lpszClassName, title.c_str(),
                                  style, x, y, width, height,
                                  nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    style_title_bar(window);
    ShowWindow(window, show);
    UpdateWindow(window);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
