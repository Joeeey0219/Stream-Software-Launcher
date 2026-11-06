#include "launcher.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>

namespace stream_launcher {
namespace {

std::string to_utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (!size) throw std::runtime_error("UTF-8 conversion failed");
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring from_utf8(const std::string& value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (!size) throw std::runtime_error("Invalid UTF-8 settings file");
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

bool enabled(const std::map<std::string, std::string>& values, const char* key, bool default_value) {
    const auto found = values.find(key);
    return found == values.end() ? default_value : found->second == "true";
}

std::wstring value(const std::map<std::string, std::string>& values, const char* key) {
    const auto found = values.find(key);
    return found == values.end() ? L"" : from_utf8(found->second);
}

std::wstring last_error_text() {
    const DWORD code = GetLastError();
    wchar_t* buffer = nullptr;
    const DWORD count = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                        nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring message = count && buffer ? buffer : L"Unknown error";
    if (buffer) LocalFree(buffer);
    return message;
}

void launch_job(const Job& job) {
    if (job.target.empty()) throw std::runtime_error("missing path");
    std::wstring directory;
    if (!job.is_steam_uri) {
        const std::filesystem::path target(job.target);
        std::error_code error;
        if (!std::filesystem::is_regular_file(target, error) || _wcsicmp(target.extension().c_str(), L".exe") != 0) {
            throw std::runtime_error("executable not found");
        }
        directory = target.parent_path().wstring();
    }
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_FLAG_NO_UI;
    info.lpVerb = job.admin ? L"runas" : L"open";
    info.lpFile = job.target.c_str();
    info.lpDirectory = directory.empty() ? nullptr : directory.c_str();
    info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info)) throw std::runtime_error(to_utf8(last_error_text()));
}

}  // namespace

std::wstring module_directory() {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (!size) throw std::runtime_error("Cannot determine program directory");
        if (size < path.size()) {
            path.resize(size);
            return std::filesystem::path(path).parent_path().wstring();
        }
        path.resize(path.size() * 2);
    }
}

std::wstring settings_path() {
    return (std::filesystem::path(module_directory()) / L"settings.ini").wstring();
}

Settings load_settings() {
    Settings settings;
    std::ifstream file(std::filesystem::path(settings_path()), std::ios::binary);
    if (!file) return settings;
    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const auto equal = line.find('=');
        if (equal != std::string::npos) values[line.substr(0, equal)] = line.substr(equal + 1);
    }
    settings.obs_path = value(values, "obs_path");
    settings.vts_path = value(values, "vts_path");
    settings.onecomme_path = value(values, "onecomme_path");
    settings.mix_it_path = value(values, "mix_it_path");
    settings.rtmp_path = value(values, "rtmp_path");
    settings.admin_obs = enabled(values, "admin_obs", false);
    settings.vts_steam = enabled(values, "vts_steam", true);
    settings.sync_onecomme = enabled(values, "sync_onecomme", false);
    settings.sync_mix_it = enabled(values, "sync_mix_it", false);
    settings.sync_rtmp = enabled(values, "sync_rtmp", false);
    settings.sync_twitch = enabled(values, "sync_twitch", false);
    const auto theme = values.find("theme_mode");
    if (theme != values.end()) {
        if (theme->second == "light") settings.theme_mode = ThemeMode::light;
        else if (theme->second == "dark") settings.theme_mode = ThemeMode::dark;
    } else if (values.find("dark_theme") != values.end()) {
        settings.theme_mode = enabled(values, "dark_theme", false) ? ThemeMode::dark : ThemeMode::light;
    }
    settings.english = enabled(values, "english", false);
    const std::wstring accent = value(values, "accent_color");
    if (accent.size() == 7 && accent[0] == L'#' &&
        std::all_of(accent.begin() + 1, accent.end(), [](wchar_t c) { return iswxdigit(c) != 0; })) {
        settings.accent_hex = accent;
    } else if (enabled(values, "teal_theme", false)) {
        settings.accent_hex = L"#00787C";
    }
    return settings;
}

