#include "autostart.hpp"
#include "launcher.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <vector>

namespace stream_launcher {
namespace {

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"StreamLauncher";

std::wstring executable_path() {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (!length) return {};
        if (length < path.size()) {
            path.resize(length);
            return path;
        }
        path.resize(path.size() * 2);
    }
}

std::wstring registry_error(bool english, const wchar_t* zh, const wchar_t* en, LONG code) {
    return english ? std::wstring(en) + L" failed (Windows error " + std::to_wstring(code) + L")"
                   : std::wstring(zh) + L"失敗（Windows 錯誤碼 " + std::to_wstring(code) + L"）";
}

}  // namespace

std::wstring autostart_command(const std::wstring& path) {
    return L"\"" + path + L"\"";
}

bool autostart_enabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    DWORD type = 0;
    DWORD bytes = 0;
    LONG status = RegQueryValueExW(key, kValueName, nullptr, &type, nullptr, &bytes);
    if (status != ERROR_SUCCESS || type != REG_SZ || bytes == 0) {
        RegCloseKey(key);
        return false;
    }
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1, L'\0');
    status = RegQueryValueExW(key, kValueName, nullptr, &type,
                              reinterpret_cast<LPBYTE>(buffer.data()), &bytes);
    RegCloseKey(key);
    return status == ERROR_SUCCESS && autostart_command(executable_path()) == buffer.data();
}

bool set_autostart(bool enabled, std::wstring& error, bool english) {
    HKEY key = nullptr;
    LONG status = enabled
        ? RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr)
        : RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key);
    if (!enabled && status == ERROR_FILE_NOT_FOUND) return true;
    if (status != ERROR_SUCCESS) {
        error = registry_error(english, L"開啟開機啟動設定", L"Opening startup settings", status);
        return false;
    }
    if (enabled) {
        const std::wstring command = autostart_command(executable_path());
        status = command == L"\"\"" ? ERROR_FILE_NOT_FOUND
                 : RegSetValueExW(key, kValueName, 0, REG_SZ,
                                  reinterpret_cast<const BYTE*>(command.c_str()),
                                  static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        status = RegDeleteValueW(key, kValueName);
        if (status == ERROR_FILE_NOT_FOUND) status = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        error = registry_error(english, L"更新開機啟動設定", L"Updating startup settings", status);
        return false;
    }
    return true;
}

}  // namespace stream_launcher
