#include "../src/launcher.hpp"
#include "../src/autostart.hpp"
#include "../src/update_check.hpp"
#include "../src/color_math.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <string>

using namespace stream_launcher;

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--settings-roundtrip") {
        Settings saved;
        saved.obs_path = L"C:\\直播工具\\obs.exe";
        saved.vts_steam = false;
        saved.theme_mode = ThemeMode::dark;
        saved.english = true;
        saved.accent_hex = L"#7A3FC1";
        std::wstring error;
        if (!save_settings(saved, error)) throw std::runtime_error("settings save failed");
        const Settings loaded = load_settings();
        if (loaded.obs_path != saved.obs_path || loaded.vts_steam != saved.vts_steam ||
            loaded.theme_mode != ThemeMode::dark || !loaded.english || loaded.accent_hex != saved.accent_hex)
            throw std::runtime_error("settings roundtrip failed");
        saved.theme_mode = ThemeMode::light;
        if (!save_settings(saved, error) || load_settings().theme_mode != ThemeMode::light)
            throw std::runtime_error("light theme roundtrip failed");
        saved.theme_mode = ThemeMode::system;
        if (!save_settings(saved, error) || load_settings().theme_mode != ThemeMode::system)
            throw std::runtime_error("system theme roundtrip failed");
        std::ofstream legacy(std::filesystem::path(settings_path()), std::ios::binary | std::ios::trunc);
        legacy << "teal_theme=true\n";
        legacy.close();
        const Settings legacy_loaded = load_settings();
        if (legacy_loaded.accent_hex != L"#00787C" || legacy_loaded.theme_mode != ThemeMode::system)
            throw std::runtime_error("legacy theme migration failed");
        std::ofstream legacy_theme(std::filesystem::path(settings_path()), std::ios::binary | std::ios::trunc);
        legacy_theme << "dark_theme=true\n";
        legacy_theme.close();
        if (load_settings().theme_mode != ThemeMode::dark) throw std::runtime_error("legacy dark theme migration failed");
        std::wcout << L"settings roundtrip passed\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--live-update-check") {
        const auto update = check_for_updates();
        std::wcout << L"update status=" << static_cast<int>(update.status)
                   << L" latest=" << update.latest_tag << L"\n";
        return update.status == UpdateStatus::error ? 1 : 0;
    }
    const auto require = [](bool condition) {
        if (!condition) throw std::runtime_error("launcher plan assertion failed");
    };
    const Rgb red = hsv_rgb(0.0, 1.0, 1.0);
    const Rgb green = hsv_rgb(120.0, 1.0, 1.0);
    const Rgb blue = hsv_rgb(240.0, 1.0, 1.0);
    require(red.r == 255 && red.g == 0 && red.b == 0 && dib_pixel(red) == 0x00FF0000);
    require(green.r == 0 && green.g == 255 && green.b == 0 && dib_pixel(green) == 0x0000FF00);
    require(blue.r == 0 && blue.g == 0 && blue.b == 255 && dib_pixel(blue) == 0x000000FF);
    Settings settings;
    settings.obs_path = L"C:\\Program Files\\obs-studio\\bin\\64bit\\obs64.exe";
    settings.vts_path = L"C:\\Steam\\VTube Studio.exe";

    auto jobs = build_plan(settings);
    require(jobs.size() == 2);
    require(jobs[1].is_steam_uri);
    require(jobs[1].target == kVtsSteamUri);
    require(jobs[1].target != settings.vts_path);

    settings.vts_steam = false;
    jobs = build_plan(settings);
    require(!jobs[1].is_steam_uri);
    require(jobs[1].target == settings.vts_path);

    settings.sync_onecomme = true;
    settings.sync_mix_it = true;
    settings.sync_rtmp = true;
    jobs = build_plan(settings);
    require(jobs.size() == 5);
    require(jobs[2].name == L"onecomme");
    require(jobs[3].name == L"Mix-it");
    require(jobs[4].name == L"RTMP 工具");

    require(autostart_command(L"C:\\Program Files\\Stream Launcher\\StreamLauncher.exe") ==
            L"\"C:\\Program Files\\Stream Launcher\\StreamLauncher.exe\"");

    Version version{};
    require(parse_version("0.1", version));
    require(version.major == 0 && version.minor == 1 && version.patch == 0);
    require(parse_version("V1.5.2", version));
    require(version.major == 1 && version.minor == 5 && version.patch == 2);
    require(!parse_version("0.1.", version));
    require(!parse_version("release-1.5.2", version));

    std::wcout << L"launcher plan tests passed\n";
    return 0;
}
