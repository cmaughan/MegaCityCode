#include "app.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <spectre/log.h>
#include <utility>
#include <vector>

namespace spectre
{

namespace
{

void apply_ui_option(UiOptions& options, const std::string& name, const MpackValue& value)
{
    if (name == "ambiwidth" && value.type() == MpackValue::String)
    {
        options.ambiwidth = (value.as_str() == "double") ? AmbiWidth::Double : AmbiWidth::Single;
    }
}

} // namespace

App::App(AppOptions options)
    : options_(std::move(options))
    , config_(options_.initial_config)
    , grid_pipeline_(grid_, highlights_, text_service_)
{
    pending_window_activation_ = options_.activate_window_on_startup;
}

bool App::initialize()
{
    SPECTRE_LOG_INFO(LogCategory::App, "Initializing");
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
    if (!initialize_nvim())
        return false;

    wire_ui_callbacks();
    wire_window_callbacks();

    saw_flush_ = false;
    saw_frame_ = false;
    frame_requested_ = false;
    cursor_visible_ = true;
    cursor_busy_ = false;
    next_blink_deadline_.reset();
    last_activity_time_ = std::chrono::steady_clock::now();

    if (!attach_ui())
        return false;
    if (!execute_startup_commands())
        return false;

    SPECTRE_LOG_INFO(LogCategory::App, "UI attached: %dx%d", grid_cols_, grid_rows_);
    running_ = true;
    rollback.armed = false;
    return true;
}

bool App::initialize_window_and_renderer()
{
    if (!window_.initialize("Spectre", config_.window_width, config_.window_height))
    {
        SPECTRE_LOG_ERROR(LogCategory::App, "Failed to create window");
        return false;
    }
    SPECTRE_LOG_INFO(LogCategory::App, "Window created");

    renderer_ = create_renderer();
    if (!renderer_ || !renderer_->initialize(window_))
    {
        SPECTRE_LOG_ERROR(LogCategory::App, "Failed to init renderer");
        return false;
    }
    SPECTRE_LOG_INFO(LogCategory::App, "Renderer initialized");
    return true;
}

bool App::initialize_text_service()
{
    float display_ppi = window_.display_ppi();
    SPECTRE_LOG_INFO(LogCategory::App, "Display PPI: %.0f", display_ppi);

    TextServiceConfig text_config;
    text_config.font_path = config_.font_path;
    text_config.fallback_paths = config_.fallback_paths;
    if (!text_service_.initialize(text_config, config_.font_size, display_ppi))
    {
        SPECTRE_LOG_ERROR(LogCategory::App, "Failed to load font");
        return false;
    }
    SPECTRE_LOG_INFO(LogCategory::App, "Font loaded");

    const auto& metrics = text_service_.metrics();
    renderer_->set_cell_size(metrics.cell_width, metrics.cell_height);
    renderer_->set_ascender(metrics.ascender);

    auto [pw, ph] = window_.size_pixels();
    int pad = renderer_->padding();
    grid_cols_ = (pw - pad * 2) / metrics.cell_width;
    grid_rows_ = (ph - pad * 2) / metrics.cell_height;
    if (grid_cols_ < 1)
        grid_cols_ = 80;
    if (grid_rows_ < 1)
        grid_rows_ = 24;

    grid_.resize(grid_cols_, grid_rows_);
    renderer_->set_grid_size(grid_cols_, grid_rows_);
    grid_pipeline_.set_renderer(renderer_.get());
    input_.initialize(&rpc_, metrics.cell_width, metrics.cell_height);
    update_text_input_area();
    return true;
}

bool App::initialize_nvim()
{
    if (!nvim_process_.spawn("nvim", options_.nvim_args))
    {
        SPECTRE_LOG_ERROR(LogCategory::App, "Failed to spawn nvim");
        return false;
    }

    if (!rpc_.initialize(nvim_process_))
    {
        SPECTRE_LOG_ERROR(LogCategory::App, "Failed to init RPC");
        return false;
    }

    rpc_.on_notification_available = [this]() { window_.wake(); };
    ui_request_worker_.start(&rpc_);
    return true;
}

bool App::attach_ui()
{
    auto attach = rpc_.request("nvim_ui_attach", { NvimRpc::make_int(grid_cols_), NvimRpc::make_int(grid_rows_), NvimRpc::make_map({
                                                                                                                     { NvimRpc::make_str("rgb"), NvimRpc::make_bool(true) },
                                                                                                                     { NvimRpc::make_str("ext_linegrid"), NvimRpc::make_bool(true) },
                                                                                                                     { NvimRpc::make_str("ext_multigrid"), NvimRpc::make_bool(false) },
                                                                                                                 }) });
    if (!attach.ok())
    {
        SPECTRE_LOG_ERROR(LogCategory::App, "nvim_ui_attach failed");
        return false;
    }
    return true;
}

bool App::execute_startup_commands()
{
    for (const auto& command : options_.startup_commands)
    {
        auto response = rpc_.request("nvim_command", { NvimRpc::make_str(command) });
        if (!response.ok())
        {
            SPECTRE_LOG_ERROR(LogCategory::App, "Startup command failed: %s", command.c_str());
            return false;
        }
    }
    return true;
}

void App::wire_ui_callbacks()
{
    ui_events_.set_grid(&grid_);
    ui_events_.set_highlights(&highlights_);
    ui_events_.set_options(&ui_options_);
    ui_events_.on_flush = [this]() { on_flush(); };
    ui_events_.on_grid_resize = [this](int cols, int rows) {
        grid_cols_ = cols;
        grid_rows_ = rows;
        renderer_->set_grid_size(cols, rows);
        grid_pipeline_.force_full_atlas_upload();
        update_text_input_area();
    };
    ui_events_.on_cursor_goto = [this](int, int) {
        restart_cursor_blink(std::chrono::steady_clock::now());
        update_text_input_area();
    };
    ui_events_.on_mode_change = [this](int) { refresh_cursor_style(true); };
    ui_events_.on_option_set = [this](const std::string& name, const MpackValue& value) {
        apply_ui_option(ui_options_, name, value);
    };
    ui_events_.on_busy = [this](bool busy) { on_busy(busy); };
    ui_events_.on_title = [this](const std::string& title) {
        window_.set_title(title.empty() ? "Spectre" : title);
    };
}

void App::wire_window_callbacks()
{
    window_.on_key = [this](const KeyEvent& e) {
        if (e.pressed && (e.mod & SDL_KMOD_CTRL) && (e.mod & SDL_KMOD_SHIFT))
        {
            if (e.keycode == SDLK_C)
            {
                auto copied = rpc_.request("nvim_eval", { NvimRpc::make_str("getreg('\"')") });
                if (copied.ok() && copied.result.type() == MpackValue::String)
                    window_.set_clipboard_text(copied.result.as_str());
                return;
            }
            if (e.keycode == SDLK_V)
            {
                input_.paste_text(window_.clipboard_text());
                return;
            }
        }

        if (e.pressed && (e.mod & SDL_KMOD_CTRL))
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
        input_.on_key(e);
    };
    window_.on_text_input = [this](const TextInputEvent& e) { input_.on_text_input(e); };
    window_.on_text_editing = [this](const TextEditingEvent& e) { input_.on_text_editing(e); };
    window_.on_mouse_button = [this](const MouseButtonEvent& e) { input_.on_mouse_button(e); };
    window_.on_mouse_move = [this](const MouseMoveEvent& e) { input_.on_mouse_move(e); };
    window_.on_mouse_wheel = [this](const MouseWheelEvent& e) { input_.on_mouse_wheel(e); };
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
        if (saw_flush_ && saw_frame_)
        {
            SPECTRE_LOG_INFO(LogCategory::App, "Smoke test passed");
            return true;
        }
    }

