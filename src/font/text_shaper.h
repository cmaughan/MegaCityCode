#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <hb.h>

namespace spectre {

struct ShapedGlyph {
    uint32_t glyph_id;
    int x_advance;  // In pixels (already shifted by 6)
    int x_offset;
    int y_offset;
    int cluster;    // Index into original string
};

class TextShaper {
public:
    void initialize(hb_font_t* font);

    // Shape a text run and return glyph IDs + positioning
    std::vector<ShapedGlyph> shape(const std::string& text);

    // Shape a single codepoint (fast path for simple grid rendering)
    uint32_t shape_codepoint(uint32_t codepoint);

private:
    hb_font_t* font_ = nullptr;
    hb_buffer_t* buffer_ = nullptr;
};

} // namespace spectre
