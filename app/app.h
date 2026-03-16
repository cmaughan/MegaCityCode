#pragma once
#include "app_config.h"
#include <chrono>
#include <megacitycode/sdl_window.h>
#include <optional>
#include <string>
#ifdef __APPLE__
// Metal renderer header is internal to megacitycode-renderer, forward declare
#else
// Vulkan renderer header is internal to megacitycode-renderer, forward declare
#endif
#include <megacitycode/grid.h>
#include <megacitycode/renderer.h>
#include <megacitycode/text_service.h>
#include <memory>

namespace megacitycode
{

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
    void wire_ui_callbacks();
    void wire_window_callbacks();

    bool pump_once(std::optional<std::chrono::steady_clock::time_point> wait_deadline = std::nullopt);
    void on_resize(int pixel_w, int pixel_h);
    void change_font_size(int new_size);
    void request_frame();
    void request_quit();
    void refresh_grid_dimensions(int pixel_w, int pixel_h);
    int wait_timeout_ms(std::optional<std::chrono::steady_clock::time_point> wait_deadline) const;

    AppOptions options_;
    AppConfig config_;
    SdlWindow window_;
    std::unique_ptr<IRenderer> renderer_;
    TextService text_service_;
    Grid grid_;

    bool running_ = false;
    bool pending_window_activation_ = true;
    bool saw_frame_ = false;
    bool frame_requested_ = false;
    int grid_cols_ = 0, grid_rows_ = 0;
    float display_ppi_ = 96.0f;
    std::chrono::steady_clock::time_point last_activity_time_ = std::chrono::steady_clock::now();
    std::string last_render_test_error_;
};

} // namespace megacitycode