    SPECTRE_LOG_ERROR(LogCategory::App, "Smoke test timed out (flush=%d frame=%d)",
        saw_flush_ ? 1 : 0, saw_frame_ ? 1 : 0);
    return false;
}

std::optional<CapturedFrame> App::run_render_test(std::chrono::milliseconds timeout, std::chrono::milliseconds settle)
{
    auto deadline = std::chrono::steady_clock::now() + timeout;
    bool capture_started = false;
    last_render_test_error_.clear();

    while (running_ && std::chrono::steady_clock::now() < deadline)
    {
        pump_once(deadline);

        if (auto captured = renderer_->take_captured_frame())
            return captured;

        auto now = std::chrono::steady_clock::now();
        if (saw_flush_ && saw_frame_ && !frame_requested_ && now - last_activity_time_ >= settle)
        {
            if (!capture_started)
            {
                SPECTRE_LOG_INFO(LogCategory::App, "Render test requesting capture frame");
                capture_started = true;
            }
            renderer_->request_frame_capture();
            if (renderer_->begin_frame())
            {
                saw_frame_ = true;
                renderer_->end_frame();
                if (auto captured = renderer_->take_captured_frame())
                    return captured;
            }
            else
            {
                SPECTRE_LOG_WARN(LogCategory::App, "Renderer begin_frame() returned false during synchronous render-test capture");
            }
        }
    }

    auto quiet_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - last_activity_time_).count();
    last_render_test_error_ = "Timed out waiting for a stable render capture"
                              " (flush="
        + std::to_string(saw_flush_ ? 1 : 0)
        + " frame=" + std::to_string(saw_frame_ ? 1 : 0)
        + " frame_requested=" + std::to_string(frame_requested_ ? 1 : 0)
        + " quiet_ms=" + std::to_string(quiet_ms)
        + " settle_ms=" + std::to_string(settle.count())
        + " capture_started=" + std::to_string(capture_started ? 1 : 0) + ")";
    SPECTRE_LOG_ERROR(LogCategory::App, "%s", last_render_test_error_.c_str());
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

        if (!nvim_process_.is_running())
        {
            running_ = false;
            return false;
        }

        auto notifications = rpc_.drain_notifications();
        for (auto& notif : notifications)
        {
            if (notif.method == "redraw")
            {
                ui_events_.process_redraw(notif.params);
            }
        }

        advance_cursor_blink(std::chrono::steady_clock::now());

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
                SPECTRE_LOG_WARN(LogCategory::App, "Renderer begin_frame() returned false while a frame was requested");
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

