#pragma once
#include "app_config.h"
#include "grid_rendering_pipeline.h"
#include "ui_request_worker.h"
#include <chrono>
#include <optional>
#include <spectre/sdl_window.h>
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

class App
{
public:
    App();
    bool initialize();
    void run();
    bool run_smoke_test(std::chrono::milliseconds timeout);
    void shutdown();

private:
    bool initialize_window_and_renderer();
    bool initialize_text_service();
    bool initialize_nvim();
    bool attach_ui();
    void wire_ui_callbacks();
    void wire_window_callbacks();

    enum class CursorBlinkPhase
    {
        Steady,
        Wait,
        Off,
        On,
    };

    bool pump_once(std::optional<std::chrono::steady_clock::time_point> wait_deadline = std::nullopt);
    void on_flush();
    void on_busy(bool busy);
    void on_resize(int pixel_w, int pixel_h);
    void change_font_size(int new_size);
    void request_frame();
    void request_quit();
    void refresh_cursor_style(bool restart_blink);
    void restart_cursor_blink(std::chrono::steady_clock::time_point now);
    void advance_cursor_blink(std::chrono::steady_clock::time_point now);
    void apply_cursor_visibility();
    void update_text_input_area();
    void queue_resize_request(int cols, int rows, const char* reason);
    int wait_timeout_ms(std::optional<std::chrono::steady_clock::time_point> wait_deadline) const;

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
    bool cursor_visible_ = true;
    bool cursor_busy_ = false;
    bool startup_resize_pending_ = false;
    CursorStyle cursor_style_ = {};
    CursorBlinkPhase cursor_blink_phase_ = CursorBlinkPhase::Steady;
    std::optional<std::chrono::steady_clock::time_point> next_blink_deadline_;
    int pending_startup_resize_cols_ = 0;
    int pending_startup_resize_rows_ = 0;
    int grid_cols_ = 0, grid_rows_ = 0;
};

} // namespace spectre
