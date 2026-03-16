#pragma once
#include "app_config.h"
#include "cursor_blinker.h"
#include "grid_rendering_pipeline.h"
#include "ui_request_worker.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <spectre/sdl_window.h>
#include <string>
#include <vector>
#ifdef __APPLE__
// Metal renderer header is internal to spectre-renderer, forward declare
#else
// Vulkan renderer header is internal to spectre-renderer, forward declare
#endif
#include <memory>
#include <spectre/grid.h>
#include <spectre/nvim.h>
#include <spectre/renderer.h>
#include <spectre/text_service.h>

namespace spectre
{

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

class App
{
public:
    explicit App(AppOptions options = {});
    bool initialize();
    void run();
    bool run_smoke_test(std::chrono::milliseconds timeout);
    std::optional<CapturedFrame> run_render_test(std::chrono::milliseconds timeout, std::chrono::milliseconds settle);
    void shutdown();
    const std::string& last_render_test_error() const
    {
        return last_render_test_error_;
    }

private:
    bool initialize_window_and_renderer();
    bool initialize_text_service();
    bool initialize_nvim();
    bool attach_ui();
    void wire_ui_callbacks();
    void wire_window_callbacks();

    bool pump_once(std::optional<std::chrono::steady_clock::time_point> wait_deadline = std::nullopt);
    void on_flush();
    void on_busy(bool busy);
    void on_resize(int pixel_w, int pixel_h);
    void change_font_size(int new_size);
    void request_frame();
    void request_quit();
    void refresh_cursor_style(bool restart_blink);
    bool execute_startup_commands();
    void restart_cursor_blink(std::chrono::steady_clock::time_point now);
    void apply_cursor_visibility();
    void toggle_debug_overlay();
    void update_debug_overlay();
    void update_text_input_area();
    void queue_resize_request(int cols, int rows, const char* reason);
    int wait_timeout_ms(std::optional<std::chrono::steady_clock::time_point> wait_deadline) const;
    double average_frame_ms() const;
    BlinkTiming current_blink_timing() const;

    AppOptions options_;
    AppConfig config_;
    SdlWindow window_;
    std::unique_ptr<IRenderer> renderer_;
    TextService text_service_;
    Grid grid_;
    HighlightTable highlights_;
    NvimProcess nvim_process_;
    NvimRpc rpc_;
    UiEventHandler ui_events_;
    NvimInput input_;
    GridRenderingPipeline grid_pipeline_;
    UiOptions ui_options_;
    UiRequestWorker ui_request_worker_;

    bool running_ = false;
    bool pending_window_activation_ = true;
    bool saw_flush_ = false;
    bool saw_frame_ = false;
    bool frame_requested_ = false;
    bool cursor_busy_ = false;
    bool debug_overlay_enabled_ = false;
    bool startup_resize_pending_ = false;
    CursorStyle cursor_style_ = {};
    CursorBlinker cursor_blinker_;
    int pending_startup_resize_cols_ = 0;
    int pending_startup_resize_rows_ = 0;
    int grid_cols_ = 0, grid_rows_ = 0;
    size_t last_flush_dirty_cells_ = 0;
    double last_frame_ms_ = 0.0;
    std::array<double, 32> recent_frame_ms_ = {};
    size_t recent_frame_ms_count_ = 0;
    size_t recent_frame_ms_index_ = 0;
    float display_ppi_ = 96.0f;
    std::chrono::steady_clock::time_point last_activity_time_ = std::chrono::steady_clock::now();
    std::string last_render_test_error_;
};

} // namespace spectre