void App::on_flush()
{
    bool first_flush = !saw_flush_;
    saw_flush_ = true;
    last_activity_time_ = std::chrono::steady_clock::now();
    grid_pipeline_.flush();
    if (first_flush && startup_resize_pending_)
    {
        startup_resize_pending_ = false;
        if (pending_startup_resize_cols_ != grid_cols_ || pending_startup_resize_rows_ != grid_rows_)
            queue_resize_request(pending_startup_resize_cols_, pending_startup_resize_rows_, "startup resize");
    }
    refresh_cursor_style(true);
}

void App::on_busy(bool busy)
{
    cursor_busy_ = busy;
    last_activity_time_ = std::chrono::steady_clock::now();
    restart_cursor_blink(std::chrono::steady_clock::now());
}

void App::on_resize(int pixel_w, int pixel_h)
{
    renderer_->resize(pixel_w, pixel_h);
    last_activity_time_ = std::chrono::steady_clock::now();
    request_frame();
    update_text_input_area();

    auto [cell_w, cell_h] = renderer_->cell_size_pixels();
    int pad = renderer_->padding();
    int new_cols = (pixel_w - pad * 2) / cell_w;
    int new_rows = (pixel_h - pad * 2) / cell_h;

    if (new_cols < 1)
        new_cols = 1;
    if (new_rows < 1)
        new_rows = 1;

    if (new_cols == grid_cols_ && new_rows == grid_rows_)
        return;

    if (!saw_flush_)
    {
        startup_resize_pending_ = true;
        pending_startup_resize_cols_ = new_cols;
        pending_startup_resize_rows_ = new_rows;
        return;
    }

    queue_resize_request(new_cols, new_rows, "window resize");
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
    renderer_->set_grid_size(grid_cols_, grid_rows_);
    input_.set_cell_size(metrics.cell_width, metrics.cell_height);
    grid_.mark_all_dirty();
    grid_pipeline_.force_full_atlas_upload();
    update_text_input_area();

    auto [pw, ph] = window_.size_pixels();
    int pad = renderer_->padding();
    int new_cols = (pw - pad * 2) / metrics.cell_width;
    int new_rows = (ph - pad * 2) / metrics.cell_height;
    if (new_cols < 1)
        new_cols = 1;
    if (new_rows < 1)
        new_rows = 1;

    grid_pipeline_.flush();
    refresh_cursor_style(true);

    if (new_cols != grid_cols_ || new_rows != grid_rows_)
        queue_resize_request(new_cols, new_rows, "font resize");

    config_.font_size = new_size;
    config_.save();
}

void App::request_frame()
{
    frame_requested_ = true;
    window_.wake();
}

void App::request_quit()
{
    if (nvim_process_.is_running())
    {
        rpc_.notify("nvim_input", { NvimRpc::make_str("<C-\\><C-n>:qa!<CR>") });
    }
    running_ = false;
}

void App::refresh_cursor_style(bool restart_blink)
{
    cursor_style_ = {};
    cursor_style_.bg = highlights_.default_fg();
    cursor_style_.fg = highlights_.default_bg();

    int mode = ui_events_.current_mode();
    if (mode >= 0 && mode < (int)ui_events_.modes().size())
    {
        const auto& mode_info = ui_events_.modes()[mode];
        cursor_style_.shape = mode_info.cursor_shape;
        cursor_style_.cell_percentage = mode_info.cell_percentage;
        if (mode_info.attr_id != 0)
        {
            Color fg;
            Color bg;
            highlights_.resolve(highlights_.get((uint16_t)mode_info.attr_id), fg, bg);
            cursor_style_.fg = fg;
            cursor_style_.bg = bg;
            cursor_style_.use_explicit_colors = true;
        }
    }

    if (restart_blink)
    {
        restart_cursor_blink(std::chrono::steady_clock::now());
    }
    else
    {
        apply_cursor_visibility();
    }
}

