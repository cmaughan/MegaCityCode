#pragma once
#include "grid_rendering_pipeline.h"
#include <spectre/sdl_window.h>
#ifdef __APPLE__
// Metal renderer header is internal to spectre-renderer, forward declare
#else
// Vulkan renderer header is internal to spectre-renderer, forward declare
#endif
#include <memory>
#include <spectre/grid.h>
#include <spectre/nvim.h>
#include <spectre/renderer.h>
#include <spectre/text_service.h>

namespace spectre
{

class App
{
public:
    bool initialize();
    void run();
    void shutdown();

private:
    void on_flush();
    void on_resize(int pixel_w, int pixel_h);
    void change_font_size(int new_size);

    SdlWindow window_;
    std::unique_ptr<IRenderer> renderer_;
    TextService text_service_;
    Grid grid_;
    HighlightTable highlights_;
    NvimProcess nvim_process_;
    NvimRpc rpc_;
    UiEventHandler ui_events_;
    NvimInput input_;
    GridRenderingPipeline grid_pipeline_;
    UiOptions ui_options_;

    bool running_ = false;
    bool pending_window_activation_ = true;
    int grid_cols_ = 0, grid_rows_ = 0;
};

} // namespace spectre
