#include "renderer_state.h"

#include <algorithm>
#include <cstring>

namespace spectre
{

void RendererState::set_grid_size(int cols, int rows, int padding)
{
    grid_cols_ = cols;
    grid_rows_ = rows;
    padding_ = padding;
    cursor_applied_ = false;
    cursor_overlay_active_ = false;

    gpu_cells_.resize((size_t)cols * rows);
    relayout();
}

void RendererState::set_cell_size(int w, int h)
{
    cell_w_ = w;
    cell_h_ = h;
    relayout();
}

void RendererState::set_ascender(int a)
{
    ascender_ = a;
}

void RendererState::relayout()
{
    std::fill(gpu_cells_.begin(), gpu_cells_.end(), GpuCell{});

    for (int r = 0; r < grid_rows_; r++)
    {
        for (int c = 0; c < grid_cols_; c++)
        {
            auto& cell = gpu_cells_[(size_t)r * grid_cols_ + c];
            cell.pos_x = (float)(c * cell_w_ + padding_);
            cell.pos_y = (float)(r * cell_h_ + padding_);
            cell.size_x = (float)cell_w_;
            cell.size_y = (float)cell_h_;
            cell.bg_r = 0.1f;
            cell.bg_g = 0.1f;
            cell.bg_b = 0.1f;
            cell.bg_a = 1.0f;
            cell.fg_r = 1.0f;
            cell.fg_g = 1.0f;
            cell.fg_b = 1.0f;
            cell.fg_a = 1.0f;
        }
    }
}

void RendererState::update_cells(std::span<const CellUpdate> updates)
{
    restore_cursor();

    for (const auto& u : updates)
    {
        if (u.col < 0 || u.col >= grid_cols_ || u.row < 0 || u.row >= grid_rows_)
            continue;

        auto& cell = gpu_cells_[(size_t)u.row * grid_cols_ + u.col];
        cell.bg_r = u.bg.r;
        cell.bg_g = u.bg.g;
        cell.bg_b = u.bg.b;
        cell.bg_a = u.bg.a;
        cell.fg_r = u.fg.r;
        cell.fg_g = u.fg.g;
        cell.fg_b = u.fg.b;
        cell.fg_a = u.fg.a;
        cell.uv_x0 = u.glyph.u0;
        cell.uv_y0 = u.glyph.v0;
        cell.uv_x1 = u.glyph.u1;
        cell.uv_y1 = u.glyph.v1;
        cell.glyph_offset_x = (float)u.glyph.bearing_x;
        cell.glyph_offset_y = (float)(cell_h_ - ascender_ + u.glyph.bearing_y);
        cell.glyph_size_x = (float)u.glyph.width;
        cell.glyph_size_y = (float)u.glyph.height;
        cell.style_flags = u.style_flags;
    }
}

void RendererState::set_cursor(int col, int row, const CursorStyle& style)
{
    restore_cursor();
    cursor_col_ = col;
    cursor_row_ = row;
    cursor_style_ = style;
}

void RendererState::restore_cursor()
{
    if (!cursor_applied_)
        return;

    cursor_applied_ = false;
    cursor_overlay_active_ = false;

    if (cursor_col_ < 0 || cursor_col_ >= grid_cols_ || cursor_row_ < 0 || cursor_row_ >= grid_rows_)
        return;

    int idx = cursor_row_ * grid_cols_ + cursor_col_;
    gpu_cells_[(size_t)idx] = cursor_saved_cell_;
}

void RendererState::apply_cursor()
{
    if (cursor_col_ < 0 || cursor_col_ >= grid_cols_ || cursor_row_ < 0 || cursor_row_ >= grid_rows_)
        return;

    int idx = cursor_row_ * grid_cols_ + cursor_col_;
    auto& cell = gpu_cells_[(size_t)idx];

    cursor_saved_cell_ = cell;
    cursor_applied_ = true;
    cursor_overlay_active_ = false;

    if (cursor_style_.shape == CursorShape::Block)
    {
        if (cursor_style_.use_explicit_colors)
        {
            cell.fg_r = cursor_style_.fg.r;
            cell.fg_g = cursor_style_.fg.g;
            cell.fg_b = cursor_style_.fg.b;
            cell.fg_a = cursor_style_.fg.a;
            cell.bg_r = cursor_style_.bg.r;
            cell.bg_g = cursor_style_.bg.g;
            cell.bg_b = cursor_style_.bg.b;
            cell.bg_a = cursor_style_.bg.a;
        }
        else
        {
            std::swap(cell.fg_r, cell.bg_r);
            std::swap(cell.fg_g, cell.bg_g);
            std::swap(cell.fg_b, cell.bg_b);
            std::swap(cell.fg_a, cell.bg_a);
        }
        return;
    }

    overlay_cell_ = {};
    overlay_cell_.bg_r = cursor_style_.bg.r;
    overlay_cell_.bg_g = cursor_style_.bg.g;
    overlay_cell_.bg_b = cursor_style_.bg.b;
    overlay_cell_.bg_a = cursor_style_.bg.a;

    int percentage = cursor_style_.cell_percentage;
    if (percentage <= 0)
        percentage = (cursor_style_.shape == CursorShape::Vertical) ? 25 : 20;

    if (cursor_style_.shape == CursorShape::Vertical)
    {
        overlay_cell_.pos_x = cell.pos_x;
        overlay_cell_.pos_y = cell.pos_y;
        overlay_cell_.size_x = std::max(1.0f, cell.size_x * percentage / 100.0f);
        overlay_cell_.size_y = cell.size_y;
    }
    else
    {
        overlay_cell_.pos_x = cell.pos_x;
        overlay_cell_.size_y = std::max(1.0f, cell.size_y * percentage / 100.0f);
        overlay_cell_.pos_y = cell.pos_y + cell.size_y - overlay_cell_.size_y;
        overlay_cell_.size_x = cell.size_x;
    }

    cursor_overlay_active_ = true;
}

void RendererState::copy_to(void* dst) const
{
    auto* bytes = static_cast<std::byte*>(dst);
    if (!gpu_cells_.empty())
    {
        std::memcpy(bytes, gpu_cells_.data(), gpu_cells_.size() * sizeof(GpuCell));
    }

    GpuCell overlay = cursor_overlay_active_ ? overlay_cell_ : GpuCell{};
    std::memcpy(bytes + gpu_cells_.size() * sizeof(GpuCell), &overlay, sizeof(GpuCell));
}

} // namespace spectre
