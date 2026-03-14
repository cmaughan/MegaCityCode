#include "app.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <vector>
#include <algorithm>

namespace spectre {

bool App::initialize() {
    fprintf(stderr, "[spectre] Initializing...\n");

    // 1. Create window
    if (!window_.initialize("Spectre", 1280, 800)) {
        fprintf(stderr, "[spectre] Failed to create window\n");
        return false;
    }
    fprintf(stderr, "[spectre] Window created\n");

    // 2. Init renderer
    if (!renderer_.initialize(window_)) {
        fprintf(stderr, "[spectre] Failed to init renderer\n");
        return false;
    }
    fprintf(stderr, "[spectre] Renderer initialized\n");

    // 3. Load font — use physical display PPI so the same point size
    // produces the same physical height on any display
    display_ppi_ = window_.display_ppi();
    fprintf(stderr, "[spectre] Display PPI: %.0f\n", display_ppi_);
    font_path_ = "fonts/JetBrainsMonoNerdFontMono-Regular.ttf";
    if (!font_.initialize(font_path_, font_size_, display_ppi_)) {
        fprintf(stderr, "[spectre] Failed to load font\n");
        return false;
    }
    fprintf(stderr, "[spectre] Font loaded\n");

    // Set cell size from font metrics
    auto& metrics = font_.metrics();
    renderer_.set_cell_size(metrics.cell_width, metrics.cell_height);
    renderer_.set_ascender(metrics.ascender);

    // Init glyph cache and shaper
    glyph_cache_.initialize(font_.face(), font_.point_size());
    shaper_.initialize(font_.hb_font());

    // 4. Calculate grid dimensions (subtract padding on both sides)
    auto [pw, ph] = window_.size_pixels();
    int pad = renderer_.padding();
    grid_cols_ = (pw - pad * 2) / metrics.cell_width;
    grid_rows_ = (ph - pad * 2) / metrics.cell_height;
    if (grid_cols_ < 1) grid_cols_ = 80;
    if (grid_rows_ < 1) grid_rows_ = 24;

    grid_.resize(grid_cols_, grid_rows_);
    renderer_.set_grid_size(grid_cols_, grid_rows_);

    // Upload initial (empty) atlas
    renderer_.set_atlas_texture(glyph_cache_.atlas_data(),
        glyph_cache_.atlas_width(), glyph_cache_.atlas_height());
    atlas_needs_full_upload_ = false;

    // 5. Spawn nvim
    if (!nvim_process_.spawn()) {
        fprintf(stderr, "Failed to spawn nvim\n");
        return false;
    }

    // 6. Init RPC
    if (!rpc_.initialize(nvim_process_)) {
        fprintf(stderr, "Failed to init RPC\n");
        return false;
    }

    // 7. Setup UI event handler
    ui_events_.set_grid(&grid_);
    ui_events_.set_highlights(&highlights_);
    ui_events_.on_flush = [this]() { on_flush(); };
    ui_events_.on_grid_resize = [this](int cols, int rows) {
        grid_cols_ = cols;
        grid_rows_ = rows;
        renderer_.set_grid_size(cols, rows);
    };

    // 8. Setup input handler
    input_.initialize(&rpc_, metrics.cell_width, metrics.cell_height);

    // 9. Wire window events
    window_.on_key = [this](const KeyEvent& e) {
        // Intercept Ctrl+=/- for font size
        if (e.pressed && (e.mod & SDL_KMOD_CTRL)) {
            if (e.keycode == SDLK_EQUALS || e.keycode == SDLK_PLUS) {
                change_font_size(font_size_ + 1);
                return;
            } else if (e.keycode == SDLK_MINUS) {
                change_font_size(font_size_ - 1);
                return;
            } else if (e.keycode == SDLK_0) {
                change_font_size(DEFAULT_FONT_SIZE);
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
    rpc_.request("nvim_ui_attach", {
        NvimRpc::make_int(grid_cols_),
        NvimRpc::make_int(grid_rows_),
        NvimRpc::make_map({
            { NvimRpc::make_str("rgb"), NvimRpc::make_bool(true) },
            { NvimRpc::make_str("ext_linegrid"), NvimRpc::make_bool(true) },
        })
    });

    fprintf(stderr, "[spectre] UI attached: %dx%d\n", grid_cols_, grid_rows_);

    running_ = true;
    return true;
}

void App::run() {
    while (running_) {
        // 1. Poll window events (input forwarded via callbacks)
        if (!window_.poll_events()) {
            rpc_.notify("nvim_input", { NvimRpc::make_str("<C-\\><C-n>:qa!<CR>") });
            running_ = false;
            break;
        }

        // Check if nvim is still running
        if (!nvim_process_.is_running()) {
            running_ = false;
            break;
        }

        // 2. Begin frame (waits for GPU fence - safe to write SSBO after this)
        if (renderer_.begin_frame()) {
            // 3. Process nvim notifications (may write SSBO via on_flush)
            auto notifications = rpc_.drain_notifications();
            for (auto& notif : notifications) {
                if (notif.method == "redraw") {
                    ui_events_.process_redraw(notif.params);
                }
            }

            // 4. Record and submit
            renderer_.end_frame();
        }
    }
}

void App::on_flush() {
    flush_count_++;
    update_grid_to_renderer();

    // Update cursor
    int mode = ui_events_.current_mode();
    CursorShape shape = CursorShape::Block;
    if (mode >= 0 && mode < (int)ui_events_.modes().size()) {
        shape = ui_events_.modes()[mode].cursor_shape;
    }
    renderer_.set_cursor(ui_events_.cursor_col(), ui_events_.cursor_row(),
        shape, { 1.0f, 1.0f, 1.0f, 0.7f });
}

void App::update_grid_to_renderer() {
    auto dirty = grid_.get_dirty_cells();
    if (dirty.empty()) return;

    std::vector<CellUpdate> updates;
    updates.reserve(dirty.size());

    bool atlas_updated = false;

    for (auto& [col, row] : dirty) {
        const auto& cell = grid_.get_cell(col, row);
        const auto& hl = highlights_.get(cell.hl_attr_id);

        Color fg, bg;
        highlights_.resolve(hl, fg, bg);

        CellUpdate update;
        update.col = col;
        update.row = row;
        update.bg = bg;
        update.fg = fg;
        update.style_flags = hl.style_flags();

        // Get glyph from cache (skip continuation cells)
        if (!cell.double_width_cont && cell.codepoint != ' ' && cell.codepoint != 0) {
            uint32_t glyph_id = shaper_.shape_codepoint(cell.codepoint);
            update.glyph = glyph_cache_.get_glyph(glyph_id);
            if (glyph_cache_.atlas_dirty()) {
                atlas_updated = true;
            }
        }

        updates.push_back(update);
    }

    // Upload atlas if glyphs were rasterized
    if (atlas_updated || atlas_needs_full_upload_) {
        renderer_.set_atlas_texture(glyph_cache_.atlas_data(),
            glyph_cache_.atlas_width(), glyph_cache_.atlas_height());
        glyph_cache_.clear_dirty();
        atlas_needs_full_upload_ = false;
    }

    renderer_.update_cells(updates);
    grid_.clear_dirty();
}

void App::on_resize(int pixel_w, int pixel_h) {
    renderer_.resize(pixel_w, pixel_h);

    auto [cell_w, cell_h] = renderer_.cell_size_pixels();
    int pad = renderer_.padding();
    int new_cols = (pixel_w - pad * 2) / cell_w;
    int new_rows = (pixel_h - pad * 2) / cell_h;

    if (new_cols < 1) new_cols = 1;
    if (new_rows < 1) new_rows = 1;

    if (new_cols != grid_cols_ || new_rows != grid_rows_) {
        // Tell nvim about the new grid size
        rpc_.request("nvim_ui_try_resize", {
            NvimRpc::make_int(new_cols),
            NvimRpc::make_int(new_rows)
        });
    }
}

void App::change_font_size(int new_size) {
    new_size = std::max(MIN_FONT_SIZE, std::min(MAX_FONT_SIZE, new_size));
    if (new_size == font_size_) return;
    font_size_ = new_size;

    // Update font
    font_.set_point_size(font_size_);
    auto& metrics = font_.metrics();

    // Reset glyph cache and shaper for new size
    glyph_cache_.reset(font_.face(), font_.point_size());
    shaper_.initialize(font_.hb_font());

    // Update renderer metrics
    renderer_.set_cell_size(metrics.cell_width, metrics.cell_height);
    renderer_.set_ascender(metrics.ascender);
    input_.set_cell_size(metrics.cell_width, metrics.cell_height);

    // Recalculate grid
    auto [pw, ph] = window_.size_pixels();
    int pad = renderer_.padding();
    int new_cols = (pw - pad * 2) / metrics.cell_width;
    int new_rows = (ph - pad * 2) / metrics.cell_height;
    if (new_cols < 1) new_cols = 1;
    if (new_rows < 1) new_rows = 1;

    // Upload fresh atlas
    atlas_needs_full_upload_ = true;

    // Tell nvim about the new grid size
    rpc_.request("nvim_ui_try_resize", {
        NvimRpc::make_int(new_cols),
        NvimRpc::make_int(new_rows)
    });
}

void App::shutdown() {
    // Graceful nvim shutdown
    if (nvim_process_.is_running()) {
        rpc_.notify("nvim_input", { NvimRpc::make_str("<C-\\><C-n>:qa!<CR>") });
    }
    rpc_.shutdown();
    nvim_process_.shutdown();

    font_.shutdown();
    renderer_.shutdown();
    window_.shutdown();
}

} // namespace spectre
