#include "app.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <spectre/log.h>
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

bool App::initialize()
{
    SPECTRE_LOG_INFO(LogCategory::App, "Initializing");

    // 1. Create window
    if (!window_.initialize("Spectre", 1280, 800))
    {
        SPECTRE_LOG_ERROR(LogCategory::App, "Failed to create window");
        return false;
    }
    SPECTRE_LOG_INFO(LogCategory::App, "Window created");

    // 2. Init renderer
    renderer_ = create_renderer();
    if (!renderer_ || !renderer_->initialize(window_))
    {
        SPECTRE_LOG_ERROR(LogCategory::App, "Failed to init renderer");
        return false;
    }
    SPECTRE_LOG_INFO(LogCategory::App, "Renderer initialized");

    // 3. Load font
    float display_ppi = window_.display_ppi();
    SPECTRE_LOG_INFO(LogCategory::App, "Display PPI: %.0f", display_ppi);
    if (!text_service_.initialize(TextService::DEFAULT_POINT_SIZE, display_ppi))
    {
        SPECTRE_LOG_ERROR(LogCategory::App, "Failed to load font");
        return false;
    }
    SPECTRE_LOG_INFO(LogCategory::App, "Font loaded");

    // Set cell size from font metrics
    const auto& metrics = text_service_.metrics();
    renderer_->set_cell_size(metrics.cell_width, metrics.cell_height);
    renderer_->set_ascender(metrics.ascender);

    // 4. Calculate grid dimensions
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
    grid_pipeline_.initialize(renderer_.get(), &grid_, &highlights_, &text_service_);

    // 5. Spawn nvim
    if (!nvim_process_.spawn())
    {
        SPECTRE_LOG_ERROR(LogCategory::App, "Failed to spawn nvim");
        return false;
    }

    // 6. Init RPC
    if (!rpc_.initialize(nvim_process_))
    {
        SPECTRE_LOG_ERROR(LogCategory::App, "Failed to init RPC");
        return false;
    }

    // 7. Setup UI event handler
    ui_events_.set_grid(&grid_);
    ui_events_.set_highlights(&highlights_);
    ui_events_.set_options(&ui_options_);
    ui_events_.on_flush = [this]() { on_flush(); };
    ui_events_.on_grid_resize = [this](int cols, int rows) {
        grid_cols_ = cols;
        grid_rows_ = rows;
        renderer_->set_grid_size(cols, rows);
        grid_pipeline_.force_full_atlas_upload();
    };
    ui_events_.on_option_set = [this](const std::string& name, const MpackValue& value) {
        apply_ui_option(ui_options_, name, value);
    };

    // 8. Setup input handler
    input_.initialize(&rpc_, metrics.cell_width, metrics.cell_height);

    // 9. Wire window events
    window_.on_key = [this](const KeyEvent& e) {
        if (e.pressed && (e.mod & SDL_KMOD_CTRL))
        {
            if (e.keycode == SDLK_EQUALS || e.keycode == SDLK_PLUS)
            {
                change_font_size(text_service_.point_size() + 1);
                return;
            }
            else if (e.keycode == SDLK_MINUS)
            {
                change_font_size(text_service_.point_size() - 1);
                return;
            }
            else if (e.keycode == SDLK_0)
            {
                change_font_size(TextService::DEFAULT_POINT_SIZE);
                return;
            }
        }
        input_.on_key(e);
    };
    window_.on_text_input = [this](const TextInputEvent& e) { input_.on_text_input(e); };
    window_.on_mouse_button = [this](const MouseButtonEvent& e) { input_.on_mouse_button(e); };
    window_.on_mouse_move = [this](const MouseMoveEvent& e) { input_.on_mouse_move(e); };
    window_.on_mouse_wheel = [this](const MouseWheelEvent& e) { input_.on_mouse_wheel(e); };
    window_.on_resize = [this](const WindowResizeEvent& e) { on_resize(e.width, e.height); };

    // 10. Attach UI
    auto attach = rpc_.request("nvim_ui_attach", { NvimRpc::make_int(grid_cols_), NvimRpc::make_int(grid_rows_), NvimRpc::make_map({
                                                                                                                     { NvimRpc::make_str("rgb"), NvimRpc::make_bool(true) },
                                                                                                                     { NvimRpc::make_str("ext_linegrid"), NvimRpc::make_bool(true) },
                                                                                                                 }) });
    if (!attach.ok())
    {
        SPECTRE_LOG_ERROR(LogCategory::App, "nvim_ui_attach failed");
        return false;
    }

    SPECTRE_LOG_INFO(LogCategory::App, "UI attached: %dx%d", grid_cols_, grid_rows_);
    saw_flush_ = false;
    saw_frame_ = false;
    running_ = true;
    return true;
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
        pump_once();
        if (saw_flush_ && saw_frame_)
        {
            SPECTRE_LOG_INFO(LogCategory::App, "Smoke test passed");
            return true;
        }
        SDL_Delay(10);
    }

    SPECTRE_LOG_ERROR(LogCategory::App, "Smoke test timed out (flush=%d frame=%d)",
        saw_flush_ ? 1 : 0, saw_frame_ ? 1 : 0);
    return false;
}

