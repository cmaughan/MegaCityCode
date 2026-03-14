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
    void initialize(IRenderer* renderer, Grid* grid, HighlightTable* highlights, TextService* text_service);
    void flush();
    void force_full_atlas_upload();

private:
    void upload_atlas();

    IRenderer* renderer_ = nullptr;
    Grid* grid_ = nullptr;
    HighlightTable* highlights_ = nullptr;
    TextService* text_service_ = nullptr;

    bool force_full_atlas_upload_ = true;
    std::vector<uint8_t> atlas_upload_scratch_;
};

} // namespace spectre
