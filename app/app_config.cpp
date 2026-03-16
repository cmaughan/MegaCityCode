#include "app_config.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace spectre
{

namespace
{

constexpr int kMinWindowWidth = 640;
constexpr int kMinWindowHeight = 400;
constexpr int kMaxWindowWidth = 3840;
constexpr int kMaxWindowHeight = 2160;

std::filesystem::path config_path()
{
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    std::filesystem::path base = appdata ? appdata : ".";
    return base / "spectre" / "config.toml";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    std::filesystem::path base = home ? home : ".";
    return base / "Library" / "Application Support" / "spectre" / "config.toml";
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");
    std::filesystem::path base = xdg ? xdg : (home ? std::filesystem::path(home) / ".config" : std::filesystem::path("."));
    return base / "spectre" / "config.toml";
#endif
}

std::string trim(std::string value)
{
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string unquote(std::string value)
{
    value = trim(std::move(value));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        return value.substr(1, value.size() - 2);
    return value;
}

std::vector<std::string> parse_string_array(const std::string& value)
{
    std::vector<std::string> result;
    std::string trimmed = trim(value);
    if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']')
        return result;

    trimmed = trimmed.substr(1, trimmed.size() - 2);
    std::stringstream stream(trimmed);
    std::string item;
    while (std::getline(stream, item, ','))
    {
        item = trim(item);
        if (!item.empty())
            result.push_back(unquote(item));
    }
    return result;
}

int parse_window_dimension(const std::string& value, int fallback, int min_value, int max_value)
{
    try
    {
        int parsed = std::stoi(value);
        if (parsed < min_value || parsed > max_value)
            return fallback;
        return parsed;
    }
    catch (const std::exception&)
    {
        return fallback;
    }
}

int clamp_window_dimension(int value, int fallback, int min_value, int max_value)
{
    if (value < min_value || value > max_value)
        return fallback;
    return value;
}

int parse_font_size(const std::string& value, int fallback)
{
    try
    {
        return std::clamp(std::stoi(value), TextService::MIN_POINT_SIZE, TextService::MAX_POINT_SIZE);
    }
    catch (const std::exception&)
    {
        return fallback;
    }
}

} // namespace

AppConfig AppConfig::parse(std::string_view content)
{
    AppConfig config;
    std::istringstream in{std::string(content)};
    std::string line;
    while (std::getline(in, line))
    {
        auto hash = line.find('#');
        if (hash != std::string::npos)
            line.erase(hash);
        line = trim(line);
        if (line.empty())
            continue;

        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));

        if (key == "window_width")
            config.window_width = parse_window_dimension(value, config.window_width, kMinWindowWidth, kMaxWindowWidth);
        else if (key == "window_height")
            config.window_height = parse_window_dimension(value, config.window_height, kMinWindowHeight, kMaxWindowHeight);
        else if (key == "font_size")
            config.font_size = parse_font_size(value, config.font_size);
        else if (key == "font_path")
            config.font_path = unquote(value);
        else if (key == "fallback_paths")
            config.fallback_paths = parse_string_array(value);
    }

    return config;
}

std::string AppConfig::serialize() const
{
    std::ostringstream out;
    out << "window_width = " << clamp_window_dimension(window_width, AppConfig{}.window_width, kMinWindowWidth, kMaxWindowWidth) << "\n";
    out << "window_height = " << clamp_window_dimension(window_height, AppConfig{}.window_height, kMinWindowHeight, kMaxWindowHeight) << "\n";
    out << "font_size = " << std::clamp(font_size, TextService::MIN_POINT_SIZE, TextService::MAX_POINT_SIZE) << "\n";
    if (!font_path.empty())
        out << "font_path = \"" << font_path << "\"\n";
    if (!fallback_paths.empty())
    {
        out << "fallback_paths = [";
        for (size_t i = 0; i < fallback_paths.size(); i++)
        {
            if (i > 0)
                out << ", ";
            out << "\"" << fallback_paths[i] << "\"";
        }
        out << "]\n";
    }
    return out.str();
}

AppConfig AppConfig::load()
{
    auto path = config_path();
    if (!std::filesystem::exists(path))
        return {};

    std::ifstream in(path);
    std::string content{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>{}};
    return parse(content);
}

void AppConfig::save() const
{
    auto path = config_path();
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::trunc);
    out << serialize();
}

} // namespace spectre
