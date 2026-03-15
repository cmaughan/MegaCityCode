#pragma once

#include <spectre/text_service.h>
#include <string>

namespace spectre
{

struct AppConfig
{
    int window_width = 1280;
    int window_height = 800;
    int font_size = TextService::DEFAULT_POINT_SIZE;
    std::string font_path;
    std::vector<std::string> fallback_paths;

    static AppConfig load();
    void save() const;
};

} // namespace spectre
