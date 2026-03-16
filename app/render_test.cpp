#include "render_test.h"
#include "toml_util.h"

#include <spectre/log.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <sstream>
#include <string_view>
#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#endif

namespace spectre
{

namespace
{

using namespace toml;

constexpr int kBmpHeaderSize = 14;
constexpr int kBmpInfoSize = 40;
constexpr uint32_t kBmpMagic = 0x4D42;

std::string platform_suffix()
{
#ifdef _WIN32
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}

std::string normalized_path_string(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

std::string expand_placeholders(std::string value, const std::filesystem::path& scenario_dir)
{
    const std::array replacements = {
        std::pair<std::string_view, std::string>{ "${SCENARIO_DIR}", normalized_path_string(scenario_dir) },
        std::pair<std::string_view, std::string>{ "${PROJECT_ROOT}", normalized_path_string(scenario_dir.parent_path().parent_path()) },
    };

    for (const auto& [needle, replacement] : replacements)
    {
        size_t pos = 0;
        while ((pos = value.find(needle, pos)) != std::string::npos)
        {
            value.replace(pos, needle.size(), replacement);
            pos += replacement.size();
        }
    }
    return value;
}

void append_u16(std::vector<uint8_t>& out, uint16_t value)
{
    out.push_back(static_cast<uint8_t>(value & 0xff));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
}

void append_u32(std::vector<uint8_t>& out, uint32_t value)
{
    out.push_back(static_cast<uint8_t>(value & 0xff));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
}

void append_i32(std::vector<uint8_t>& out, int32_t value)
{
    append_u32(out, static_cast<uint32_t>(value));
}

bool write_bmp_rgba(const std::filesystem::path& path, const CapturedFrame& frame)
{
    if (!frame.valid())
        return false;

    std::filesystem::create_directories(path.parent_path());

    const uint32_t image_size = static_cast<uint32_t>(frame.width * frame.height * 4);
    const uint32_t file_size = kBmpHeaderSize + kBmpInfoSize + image_size;

    std::vector<uint8_t> bytes;
    bytes.reserve(file_size);

    append_u16(bytes, static_cast<uint16_t>(kBmpMagic));
    append_u32(bytes, file_size);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, kBmpHeaderSize + kBmpInfoSize);

    append_u32(bytes, kBmpInfoSize);
    append_i32(bytes, frame.width);
    append_i32(bytes, -frame.height);
    append_u16(bytes, 1);
    append_u16(bytes, 32);
    append_u32(bytes, 0);
    append_u32(bytes, image_size);
    append_i32(bytes, 0);
    append_i32(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, 0);

    for (size_t i = 0; i < frame.rgba.size(); i += 4)
    {
        bytes.push_back(frame.rgba[i + 2]);
        bytes.push_back(frame.rgba[i + 1]);
        bytes.push_back(frame.rgba[i + 0]);
        bytes.push_back(frame.rgba[i + 3]);
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return out.good();
}

std::optional<CapturedFrame> read_bmp_rgba(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;

    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() < kBmpHeaderSize + kBmpInfoSize)
        return std::nullopt;

    auto read_u16 = [&](size_t offset) -> uint16_t {
        return static_cast<uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
    };
    auto read_u32 = [&](size_t offset) -> uint32_t {
        return static_cast<uint32_t>(bytes[offset] | (bytes[offset + 1] << 8) | (bytes[offset + 2] << 16) | (bytes[offset + 3] << 24));
    };
    auto read_i32 = [&](size_t offset) -> int32_t {
        return static_cast<int32_t>(read_u32(offset));
    };

    if (read_u16(0) != kBmpMagic)
        return std::nullopt;

    const uint32_t pixel_offset = read_u32(10);
    const int32_t width = read_i32(18);
    const int32_t height = read_i32(22);
    const uint16_t planes = read_u16(26);
    const uint16_t bpp = read_u16(28);
    const uint32_t compression = read_u32(30);

    if (planes != 1 || bpp != 32 || compression != 0 || width <= 0 || height == 0)
        return std::nullopt;

    const bool top_down = height < 0;
    const int abs_height = std::abs(height);
    const size_t expected_size = static_cast<size_t>(width) * abs_height * 4;
    if (bytes.size() < pixel_offset + expected_size)
        return std::nullopt;

    CapturedFrame frame;
    frame.width = width;
    frame.height = abs_height;
    frame.rgba.resize(expected_size);

    for (int y = 0; y < abs_height; ++y)
    {
        const int src_y = top_down ? y : (abs_height - 1 - y);
        const size_t src_row = pixel_offset + static_cast<size_t>(src_y * width * 4);
        const size_t dst_row = static_cast<size_t>(y * width * 4);
        for (int x = 0; x < width; ++x)
        {
            const size_t src = src_row + static_cast<size_t>(x * 4);
            const size_t dst = dst_row + static_cast<size_t>(x * 4);
            frame.rgba[dst + 0] = bytes[src + 2];
            frame.rgba[dst + 1] = bytes[src + 1];
            frame.rgba[dst + 2] = bytes[src + 0];
            frame.rgba[dst + 3] = bytes[src + 3];
        }
    }

    return frame;
}

struct RenderDiff
{
    bool passed = false;
    size_t changed_pixels = 0;
    double changed_pixels_pct = 0.0;
    double mean_abs_channel_diff = 0.0;
    uint8_t max_channel_diff = 0;
    CapturedFrame diff_image;
};

RenderDiff compare_frames(const CapturedFrame& actual, const CapturedFrame& reference, int tolerance, double threshold_pct)
{
    RenderDiff diff;
    diff.diff_image.width = actual.width;
    diff.diff_image.height = actual.height;
    diff.diff_image.rgba.resize(actual.rgba.size(), 0);

    if (!actual.valid() || !reference.valid() || actual.width != reference.width || actual.height != reference.height)
        return diff;

    uint64_t diff_sum = 0;
    for (size_t i = 0; i < actual.rgba.size(); i += 4)
    {
        const uint8_t dr = static_cast<uint8_t>(std::abs(int(actual.rgba[i + 0]) - int(reference.rgba[i + 0])));
        const uint8_t dg = static_cast<uint8_t>(std::abs(int(actual.rgba[i + 1]) - int(reference.rgba[i + 1])));
        const uint8_t db = static_cast<uint8_t>(std::abs(int(actual.rgba[i + 2]) - int(reference.rgba[i + 2])));
        const uint8_t da = static_cast<uint8_t>(std::abs(int(actual.rgba[i + 3]) - int(reference.rgba[i + 3])));
        const uint8_t max_delta = std::max({ dr, dg, db, da });

        if (max_delta > tolerance)
            ++diff.changed_pixels;

        diff.max_channel_diff = std::max(diff.max_channel_diff, max_delta);
        diff_sum += static_cast<uint64_t>(dr) + dg + db + da;

        diff.diff_image.rgba[i + 0] = dr;
        diff.diff_image.rgba[i + 1] = dg;
        diff.diff_image.rgba[i + 2] = db;
        diff.diff_image.rgba[i + 3] = 255;
    }

    const double total_pixels = static_cast<double>(actual.width) * actual.height;
    diff.changed_pixels_pct = total_pixels > 0.0 ? (static_cast<double>(diff.changed_pixels) * 100.0) / total_pixels : 0.0;
    diff.mean_abs_channel_diff = actual.rgba.empty() ? 0.0 : static_cast<double>(diff_sum) / static_cast<double>(actual.rgba.size());
    diff.passed = diff.changed_pixels_pct <= threshold_pct;
    return diff;
}

bool write_report(const std::filesystem::path& path, const RenderTestScenario& scenario, const CapturedFrame& actual,
    const std::optional<CapturedFrame>& reference, const RenderDiff* diff, bool blessed)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::trunc);
    if (!out)
        return false;

