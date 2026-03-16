#pragma once

#include "app_config.h"
#include <filesystem>
#include <megacitycode/renderer.h>
#include <optional>
#include <string>
#include <vector>

namespace megacitycode
{

struct RenderTestScenario
{
    std::string name;
    std::filesystem::path scenario_path;
    std::filesystem::path font_path;
    std::vector<std::string> fallback_paths;
    int width = 1280;
    int height = 800;
    int font_size = TextService::DEFAULT_POINT_SIZE;
    int timeout_ms = 5000;
    int settle_ms = 100;
    int pixel_tolerance = 8;
    double changed_pixels_threshold_pct = 0.1;

    std::filesystem::path reference_image_path() const;
    std::filesystem::path actual_image_path() const;
    std::filesystem::path diff_image_path() const;
    std::filesystem::path report_path() const;
    AppOptions make_app_options() const;
};

std::optional<RenderTestScenario> load_render_test_scenario(const std::filesystem::path& path, std::string* error_message = nullptr);
bool export_render_test_frame(const std::filesystem::path& path, const CapturedFrame& frame, std::string* error_message = nullptr);
bool finalize_render_test_result(const RenderTestScenario& scenario, const CapturedFrame& frame, bool bless_reference, std::string* error_message = nullptr);
void write_render_test_failure_report(const RenderTestScenario& scenario, std::string_view error_message);

} // namespace megacitycode
