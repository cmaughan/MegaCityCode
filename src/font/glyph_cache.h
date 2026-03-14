#pragma once
#include "renderer/types.h"
#include <unordered_map>
#include <vector>
#include <cstdint>

struct FT_FaceRec_;
typedef struct FT_FaceRec_* FT_Face;

namespace spectre {

class GlyphCache {
public:
    static constexpr int ATLAS_SIZE = 2048;

    bool initialize(FT_Face face, int pixel_size);
    void reset(FT_Face face, int pixel_size);

    // Get atlas region for a glyph. Rasterizes on cache miss.
    const AtlasRegion& get_glyph(uint32_t glyph_id);

    // Check if atlas was updated since last call
    bool atlas_dirty() const { return dirty_; }
    void clear_dirty() { dirty_ = false; }

    // Atlas data
    const uint8_t* atlas_data() const { return atlas_.data(); }
    int atlas_width() const { return ATLAS_SIZE; }
    int atlas_height() const { return ATLAS_SIZE; }

    // Get the region that was most recently updated (for incremental upload)
    struct DirtyRect { int x, y, w, h; };
    const DirtyRect& dirty_rect() const { return dirty_rect_; }

private:
    bool rasterize_glyph(uint32_t glyph_id, AtlasRegion& region);

    FT_Face face_ = nullptr;
    int pixel_size_ = 0;

    std::vector<uint8_t> atlas_;
    std::unordered_map<uint32_t, AtlasRegion> cache_;

    // Shelf packing state
    int shelf_x_ = 0;      // Current x position in current shelf
    int shelf_y_ = 0;      // Current shelf y position
    int shelf_height_ = 0;  // Max height in current shelf

    bool dirty_ = false;
    DirtyRect dirty_rect_ = {};

    AtlasRegion empty_region_ = {};
};

} // namespace spectre
