#pragma once
#include "grid_rendering_pipeline.h"
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
    bool initialize();
    void run();
    bool run_smoke_test(std::chrono::milliseconds timeout);
    void shutdown();

private:
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
    int wait_timeout_ms(std::optional<std::chrono::steady_clock::time_point> wait_deadline) const;

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

    bool running_ = false;
    bool pending_window_activation_ = true;
    bool saw_flush_ = false;
    bool saw_frame_ = false;
    bool frame_requested_ = false;
    bool cursor_visible_ = true;
    bool cursor_busy_ = false;
    CursorStyle cursor_style_ = {};
    CursorBlinkPhase cursor_blink_phase_ = CursorBlinkPhase::Steady;
    std::optional<std::chrono::steady_clock::time_point> next_blink_deadline_;
    int grid_cols_ = 0, grid_rows_ = 0;
};

} // namespace spectre
