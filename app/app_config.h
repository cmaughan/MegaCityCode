#pragma once

#include <spectre/text_service.h>
#include <string>
#include <string_view>
#include <vector>

namespace spectre
{

struct AppConfig
{
    int window_width = 1280;
    int window_height = 800;
    int font_size = TextService::DEFAULT_POINT_SIZE;
    std::string font_path;
    std::vector<std::string> fallback_paths;

    // Parse config from a string (TOML-like format). Returns defaults for any missing keys.
    static AppConfig parse(std::string_view content);
    // Serialize config to a string suitable for round-tripping through parse().
    std::string serialize() const;

    static AppConfig load();
    void save() const;
};

} // namespace spectre
