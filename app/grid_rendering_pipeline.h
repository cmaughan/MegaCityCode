#pragma once

#include <spectre/grid.h>
#include <spectre/renderer.h>
#include <spectre/text_service.h>
#include <vector>

namespace spectre
{

class GridRenderingPipeline
{
public:
    GridRenderingPipeline(Grid& grid, HighlightTable& highlights, TextService& text_service);
    void set_renderer(IRenderer* renderer);
    void flush();
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
