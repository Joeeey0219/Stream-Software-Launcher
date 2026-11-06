#pragma once

#include <string>

namespace stream_launcher {

struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
};

bool parse_version(const std::string& tag, Version& version);

enum class UpdateStatus { newer, current, development_ahead, no_release, error };

struct UpdateResult {
    UpdateStatus status = UpdateStatus::error;
    std::wstring latest_tag;
    std::wstring message;
};

UpdateResult check_for_updates(bool english = false);

}  // namespace stream_launcher