    out << "{\n";
    out << "  \"name\": \"" << json_escape_string(scenario.name) << "\",\n";
    out << "  \"platform\": \"" << platform_suffix() << "\",\n";
    out << "  \"width\": " << actual.width << ",\n";
    out << "  \"height\": " << actual.height << ",\n";
    out << "  \"pixel_tolerance\": " << scenario.pixel_tolerance << ",\n";
    out << "  \"changed_pixels_threshold_pct\": " << scenario.changed_pixels_threshold_pct << ",\n";
    out << "  \"blessed\": " << (blessed ? "true" : "false");
    if (reference && diff)
    {
        out << ",\n";
        out << "  \"changed_pixels\": " << diff->changed_pixels << ",\n";
        out << "  \"changed_pixels_pct\": " << diff->changed_pixels_pct << ",\n";
        out << "  \"mean_abs_channel_diff\": " << diff->mean_abs_channel_diff << ",\n";
        out << "  \"max_channel_diff\": " << static_cast<int>(diff->max_channel_diff) << ",\n";
        out << "  \"passed\": " << (diff->passed ? "true" : "false") << '\n';

        SPECTRE_LOG_INFO(spectre::LogCategory::Test,
            "[%s] diff: %.4f%% changed pixels (%zu/%d), max_channel_delta=%d, mean_abs=%.4f [%s]",
            scenario.name.c_str(), diff->changed_pixels_pct, diff->changed_pixels, actual.width * actual.height,
            static_cast<int>(diff->max_channel_diff), diff->mean_abs_channel_diff, diff->passed ? "PASS" : "FAIL");
    }
    else
    {
        out << '\n';
    }
    out << "}\n";
    return out.good();
}

