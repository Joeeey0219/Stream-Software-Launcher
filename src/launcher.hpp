#pragma once

#include <string>
#include <vector>

namespace stream_launcher {

inline constexpr wchar_t kVersion[] = L"V1.5.2";
inline constexpr wchar_t kVtsSteamUri[] = L"steam://rungameid/1325860";

enum class ThemeMode { system, light, dark };

struct Settings {
    std::wstring obs_path;
    std::wstring vts_path;
    std::wstring onecomme_path;
    std::wstring mix_it_path;
    std::wstring rtmp_path;
    bool admin_obs = false;
    bool vts_steam = true;
    bool sync_onecomme = false;
    bool sync_mix_it = false;
    bool sync_rtmp = false;
    bool sync_twitch = false;
    ThemeMode theme_mode = ThemeMode::system;
    bool english = false;
    std::wstring accent_hex = L"#2C5DCC";
};

struct Job {
    std::wstring name;
    std::wstring target;
    bool is_steam_uri = false;
    bool admin = false;
};

struct Result {
    std::vector<std::wstring> submitted;
    std::vector<std::wstring> errors;
    bool missing_path = false;
    bool launch_failure = false;
};

std::wstring module_directory();
std::wstring settings_path();
Settings load_settings();
bool save_settings(const Settings& settings, std::wstring& error);
std::vector<Job> build_plan(const Settings& settings);
Result launch_all(const Settings& settings);
Result launch_twitch_only(bool english = false);

}  // namespace stream_launcher
