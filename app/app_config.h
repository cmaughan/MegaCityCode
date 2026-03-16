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

struct AppOptions
{
    bool load_user_config = true;
    bool save_user_config = true;
    bool activate_window_on_startup = true;
    bool show_debug_overlay_on_startup = false;
    bool clamp_window_to_display = true;
    AppConfig initial_config = {};
    std::vector<std::string> nvim_args;
    std::vector<std::string> startup_commands;
    std::string nvim_working_dir;
};

} // namespace spectre