bool save_settings(const Settings& settings, std::wstring& error) {
    try {
        std::ofstream file(std::filesystem::path(settings_path()), std::ios::binary | std::ios::trunc);
        if (!file) {
            error = settings.english ? L"Could not open settings.ini. Make sure the app folder is writable."
                                     : L"無法開啟設定檔；請確認程式所在資料夾可寫入。";
            return false;
        }
        file << "# Stream Launcher " << to_utf8(kVersion) << "\n";
        file << "obs_path=" << to_utf8(settings.obs_path) << "\n";
        file << "vts_path=" << to_utf8(settings.vts_path) << "\n";
        file << "onecomme_path=" << to_utf8(settings.onecomme_path) << "\n";
        file << "mix_it_path=" << to_utf8(settings.mix_it_path) << "\n";
        file << "rtmp_path=" << to_utf8(settings.rtmp_path) << "\n";
        file << "admin_obs=" << (settings.admin_obs ? "true" : "false") << "\n";
        file << "vts_steam=" << (settings.vts_steam ? "true" : "false") << "\n";
        file << "sync_onecomme=" << (settings.sync_onecomme ? "true" : "false") << "\n";
        file << "sync_mix_it=" << (settings.sync_mix_it ? "true" : "false") << "\n";
        file << "sync_rtmp=" << (settings.sync_rtmp ? "true" : "false") << "\n";
        file << "sync_twitch=" << (settings.sync_twitch ? "true" : "false") << "\n";
        file << "theme_mode=" << (settings.theme_mode == ThemeMode::dark ? "dark" :
                                    settings.theme_mode == ThemeMode::light ? "light" : "system") << "\n";
        file << "english=" << (settings.english ? "true" : "false") << "\n";
        file << "accent_color=" << to_utf8(settings.accent_hex) << "\n";
        file.close();
        if (!file) {
            error = settings.english ? L"Could not write settings.ini." : L"寫入設定檔失敗。";
            return false;
        }
        return true;
    } catch (const std::exception&) {
        error = settings.english ? L"Settings contain characters that could not be saved."
                                 : L"設定檔包含無法儲存的字元。";
        return false;
    }
}

std::vector<Job> build_plan(const Settings& settings) {
    std::vector<Job> jobs{
        {L"OBS", settings.obs_path, false, settings.admin_obs},
        {L"VTube Studio", settings.vts_steam ? kVtsSteamUri : settings.vts_path, settings.vts_steam, false},
    };
    if (settings.sync_onecomme) jobs.push_back({L"onecomme", settings.onecomme_path});
    if (settings.sync_mix_it) jobs.push_back({L"Mix-it", settings.mix_it_path});
    if (settings.sync_rtmp) jobs.push_back({settings.english ? L"RTMP tool" : L"RTMP 工具", settings.rtmp_path});
    if (settings.sync_twitch) jobs.push_back({L"TwitchTest", (std::filesystem::path(module_directory()) / L"twitchtest-2.0" / L"TwitchTest.exe").wstring(), false, true});
    return jobs;
}

Result launch_all(const Settings& settings) {
    Result result;
    for (const Job& job : build_plan(settings)) {
        try {
            launch_job(job);
            result.submitted.push_back(job.name);
        } catch (const std::exception& exc) {
            if (job.target.empty()) {
                result.missing_path = true;
                result.errors.push_back(job.name + (settings.english ? L": path is not set" : L"：路徑未設定"));
            } else {
                result.launch_failure = true;
                result.errors.push_back(job.name + (settings.english ? L": failed to launch (" : L"：啟動失敗（") + from_utf8(exc.what()) + (settings.english ? L")" : L"）"));
            }
        }
    }
    return result;
}

Result launch_twitch_only(bool english) {
    Result result;
    const Job job{L"TwitchTest", (std::filesystem::path(module_directory()) / L"twitchtest-2.0" / L"TwitchTest.exe").wstring(), false, true};
    try {
        launch_job(job);
        result.submitted.push_back(job.name);
    } catch (const std::exception& exc) {
        result.launch_failure = true;
        result.errors.push_back(job.name + (english ? L": failed to launch (" : L"：啟動失敗（") + from_utf8(exc.what()) + (english ? L")" : L"）"));
    }
    return result;
}

}  // namespace stream_launcher