void App::restart_cursor_blink(std::chrono::steady_clock::time_point now)
{
    const ModeInfo* mode_info = nullptr;
    int mode = ui_events_.current_mode();
    if (mode >= 0 && mode < (int)ui_events_.modes().size())
        mode_info = &ui_events_.modes()[mode];

    if (cursor_busy_)
    {
        cursor_visible_ = false;
        cursor_blink_phase_ = CursorBlinkPhase::Steady;
        next_blink_deadline_.reset();
        apply_cursor_visibility();
        return;
    }

    bool blink_enabled = mode_info && mode_info->blinkwait > 0 && mode_info->blinkon > 0 && mode_info->blinkoff > 0;
    cursor_visible_ = true;
    if (!blink_enabled)
    {
        cursor_blink_phase_ = CursorBlinkPhase::Steady;
        next_blink_deadline_.reset();
    }
    else
    {
        cursor_blink_phase_ = CursorBlinkPhase::Wait;
        next_blink_deadline_ = now + std::chrono::milliseconds(mode_info->blinkwait);
    }
    apply_cursor_visibility();
}

void App::advance_cursor_blink(std::chrono::steady_clock::time_point now)
{
    if (!next_blink_deadline_ || now < *next_blink_deadline_)
        return;

    const ModeInfo* mode_info = nullptr;
    int mode = ui_events_.current_mode();
    if (mode >= 0 && mode < (int)ui_events_.modes().size())
        mode_info = &ui_events_.modes()[mode];

    if (cursor_busy_ || !mode_info || mode_info->blinkwait <= 0 || mode_info->blinkon <= 0 || mode_info->blinkoff <= 0)
    {
        restart_cursor_blink(now);
        return;
    }

    switch (cursor_blink_phase_)
    {
    case CursorBlinkPhase::Wait:
    case CursorBlinkPhase::On:
        cursor_visible_ = false;
        cursor_blink_phase_ = CursorBlinkPhase::Off;
        next_blink_deadline_ = now + std::chrono::milliseconds(mode_info->blinkoff);
        break;

    case CursorBlinkPhase::Off:
        cursor_visible_ = true;
        cursor_blink_phase_ = CursorBlinkPhase::On;
        next_blink_deadline_ = now + std::chrono::milliseconds(mode_info->blinkon);
        break;

    case CursorBlinkPhase::Steady:
        restart_cursor_blink(now);
        return;
    }

    apply_cursor_visibility();
}

void App::apply_cursor_visibility()
{
    int cursor_col = cursor_visible_ ? ui_events_.cursor_col() : -1;
    int cursor_row = cursor_visible_ ? ui_events_.cursor_row() : -1;
    renderer_->set_cursor(cursor_col, cursor_row, cursor_style_);
    update_text_input_area();
    request_frame();
}

void App::update_text_input_area()
{
    auto [cell_w, cell_h] = renderer_->cell_size_pixels();
    int x = renderer_->padding() + ui_events_.cursor_col() * cell_w;
    int y = renderer_->padding() + ui_events_.cursor_row() * cell_h;
    window_.set_text_input_area(x, y, cell_w, cell_h);
}

void App::queue_resize_request(int cols, int rows, const char* reason)
{
    ui_request_worker_.request_resize(cols, rows, reason);
}

int App::wait_timeout_ms(std::optional<std::chrono::steady_clock::time_point> wait_deadline) const
{
    auto deadline = next_blink_deadline_;
    if (wait_deadline && (!deadline || *wait_deadline < *deadline))
        deadline = wait_deadline;

    if (!deadline)
        return -1;

    auto now = std::chrono::steady_clock::now();
    if (now >= *deadline)
        return 0;

    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(*deadline - now);
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

    ui_request_worker_.stop();

    if (nvim_process_.is_running())
    {
        rpc_.notify("nvim_input", { NvimRpc::make_str("<C-\\><C-n>:qa!<CR>") });
    }
    nvim_process_.shutdown();
    rpc_.shutdown();

    text_service_.shutdown();
    if (renderer_)
        renderer_->shutdown();
    window_.shutdown();
}

} // namespace spectre
