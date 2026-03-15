#include "grid_rendering_pipeline.h"

#include <cstring>

namespace spectre
{

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
