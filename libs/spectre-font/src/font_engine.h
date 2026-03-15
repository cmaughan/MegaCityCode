#pragma once

#include <cstdint>
#include <spectre/font_metrics.h>
#include <spectre/types.h>
#include <string>
#include <unordered_map>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>

namespace spectre
{

class TextShaper;

class FontManager
{
public:
    FontManager() = default;
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;
    FontManager(FontManager&& other) noexcept;
    FontManager& operator=(FontManager&& other) noexcept;

    bool initialize(const std::string& font_path, int point_size, float display_ppi);
    bool set_point_size(int point_size);
    void shutdown();

    FT_Face face() const
    {
        return face_;
    }
    hb_font_t* hb_font() const
    {
        return hb_font_;
    }
    const FontMetrics& metrics() const
    {
        return metrics_;
    }
    int point_size() const
    {
        return point_size_;
    }

private:
    void update_metrics();

    FT_Library ft_lib_ = nullptr;
    FT_Face face_ = nullptr;
    hb_font_t* hb_font_ = nullptr;
    FontMetrics metrics_ = {};
    int point_size_ = 0;
    float display_ppi_ = 96.0f;
};

struct ShapedGlyph
{
    uint32_t glyph_id = 0;
    int x_advance = 0;
    int x_offset = 0;
    int y_offset = 0;
    int cluster = 0;
};

class TextShaper
{
public:
    TextShaper() = default;
    TextShaper(const TextShaper&) = delete;
    TextShaper& operator=(const TextShaper&) = delete;
    TextShaper(TextShaper&& other) noexcept;
    TextShaper& operator=(TextShaper&& other) noexcept;
    ~TextShaper();

    void initialize(hb_font_t* font);
    void shutdown();

    std::vector<ShapedGlyph> shape(const std::string& text);

private:
    hb_font_t* font_ = nullptr;
    hb_buffer_t* buffer_ = nullptr;
};

class GlyphCache
{
public:
    struct DirtyRect
    {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
    };

    bool initialize(FT_Face face, int pixel_size, int atlas_size = GLYPH_ATLAS_SIZE);
    void reset(FT_Face face, int pixel_size);

    const AtlasRegion& get_cluster(const std::string& text, FT_Face face, TextShaper& shaper);

    bool atlas_dirty() const
    {
        return dirty_;
    }
    void clear_dirty()
    {
        dirty_ = false;
        dirty_rect_ = {};
    }

    const uint8_t* atlas_data() const
    {
        return atlas_.data();
    }
    int atlas_width() const
    {
        return atlas_size_;
    }
    int atlas_height() const
    {
        return atlas_size_;
    }
    const DirtyRect& dirty_rect() const
    {
        return dirty_rect_;
    }

    bool consume_overflowed()
    {
        bool overflowed = overflowed_;
        overflowed_ = false;
        return overflowed;
    }

private:
    struct ClusterKey
    {
        FT_Face face = nullptr;
        std::string text;

        bool operator==(const ClusterKey&) const = default;
    };

    struct ClusterKeyHash
    {
        size_t operator()(const ClusterKey& key) const;
    };

    bool rasterize_cluster(const std::string& text, FT_Face face, TextShaper& shaper, AtlasRegion& region);
    bool reserve_region(int w, int h, int& atlas_x, int& atlas_y, const char* label);

    FT_Face face_ = nullptr;
    int pixel_size_ = 0;
    int atlas_size_ = GLYPH_ATLAS_SIZE;

    std::vector<uint8_t> atlas_;
    std::unordered_map<ClusterKey, AtlasRegion, ClusterKeyHash> cluster_cache_;

    int shelf_x_ = 0;
    int shelf_y_ = 0;
    int shelf_height_ = 0;

    bool dirty_ = false;
    DirtyRect dirty_rect_ = {};
    bool overflowed_ = false;
    AtlasRegion empty_region_ = {};
};

} // namespace spectre
