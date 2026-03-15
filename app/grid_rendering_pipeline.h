#pragma once

#include <cstddef>
#include <spectre/grid.h>
#include <spectre/renderer.h>
#include <spectre/text_service.h>
#include <string>
#include <vector>

namespace spectre
{

struct DebugOverlayState
{
    bool enabled = false;
    float display_ppi = 0.0f;
    int cell_width = 0;
    int cell_height = 0;
    int grid_cols = 0;
    int grid_rows = 0;
    size_t dirty_cells = 0;
    double frame_ms = 0.0;
    double average_frame_ms = 0.0;
    float atlas_usage_ratio = 0.0f;
    size_t atlas_glyph_count = 0;
    int atlas_reset_count = 0;
};

class GridRenderingPipeline
{
public:
    GridRenderingPipeline(Grid& grid, HighlightTable& highlights, TextService& text_service);
    void set_renderer(IRenderer* renderer);
    void flush();
    void set_debug_overlay(const DebugOverlayState& overlay);
    void force_full_atlas_upload();

private:
    void upload_atlas();

    IRenderer* renderer_ = nullptr;
    Grid& grid_;
    HighlightTable& highlights_;
    TextService& text_service_;

    bool force_full_atlas_upload_ = true;
    std::vector<uint8_t> atlas_upload_scratch_;
};

} // namespace spectre
