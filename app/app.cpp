#include "app.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <megacitycode/log.h>
#include <utility>

namespace megacitycode
{

App::App(AppOptions options)
    : options_(std::move(options))
    , config_(options_.initial_config)
{
    pending_window_activation_ = options_.activate_window_on_startup;
}

bool App::initialize()
{
    MEGACITYCODE_LOG_INFO(LogCategory::App, "Initializing");
    if (options_.load_user_config)
        config_ = AppConfig::load();

    struct InitRollback
    {
        App* app = nullptr;
        bool armed = true;
        ~InitRollback()
        {
            if (armed)
                app->shutdown();
        }
    } rollback{ this };

    if (!initialize_window_and_renderer())
        return false;
    if (!initialize_text_service())
        return false;

    wire_ui_callbacks();
    wire_window_callbacks();

    saw_frame_ = false;
    frame_requested_ = false;
    last_activity_time_ = std::chrono::steady_clock::now();
    running_ = true;
    request_frame();
    MEGACITYCODE_LOG_INFO(LogCategory::App, "Viewer ready: %dx%d cells reserved for future use", grid_cols_, grid_rows_);
    rollback.armed = false;
    return true;
}

bool App::initialize_window_and_renderer()
{
    window_.set_clamp_to_display(options_.clamp_window_to_display);
    if (!window_.initialize("MegaCityCode Viewer", config_.window_width, config_.window_height))
    {
        MEGACITYCODE_LOG_ERROR(LogCategory::App, "Failed to create window");
        return false;
    }
    MEGACITYCODE_LOG_INFO(LogCategory::App, "Window created");

    renderer_ = create_renderer();
    if (!renderer_ || !renderer_->initialize(window_))
    {
        MEGACITYCODE_LOG_ERROR(LogCategory::App, "Failed to init renderer");
        return false;
    }
    MEGACITYCODE_LOG_INFO(LogCategory::App, "Renderer initialized");
    return true;
}

bool App::initialize_text_service()
{
    display_ppi_ = window_.display_ppi();
    MEGACITYCODE_LOG_INFO(LogCategory::App, "Display PPI: %.0f", display_ppi_);

    TextServiceConfig text_config;
    text_config.font_path = config_.font_path;
    text_config.fallback_paths = config_.fallback_paths;
    if (!text_service_.initialize(text_config, config_.font_size, display_ppi_))
    {
        MEGACITYCODE_LOG_ERROR(LogCategory::App, "Failed to load font");
        return false;
    }
    MEGACITYCODE_LOG_INFO(LogCategory::App, "Font loaded");

    const auto& metrics = text_service_.metrics();
    renderer_->set_cell_size(metrics.cell_width, metrics.cell_height);
    renderer_->set_ascender(metrics.ascender);

    auto [pw, ph] = window_.size_pixels();
    refresh_grid_dimensions(pw, ph);
    return true;
}

void App::wire_ui_callbacks()
{
}

void App::wire_window_callbacks()
{
    window_.on_key = [this](const KeyEvent& e) {
        if (!e.pressed)
            return;

        if (e.keycode == SDLK_ESCAPE)
        {
            request_quit();
            return;
        }

        if ((e.mod & SDL_KMOD_CTRL))
        {
            if (e.keycode == SDLK_EQUALS || e.keycode == SDLK_PLUS)
            {
                change_font_size(text_service_.point_size() + 1);
                return;
            }
            if (e.keycode == SDLK_MINUS)
            {
                change_font_size(text_service_.point_size() - 1);
                return;
            }
            if (e.keycode == SDLK_0)
            {
                change_font_size(TextService::DEFAULT_POINT_SIZE);
                return;
            }
        }
    };
    window_.on_resize = [this](const WindowResizeEvent& e) { on_resize(e.width, e.height); };
}

void App::run()
{
    while (running_)
    {
        pump_once();
    }
}

bool App::run_smoke_test(std::chrono::milliseconds timeout)
{
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (running_ && std::chrono::steady_clock::now() < deadline)
    {
        pump_once(deadline);
        if (saw_frame_)
        {
            MEGACITYCODE_LOG_INFO(LogCategory::App, "Smoke test passed");
            return true;
        }
    }

    MEGACITYCODE_LOG_ERROR(LogCategory::App, "Smoke test timed out before the first frame");
    return false;
}

std::optional<CapturedFrame> App::run_render_test(std::chrono::milliseconds timeout, std::chrono::milliseconds settle)
{
    auto deadline = std::chrono::steady_clock::now() + timeout;
    bool capture_requested = false;
    last_render_test_error_.clear();

    while (running_ && std::chrono::steady_clock::now() < deadline)
    {
        pump_once(deadline);

        if (auto captured = renderer_->take_captured_frame())
            return captured;

        auto now = std::chrono::steady_clock::now();
        if (saw_frame_ && !capture_requested && now - last_activity_time_ >= settle)
        {
            renderer_->request_frame_capture();
            capture_requested = true;
            if (renderer_->begin_frame())
            {
                renderer_->end_frame();
                if (auto captured = renderer_->take_captured_frame())
                    return captured;
            }
            else
            {
                MEGACITYCODE_LOG_WARN(LogCategory::App, "Renderer begin_frame() returned false during synchronous render capture");
            }
        }
    }

    last_render_test_error_ = "Timed out waiting for a render capture"
                              " (frame="
        + std::to_string(saw_frame_ ? 1 : 0)
        + " frame_requested=" + std::to_string(frame_requested_ ? 1 : 0)
        + " settle_ms=" + std::to_string(settle.count())
        + " capture_requested=" + std::to_string(capture_requested ? 1 : 0) + ")";
    MEGACITYCODE_LOG_ERROR(LogCategory::App, "%s", last_render_test_error_.c_str());
    return std::nullopt;
}

bool App::pump_once(std::optional<std::chrono::steady_clock::time_point> wait_deadline)
{
    while (running_)
    {
        if (pending_window_activation_)
        {
            window_.activate();
            pending_window_activation_ = false;
        }

        if (!window_.poll_events())
        {
            request_quit();
            return false;
        }

        if (frame_requested_)
        {
            if (renderer_->begin_frame())
            {
                saw_frame_ = true;
                renderer_->end_frame();
                frame_requested_ = false;
            }
            else
            {
                MEGACITYCODE_LOG_WARN(LogCategory::App, "Renderer begin_frame() returned false while a frame was requested");
            }
            return running_;
        }

        if (wait_deadline && std::chrono::steady_clock::now() >= *wait_deadline)
            return running_;

        int timeout_ms = wait_timeout_ms(wait_deadline);
        if (!window_.wait_events(timeout_ms))
        {
            request_quit();
            return false;
        }
    }

    return false;
}

void App::on_resize(int pixel_w, int pixel_h)
{
    renderer_->resize(pixel_w, pixel_h);
    refresh_grid_dimensions(pixel_w, pixel_h);
    last_activity_time_ = std::chrono::steady_clock::now();
    request_frame();
}

void App::change_font_size(int new_size)
{
    new_size = std::max(TextService::MIN_POINT_SIZE, std::min(TextService::MAX_POINT_SIZE, new_size));
    if (new_size == text_service_.point_size())
        return;
    if (!text_service_.set_point_size(new_size))
        return;

    const auto& metrics = text_service_.metrics();

    renderer_->set_cell_size(metrics.cell_width, metrics.cell_height);
    renderer_->set_ascender(metrics.ascender);

    auto [pw, ph] = window_.size_pixels();
    refresh_grid_dimensions(pw, ph);

    config_.font_size = new_size;
    config_.save();
    last_activity_time_ = std::chrono::steady_clock::now();
    request_frame();
}

void App::request_frame()
{
    frame_requested_ = true;
    window_.wake();
}

void App::request_quit()
{
    running_ = false;
    window_.wake();
}

void App::refresh_grid_dimensions(int pixel_w, int pixel_h)
{
    const auto& metrics = text_service_.metrics();
    const int cell_w = std::max(1, metrics.cell_width);
    const int cell_h = std::max(1, metrics.cell_height);
    const int pad = std::max(0, renderer_->padding());

    grid_cols_ = std::max(1, (pixel_w - pad * 2) / cell_w);
    grid_rows_ = std::max(1, (pixel_h - pad * 2) / cell_h);
    grid_.resize(grid_cols_, grid_rows_);
    renderer_->set_grid_size(grid_cols_, grid_rows_);
}

int App::wait_timeout_ms(std::optional<std::chrono::steady_clock::time_point> wait_deadline) const
{
    if (!wait_deadline)
        return -1;

    auto now = std::chrono::steady_clock::now();
    if (now >= *wait_deadline)
        return 0;

    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(*wait_deadline - now);
    return std::max(1, (int)delta.count());
}

void App::shutdown()
{
    if (options_.save_user_config)
    {
        auto [window_w, window_h] = window_.size_logical();
        if (window_w > 0 && window_h > 0)
        {
            config_.window_width = window_w;
            config_.window_height = window_h;
        }
        config_.font_size = text_service_.point_size();
        config_.font_path = text_service_.primary_font_path();
        config_.save();
    }

    text_service_.shutdown();
    if (renderer_)
        renderer_->shutdown();
    window_.shutdown();
}

} // namespace megacitycode
