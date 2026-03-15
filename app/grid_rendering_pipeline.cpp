#include "grid_rendering_pipeline.h"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace spectre
{

namespace
{

constexpr Color kOverlayBackground = { 0.03f, 0.05f, 0.08f, 0.88f };
constexpr Color kOverlayForeground = { 0.95f, 0.98f, 1.0f, 1.0f };

std::string format_line(const char* fmt, ...)
{
    std::array<char, 128> buffer = {};
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer.data(), buffer.size(), fmt, args);
    va_end(args);
    return std::string(buffer.data());
}

std::vector<std::string> build_overlay_lines(const DebugOverlayState& overlay)
{
    return {
        format_line("DEBUG  PPI %.0f  Cell %dx%d  Grid %dx%d",
            overlay.display_ppi, overlay.cell_width, overlay.cell_height, overlay.grid_cols, overlay.grid_rows),
        format_line("Frame %.2f ms  Avg %.2f ms  Dirty %zu",
            overlay.frame_ms, overlay.average_frame_ms, overlay.dirty_cells),
        format_line("Atlas %.1f%%  Glyphs %zu  Resets %d",
            overlay.atlas_usage_ratio * 100.0f, overlay.atlas_glyph_count, overlay.atlas_reset_count),
    };
}

} // namespace

GridRenderingPipeline::GridRenderingPipeline(Grid& grid, HighlightTable& highlights, TextService& text_service)
    : grid_(grid)
    , highlights_(highlights)
    , text_service_(text_service)
{
}

void GridRenderingPipeline::set_renderer(IRenderer* renderer)
{
    renderer_ = renderer;
}

void GridRenderingPipeline::flush()
{
    if (!renderer_)
        return;

    for (int attempt = 0; attempt < 2; attempt++)
    {
        if (attempt > 0)
        {
            grid_.mark_all_dirty();
            force_full_atlas_upload_ = true;
        }

        auto dirty = grid_.get_dirty_cells();
        if (dirty.empty())
            return;

        std::vector<CellUpdate> updates;
        updates.reserve(dirty.size());

        bool atlas_updated = false;

        for (auto& [col, row] : dirty)
        {
            const auto& cell = grid_.get_cell(col, row);
            const auto& hl = highlights_.get(cell.hl_attr_id);

            Color fg;
            Color bg;
            Color sp;
            highlights_.resolve(hl, fg, bg, &sp);

            CellUpdate update;
            update.col = col;
            update.row = row;
            update.bg = bg;
            update.fg = fg;
            update.sp = sp;
            update.style_flags = hl.style_flags();

            if (!cell.double_width_cont && !cell.text.empty() && cell.text != " ")
            {
                update.glyph = text_service_.resolve_cluster(cell.text);
                if (update.glyph.is_color)
                    update.style_flags |= STYLE_FLAG_COLOR_GLYPH;
                atlas_updated = atlas_updated || text_service_.atlas_dirty();
            }

            updates.push_back(update);
        }

        bool atlas_reset = text_service_.consume_atlas_reset();
        if (atlas_reset)
            continue;

        if (force_full_atlas_upload_ || atlas_updated)
            upload_atlas();

        renderer_->update_cells(updates);
        grid_.clear_dirty();
        return;
    }
}

void GridRenderingPipeline::set_debug_overlay(const DebugOverlayState& overlay)
{
    if (!renderer_)
        return;

    if (!overlay.enabled || overlay.grid_cols <= 2 || overlay.grid_rows <= 0)
    {
        renderer_->set_overlay_cells(std::span<const CellUpdate>());
        return;
    }

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        auto lines = build_overlay_lines(overlay);
        const int available_width = std::max(1, overlay.grid_cols - 2);
        const int line_count = std::min((int)lines.size(), overlay.grid_rows);

        size_t max_width = 0;
        for (int i = 0; i < line_count; ++i)
        {
            if ((int)lines[(size_t)i].size() > available_width)
                lines[(size_t)i].resize((size_t)available_width);
            max_width = std::max(max_width, lines[(size_t)i].size());
        }

        if (max_width == 0 || line_count == 0)
        {
            renderer_->set_overlay_cells(std::span<const CellUpdate>());
            return;
        }

        std::vector<CellUpdate> overlay_updates;
        overlay_updates.reserve(max_width * (size_t)line_count);

        bool atlas_updated = false;
        const int start_col = std::max(0, overlay.grid_cols - (int)max_width - 1);
        for (int row = 0; row < line_count; ++row)
        {
            const auto& line = lines[(size_t)row];
            for (size_t col = 0; col < max_width; ++col)
            {
                CellUpdate update;
                update.col = start_col + (int)col;
                update.row = row;
                update.bg = kOverlayBackground;
                update.fg = kOverlayForeground;
                update.sp = kOverlayForeground;

                char ch = (col < line.size()) ? line[col] : ' ';
                if (ch != ' ')
                {
                    update.glyph = text_service_.resolve_cluster(std::string(1, ch));
                    if (update.glyph.is_color)
                        update.style_flags |= STYLE_FLAG_COLOR_GLYPH;
                    atlas_updated = atlas_updated || text_service_.atlas_dirty();
                }

                overlay_updates.push_back(update);
            }
        }

        if (text_service_.consume_atlas_reset())
        {
            grid_.mark_all_dirty();
            force_full_atlas_upload_ = true;
            flush();
            continue;
        }

        if (force_full_atlas_upload_ || atlas_updated)
            upload_atlas();

        renderer_->set_overlay_cells(overlay_updates);
        return;
    }
}

void GridRenderingPipeline::force_full_atlas_upload()
{
    force_full_atlas_upload_ = true;
}

void GridRenderingPipeline::upload_atlas()
{
    if (!text_service_.atlas_dirty() && !force_full_atlas_upload_)
        return;

    if (force_full_atlas_upload_)
    {
        renderer_->set_atlas_texture(
            text_service_.atlas_data(), text_service_.atlas_width(), text_service_.atlas_height());
    }
    else
    {
        const auto dirty = text_service_.atlas_dirty_rect();
        if (dirty.w > 0 && dirty.h > 0)
        {
            constexpr size_t atlas_pixel_size = 4;
            atlas_upload_scratch_.resize((size_t)dirty.w * dirty.h * atlas_pixel_size);

            const uint8_t* atlas = text_service_.atlas_data();
            const int atlas_width = text_service_.atlas_width();
            for (int row = 0; row < dirty.h; row++)
            {
                const uint8_t* src = atlas
                    + (((size_t)(dirty.y + row) * atlas_width) + dirty.x) * atlas_pixel_size;
                uint8_t* dst = atlas_upload_scratch_.data() + (size_t)row * dirty.w * atlas_pixel_size;
                std::memcpy(dst, src, (size_t)dirty.w * atlas_pixel_size);
            }

            renderer_->update_atlas_region(
                dirty.x, dirty.y, dirty.w, dirty.h, atlas_upload_scratch_.data());
        }
    }

    text_service_.clear_atlas_dirty();
    force_full_atlas_upload_ = false;
}

} // namespace spectre
