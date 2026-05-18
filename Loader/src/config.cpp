#include "loader.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace {

std::string read_file_utf8(const std::wstring& path) {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file) {
        return {};
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) {
        return {};
    }
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    if (!out.empty() && out.back() == L'\0') {
        out.pop_back();
    }
    return out;
}

std::string extract_json_string(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) {
        return {};
    }
    pos = json.find(':', pos);
    if (pos == std::string::npos) {
        return {};
    }
    pos = json.find('"', pos);
    if (pos == std::string::npos) {
        return {};
    }
    const size_t start = pos + 1;
    const size_t end = json.find('"', start);
    if (end == std::string::npos) {
        return {};
    }
    return json.substr(start, end - start);
}

bool extract_json_bool(const std::string& json, const std::string& key, bool default_value) {
    const std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) {
        return default_value;
    }
    pos = json.find(':', pos);
    if (pos == std::string::npos) {
        return default_value;
    }
    pos = json.find_first_not_of(" \t\r\n", pos + 1);
    if (pos == std::string::npos) {
        return default_value;
    }
    if (json.compare(pos, 4, "true") == 0) {
        return true;
    }
    if (json.compare(pos, 5, "false") == 0) {
        return false;
    }
    return default_value;
}

std::vector<std::string> extract_json_string_array(const std::string& json, const std::string& key) {
    std::vector<std::string> result;
    const std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) {
        return result;
    }
    pos = json.find('[', pos);
    if (pos == std::string::npos) {
        return result;
    }
    const size_t end = json.find(']', pos);
    if (end == std::string::npos) {
        return result;
    }
    const std::string slice = json.substr(pos, end - pos);
    size_t cursor = 0;
    while ((cursor = slice.find('"', cursor)) != std::string::npos) {
        const size_t start = cursor + 1;
        const size_t close = slice.find('"', start);
        if (close == std::string::npos) {
            break;
        }
        result.push_back(slice.substr(start, close - start));
        cursor = close + 1;
    }
    return result;
}

}  // namespace

namespace loader {

bool load_config(const std::wstring& config_path, Config& out) {
    const std::string json = read_file_utf8(config_path);
    if (json.empty()) {
        return false;
    }

    out.enabled = extract_json_bool(json, "enabled", true);

    const std::string mode = extract_json_string(json, "mode");
    if (!mode.empty()) {
        out.mode = utf8_to_wide(mode);
    }

    const std::string payload = extract_json_string(json, "payload");
    if (!payload.empty()) {
        out.payload = utf8_to_wide(payload);
    }

    const std::string injector = extract_json_string(json, "injector");
    if (!injector.empty()) {
        out.injector = utf8_to_wide(injector);
    }

    out.targets.clear();
    for (const auto& t : extract_json_string_array(json, "targets")) {
        out.targets.push_back(utf8_to_wide(t));
    }

    return true;
}

}  // namespace loader
