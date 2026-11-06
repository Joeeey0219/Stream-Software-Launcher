#pragma once

#include <string>

namespace stream_launcher {

std::wstring autostart_command(const std::wstring& executable_path);
bool autostart_enabled();
bool set_autostart(bool enabled, std::wstring& error, bool english = false);

}  // namespace stream_launcher
