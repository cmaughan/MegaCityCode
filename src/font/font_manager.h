#pragma once
#include <string>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

namespace spectre {

struct FontMetrics {
    int cell_width;    // Monospace advance width
    int cell_height;   // Line height
    int ascender;      // Pixels above baseline
    int descender;     // Pixels below baseline (positive = down)
};

class FontManager {
public:
    bool initialize(const std::string& font_path, int point_size, float display_ppi);
    bool set_point_size(int point_size);
    void shutdown();

    FT_Face face() const { return face_; }
    hb_font_t* hb_font() const { return hb_font_; }
    const FontMetrics& metrics() const { return metrics_; }
    int point_size() const { return point_size_; }

private:
    FT_Library ft_lib_ = nullptr;
    FT_Face face_ = nullptr;
    hb_font_t* hb_font_ = nullptr;
    void update_metrics();

    FontMetrics metrics_ = {};
    int point_size_ = 0;
    float display_ppi_ = 96.0f;
};

} // namespace spectre