bool App::pump_once()
{
    if (pending_window_activation_)
    {
        window_.activate();
        pending_window_activation_ = false;
    }

    if (!window_.poll_events())
    {
        rpc_.notify("nvim_input", { NvimRpc::make_str("<C-\\><C-n>:qa!<CR>") });
        running_ = false;
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

    if (renderer_->begin_frame())
    {
        saw_frame_ = true;
        renderer_->end_frame();
    }

    return running_;
}

void App::on_flush()
{
    saw_flush_ = true;
    grid_pipeline_.flush();

    CursorStyle cursor_style;
    cursor_style.bg = highlights_.default_fg();
    cursor_style.fg = highlights_.default_bg();

    int mode = ui_events_.current_mode();
    if (mode >= 0 && mode < (int)ui_events_.modes().size())
    {
        const auto& mode_info = ui_events_.modes()[mode];
        cursor_style.shape = mode_info.cursor_shape;
        cursor_style.cell_percentage = mode_info.cell_percentage;
        if (mode_info.attr_id != 0)
        {
            Color fg;
            Color bg;
            highlights_.resolve(highlights_.get((uint16_t)mode_info.attr_id), fg, bg);
            cursor_style.fg = fg;
            cursor_style.bg = bg;
            cursor_style.use_explicit_colors = true;
        }
    }
    renderer_->set_cursor(ui_events_.cursor_col(), ui_events_.cursor_row(), cursor_style);
}

void App::on_resize(int pixel_w, int pixel_h)
{
    renderer_->resize(pixel_w, pixel_h);

    auto [cell_w, cell_h] = renderer_->cell_size_pixels();
    int pad = renderer_->padding();
    int new_cols = (pixel_w - pad * 2) / cell_w;
    int new_rows = (pixel_h - pad * 2) / cell_h;

    if (new_cols < 1)
        new_cols = 1;
    if (new_rows < 1)
        new_rows = 1;

    if (new_cols != grid_cols_ || new_rows != grid_rows_)
    {
        auto resize = rpc_.request("nvim_ui_try_resize", { NvimRpc::make_int(new_cols), NvimRpc::make_int(new_rows) });
        if (!resize.ok())
        {
            SPECTRE_LOG_WARN(LogCategory::App, "nvim_ui_try_resize failed during window resize");
        }
    }
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

    auto [pw, ph] = window_.size_pixels();
    int pad = renderer_->padding();
    int new_cols = (pw - pad * 2) / metrics.cell_width;
    int new_rows = (ph - pad * 2) / metrics.cell_height;
    if (new_cols < 1)
        new_cols = 1;
    if (new_rows < 1)
        new_rows = 1;

    grid_pipeline_.flush();

    if (new_cols != grid_cols_ || new_rows != grid_rows_)
    {
        auto resize = rpc_.request("nvim_ui_try_resize", { NvimRpc::make_int(new_cols), NvimRpc::make_int(new_rows) });
        if (!resize.ok())
        {
            SPECTRE_LOG_WARN(LogCategory::App, "nvim_ui_try_resize failed during font resize");
        }
    }
}

void App::shutdown()
{
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
