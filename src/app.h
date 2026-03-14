#pragma once
#include "window/sdl_window.h"
#ifdef __APPLE__
#include "renderer/metal/metal_renderer.h"
#else
#include "renderer/vulkan/vk_renderer.h"
#endif
#include "font/font_manager.h"
#include "font/glyph_cache.h"
#include "font/text_shaper.h"
#include "grid/grid.h"
#include "grid/highlight.h"
#include "nvim/nvim_process.h"
#include "nvim/rpc.h"
#include "nvim/ui_events.h"
#include "nvim/input.h"
#include <memory>

namespace spectre {

class App {
public:
    bool initialize();
    void run();
    void shutdown();

private:
    void on_flush();
    void on_resize(int pixel_w, int pixel_h);
    void update_grid_to_renderer();
    void change_font_size(int new_size);

    SdlWindow window_;
#ifdef __APPLE__
    MetalRenderer renderer_;
#else
    VkRenderer renderer_;
#endif
    FontManager font_;
    GlyphCache glyph_cache_;
    TextShaper shaper_;
    Grid grid_;
    HighlightTable highlights_;
    NvimProcess nvim_process_;
    NvimRpc rpc_;
    UiEventHandler ui_events_;
    NvimInput input_;

    static constexpr int DEFAULT_FONT_SIZE = 18; // points (physical size)
    static constexpr int MIN_FONT_SIZE = 6;
    static constexpr int MAX_FONT_SIZE = 36;
    std::string font_path_;
    int font_size_ = DEFAULT_FONT_SIZE;
    float display_ppi_ = 96.0f;

    bool atlas_needs_full_upload_ = true;
    bool running_ = false;
    int grid_cols_ = 0, grid_rows_ = 0;
    int flush_count_ = 0;
};

} // namespace spectre