void write_failure_report(const std::filesystem::path& path, const RenderTestScenario& scenario, std::string_view error_message)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::trunc);
    if (!out)
        return;

    out << "{\n";
    out << "  \"name\": \"" << json_escape_string(scenario.name) << "\",\n";
    out << "  \"platform\": \"" << platform_suffix() << "\",\n";
    out << "  \"error\": \"" << json_escape_string(error_message) << "\"\n";
    out << "}\n";

    SPECTRE_LOG_ERROR(spectre::LogCategory::Test, "[%s] %.*s", scenario.name.c_str(),
        static_cast<int>(error_message.size()), error_message.data());
}

std::filesystem::path default_output_path(const std::filesystem::path& scenario_path, std::string_view stem_suffix, std::string_view extension)
{
    const auto scenario_dir = scenario_path.parent_path();
    const auto stem = scenario_path.stem().string();
    return scenario_dir / "out" / (stem + "." + platform_suffix() + std::string(stem_suffix) + std::string(extension));
}

} // namespace

std::filesystem::path RenderTestScenario::reference_image_path() const
{
    return scenario_path.parent_path() / "reference" / (scenario_path.stem().string() + "." + platform_suffix() + ".bmp");
}

std::filesystem::path RenderTestScenario::actual_image_path() const
{
    return default_output_path(scenario_path, ".actual", ".bmp");
}

std::filesystem::path RenderTestScenario::diff_image_path() const
{
    return default_output_path(scenario_path, ".diff", ".bmp");
}

std::filesystem::path RenderTestScenario::report_path() const
{
    return default_output_path(scenario_path, ".report", ".json");
}

AppOptions RenderTestScenario::make_app_options() const
{
    // Scenario dimensions are physical pixels (consistent at 96 DPI / scale 1.0 on Windows).
    // On HiDPI displays SDL_CreateWindow takes logical pixels, so divide by the backing
    // scale so SDL_WINDOW_HIGH_PIXEL_DENSITY produces the intended physical resolution.
    float display_scale = 1.0f;
#ifdef __APPLE__
    {
        CGDirectDisplayID display_id = CGMainDisplayID();
        size_t logical_w = CGDisplayPixelsWide(display_id);
        CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display_id);
        if (mode)
        {
            size_t physical_w = CGDisplayModeGetPixelWidth(mode);
            CGDisplayModeRelease(mode);
            if (logical_w > 0)
                display_scale = static_cast<float>(physical_w) / static_cast<float>(logical_w);
        }
    }
#endif

    AppOptions options;
    options.load_user_config = false;
    options.save_user_config = false;
    options.activate_window_on_startup = false;
    options.show_debug_overlay_on_startup = debug_overlay;
    options.clamp_window_to_display = false;
    options.initial_config.window_width = static_cast<int>(std::round(width / display_scale));
    options.initial_config.window_height = static_cast<int>(std::round(height / display_scale));
    options.initial_config.font_size = font_size;
    options.initial_config.font_path = font_path.empty() ? std::string{} : normalized_path_string(font_path);
    options.initial_config.fallback_paths = fallback_paths;
    options.nvim_args = nvim_args;
    options.startup_commands = commands;
    return options;
}

