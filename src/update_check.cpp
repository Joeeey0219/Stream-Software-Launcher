#include "update_check.hpp"
#include "launcher.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

#include <array>
#include <cctype>
#include <string>

namespace stream_launcher {
namespace {

constexpr wchar_t kHost[] = L"api.github.com";
constexpr wchar_t kPath[] = L"/repos/Joeeey0219/Stream-Software-Launcher/releases/latest";

struct InternetHandle {
    HINTERNET value = nullptr;
    ~InternetHandle() { if (value) WinHttpCloseHandle(value); }
};

std::wstring error_message(bool english, const wchar_t* zh, const wchar_t* en) {
    return std::wstring(english ? L"Update check failed: " : L"檢查更新失敗：") + (english ? en : zh);
}

std::string json_string(const std::string& json, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    size_t position = json.find(token);
    if (position == std::string::npos) return {};
    position = json.find(':', position + token.size());
    if (position == std::string::npos) return {};
    ++position;
    while (position < json.size() && std::isspace(static_cast<unsigned char>(json[position]))) ++position;
    if (position >= json.size() || json[position++] != '"') return {};
    std::string result;
    for (; position < json.size(); ++position) {
        const char character = json[position];
        if (character == '"') return result;
        if (character == '\\') {
            if (++position >= json.size()) return {};
            const char escaped = json[position];
            if (escaped == '"' || escaped == '\\' || escaped == '/') result += escaped;
            else return {};
        } else {
            result += character;
        }
    }
    return {};
}

std::wstring from_ascii(const std::string& text) {
    return std::wstring(text.begin(), text.end());
}

}  // namespace

bool parse_version(const std::string& tag, Version& version) {
    size_t position = tag.size() && (tag[0] == 'v' || tag[0] == 'V') ? 1 : 0;
    std::array<int, 3> parts{};
    for (size_t index = 0; index < parts.size(); ++index) {
        if (position >= tag.size() || !std::isdigit(static_cast<unsigned char>(tag[position]))) return false;
        while (position < tag.size() && std::isdigit(static_cast<unsigned char>(tag[position]))) {
            if (parts[index] > 1000000) return false;
            parts[index] = parts[index] * 10 + (tag[position++] - '0');
        }
        if (position == tag.size()) {
            if (index < 1) return false;
            break;
        }
        if (tag[position++] != '.' || index == 2) return false;
    }
    if (position != tag.size()) return false;
    version = {parts[0], parts[1], parts[2]};
    return true;
}

UpdateResult check_for_updates(bool english) {
    const std::wstring agent = std::wstring(L"StreamLauncher/") + kVersion;
    InternetHandle session{WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                       WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.value) return {UpdateStatus::error, {}, error_message(english, L"無法建立網路連線。", L"Could not open a network connection.")};
    WinHttpSetTimeouts(session.value, 5000, 5000, 5000, 5000);
    InternetHandle connection{WinHttpConnect(session.value, kHost, INTERNET_DEFAULT_HTTPS_PORT, 0)};
    if (!connection.value) return {UpdateStatus::error, {}, error_message(english, L"無法連接 GitHub。", L"Could not connect to GitHub.")};
    InternetHandle request{WinHttpOpenRequest(connection.value, L"GET", kPath, nullptr,
                                               WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                               WINHTTP_FLAG_SECURE)};
    if (!request.value) return {UpdateStatus::error, {}, error_message(english, L"無法建立更新請求。", L"Could not create an update request.")};
    const wchar_t headers[] = L"Accept: application/vnd.github+json\r\n";
    if (!WinHttpSendRequest(request.value, headers, static_cast<DWORD>(-1),
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.value, nullptr)) {
        return {UpdateStatus::error, {}, error_message(english, L"GitHub 請求失敗或逾時。", L"GitHub request failed or timed out.")};
    }
    DWORD status = 0;
    DWORD bytes = sizeof(status);
    if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &bytes, WINHTTP_NO_HEADER_INDEX)) {
        return {UpdateStatus::error, {}, error_message(english, L"無法讀取 GitHub 回應。", L"Could not read the GitHub response.")};
    }
    if (status == 404) return {UpdateStatus::no_release, {}, english ? L"There is no public GitHub release yet." : L"目前還沒有公開發布版本喔！"};
    if (status != 200) {
        const std::wstring detail = std::wstring(english ? L"GitHub returned HTTP " : L"GitHub 回應 HTTP ") + std::to_wstring(status);
        return {UpdateStatus::error, {}, std::wstring(english ? L"Update check failed: " : L"檢查更新失敗：") + detail};
    }
    std::string body;
    std::array<char, 4096> buffer{};
    for (;;) {
        DWORD received = 0;
        if (!WinHttpReadData(request.value, buffer.data(), static_cast<DWORD>(buffer.size()), &received)) {
            return {UpdateStatus::error, {}, error_message(english, L"無法讀取更新資料。", L"Could not read update data.")};
        }
        if (!received) break;
        if (body.size() + received > 1024 * 1024) {
            return {UpdateStatus::error, {}, error_message(english, L"更新資料過大。", L"Update data is too large.")};
        }
        body.append(buffer.data(), received);
    }
    const std::string tag = json_string(body, "tag_name");
    Version latest{};
    Version current{};
    std::string current_tag;
    for (const wchar_t* cursor = kVersion; *cursor; ++cursor) current_tag += static_cast<char>(*cursor);
    if (!parse_version(tag, latest) || !parse_version(current_tag, current)) {
        return {UpdateStatus::error, {}, error_message(english, L"版本標籤格式無法辨識。", L"Release tag format is not recognized.")};
    }
    const std::wstring latest_text = from_ascii(tag);
    const auto comparison = [](const Version& left, const Version& right) {
        if (left.major != right.major) return left.major < right.major ? -1 : 1;
        if (left.minor != right.minor) return left.minor < right.minor ? -1 : 1;
        if (left.patch != right.patch) return left.patch < right.patch ? -1 : 1;
        return 0;
    };
    const int order = comparison(current, latest);
    if (order < 0) return {UpdateStatus::newer, latest_text,
                          english ? L"New version " + latest_text + L" is available (current: " + std::wstring(kVersion) + L").\nOpen the download page?"
                                  : L"發現新版本 " + latest_text + L"（目前 " + std::wstring(kVersion) + L"）\n是否前往下載？"};
    if (order > 0) return {UpdateStatus::development_ahead, latest_text,
                          english ? L"You're using version " + std::wstring(kVersion) + L".\nLatest public version: " + latest_text + L"."
                                  : L"目前使用版本 " + std::wstring(kVersion) + L"。\n最新公開 Release 為 " + latest_text + L"，目前不用下載舊版喔！"};
    return {UpdateStatus::current, latest_text, english ? L"Version " + std::wstring(kVersion) + L" is up to date!"
                                                         : L"目前版本 " + std::wstring(kVersion) + L" 已是最新公開版本！"};
}

}  // namespace stream_launcher