std::optional<RenderTestScenario> load_render_test_scenario(const std::filesystem::path& path, std::string* error_message)
{
    std::ifstream in(path);
    if (!in)
    {
        if (error_message)
            *error_message = "Unable to open render test scenario";
        return std::nullopt;
    }

    RenderTestScenario scenario;
    scenario.scenario_path = std::filesystem::absolute(path).lexically_normal();
    scenario.name = scenario.scenario_path.stem().string();
    const auto scenario_dir = scenario.scenario_path.parent_path();

    std::string line;
    while (std::getline(in, line))
    {
        auto hash = line.find('#');
        if (hash != std::string::npos)
            line.erase(hash);
        line = trim(line);
        if (line.empty())
            continue;

        const auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        const std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));

        if (!value.empty() && value.front() == '[' && !is_complete_array_literal(value))
        {
            std::string continuation;
            while (std::getline(in, continuation))
            {
                auto continuation_hash = continuation.find('#');
                if (continuation_hash != std::string::npos)
                    continuation.erase(continuation_hash);
                continuation = trim(continuation);
                if (continuation.empty())
                    continue;

                if (!value.empty())
                    value.push_back(' ');
                value += continuation;

                if (is_complete_array_literal(value))
                    break;
            }
        }

        if (key == "name")
            scenario.name = unquote(value);
        else if (key == "width")
            scenario.width = parse_int(value, scenario.width);
        else if (key == "height")
            scenario.height = parse_int(value, scenario.height);
        else if (key == "font_size")
            scenario.font_size = parse_int(value, scenario.font_size);
        else if (key == "timeout_ms")
            scenario.timeout_ms = parse_int(value, scenario.timeout_ms);
        else if (key == "settle_ms")
            scenario.settle_ms = parse_int(value, scenario.settle_ms);
        else if (key == "pixel_tolerance")
            scenario.pixel_tolerance = parse_int(value, scenario.pixel_tolerance);
        else if (key == "changed_pixels_threshold_pct")
            scenario.changed_pixels_threshold_pct = parse_double(value, scenario.changed_pixels_threshold_pct);
        else if (key == "debug_overlay")
            scenario.debug_overlay = parse_bool(value, scenario.debug_overlay);
        else if (key == "font_path")
        {
            const auto expanded = expand_placeholders(unquote(value), scenario_dir);
            scenario.font_path = std::filesystem::path(expanded);
        }
        else if (key == "fallback_paths")
        {
            auto parsed = parse_string_array(value);
            scenario.fallback_paths.clear();
            for (auto& entry : parsed)
                scenario.fallback_paths.push_back(expand_placeholders(entry, scenario_dir));
        }
        else if (key == "nvim_args")
        {
            scenario.nvim_args.clear();
            for (auto& entry : parse_string_array(value))
                scenario.nvim_args.push_back(expand_placeholders(entry, scenario_dir));
        }
        else if (key == "commands")
        {
            scenario.commands.clear();
            for (auto& entry : parse_string_array(value))
                scenario.commands.push_back(expand_placeholders(entry, scenario_dir));
        }
    }

    scenario.width = std::clamp(scenario.width, 320, 3840);
    scenario.height = std::clamp(scenario.height, 240, 2160);
    scenario.font_size = std::clamp(scenario.font_size, TextService::MIN_POINT_SIZE, TextService::MAX_POINT_SIZE);
    scenario.timeout_ms = std::clamp(scenario.timeout_ms, 1000, 30000);
    scenario.settle_ms = std::clamp(scenario.settle_ms, 10, 5000);
    scenario.pixel_tolerance = std::clamp(scenario.pixel_tolerance, 0, 255);
    scenario.changed_pixels_threshold_pct = std::max(0.0, scenario.changed_pixels_threshold_pct);

    if (scenario.commands.empty())
    {
        if (error_message)
            *error_message = "Render test scenario requires at least one startup command";
        return std::nullopt;
    }

    return scenario;
}

bool finalize_render_test_result(const RenderTestScenario& scenario, const CapturedFrame& frame, bool bless_reference, std::string* error_message)
{
    if (!frame.valid())
    {
        if (error_message)
            *error_message = "Captured frame is empty";
        return false;
    }

    if (!write_bmp_rgba(scenario.actual_image_path(), frame))
    {
        if (error_message)
            *error_message = "Failed to write actual capture image";
        return false;
    }

    if (bless_reference)
    {
        if (!write_bmp_rgba(scenario.reference_image_path(), frame))
        {
            if (error_message)
                *error_message = "Failed to write blessed reference image";
            return false;
        }
        write_report(scenario.report_path(), scenario, frame, std::nullopt, nullptr, true);
        return true;
    }

    auto reference = read_bmp_rgba(scenario.reference_image_path());
    if (!reference)
    {
        if (error_message)
            *error_message = "Reference image not found; rerun with --bless-render-test";
        return false;
    }

    if (reference->width != frame.width || reference->height != frame.height)
    {
        if (error_message)
            *error_message = "Reference image size does not match the captured frame";
        return false;
    }

    RenderDiff diff = compare_frames(frame, *reference, scenario.pixel_tolerance, scenario.changed_pixels_threshold_pct);
    if (!write_bmp_rgba(scenario.diff_image_path(), diff.diff_image))
    {
        if (error_message)
            *error_message = "Failed to write diff image";
        return false;
    }

    write_report(scenario.report_path(), scenario, frame, reference, &diff, false);

    if (!diff.passed && error_message)
    {
        std::ostringstream message;
        message << "Render snapshot drifted by " << diff.changed_pixels_pct << "% (" << diff.changed_pixels
                << " pixels, tolerance " << scenario.changed_pixels_threshold_pct << "%)";
        *error_message = message.str();
    }
    return diff.passed;
}

bool export_render_test_frame(const std::filesystem::path& path, const CapturedFrame& frame, std::string* error_message)
{
    if (!frame.valid())
    {
        if (error_message)
            *error_message = "Captured frame is empty";
        return false;
    }

    if (!write_bmp_rgba(path, frame))
    {
        if (error_message)
            *error_message = "Failed to write exported capture image";
        return false;
    }

    return true;
}

void write_render_test_failure_report(const RenderTestScenario& scenario, std::string_view error_message)
{
    write_failure_report(scenario.report_path(), scenario, error_message);
}

} // namespace spectre
