#include "font_engine.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <climits>
#include <cstring>
#include <spectre/log.h>

namespace spectre
{

namespace
{

struct RasterizedGlyph
{
    int width = 0;
    int height = 0;
    int left = 0;
    int top = 0;
    std::vector<uint8_t> pixels;
};

const uint8_t* bitmap_row_ptr(const FT_Bitmap& bmp, int row)
{
    if (bmp.pitch >= 0)
        return bmp.buffer + row * bmp.pitch;
    return bmp.buffer + (bmp.rows - 1 - row) * (-bmp.pitch);
}

bool bitmap_to_grayscale(const FT_Bitmap& bmp, std::vector<uint8_t>& out)
{
    int width = (int)bmp.width;
    int height = (int)bmp.rows;
    if (width <= 0 || height <= 0)
    {
        out.clear();
        return true;
    }

    out.assign((size_t)width * height, 0);

    switch (bmp.pixel_mode)
    {
    case FT_PIXEL_MODE_GRAY:
        for (int row = 0; row < height; row++)
        {
            const uint8_t* src = bitmap_row_ptr(bmp, row);
            memcpy(out.data() + (size_t)row * width, src, width);
        }
        return true;

    case FT_PIXEL_MODE_MONO:
        for (int row = 0; row < height; row++)
        {
            const uint8_t* src = bitmap_row_ptr(bmp, row);
            for (int col = 0; col < width; col++)
            {
                uint8_t mask = (uint8_t)(0x80 >> (col & 7));
                out[(size_t)row * width + col] = (src[col >> 3] & mask) ? 255 : 0;
            }
        }
        return true;

    case FT_PIXEL_MODE_BGRA:
        for (int row = 0; row < height; row++)
        {
            const uint8_t* src = bitmap_row_ptr(bmp, row);
            for (int col = 0; col < width; col++)
            {
                out[(size_t)row * width + col] = src[col * 4 + 3];
            }
        }
        return true;
    }

    SPECTRE_LOG_WARN(LogCategory::Font, "Unsupported FreeType pixel mode: %u", bmp.pixel_mode);
    return false;
}

void expand_dirty_rect(GlyphCache::DirtyRect& dirty_rect, bool dirty, int x, int y, int w, int h)
{
    if (!dirty)
    {
        dirty_rect = { x, y, w, h };
        return;
    }

    int left = std::min(dirty_rect.x, x);
    int top = std::min(dirty_rect.y, y);
    int right = std::max(dirty_rect.x + dirty_rect.w, x + w);
    int bottom = std::max(dirty_rect.y + dirty_rect.h, y + h);
    dirty_rect = { left, top, right - left, bottom - top };
}

} // namespace

bool GlyphCache::initialize(FT_Face face, int pixel_size, int atlas_size)
{
    face_ = face;
    pixel_size_ = pixel_size;
    atlas_size_ = std::max(1, atlas_size);
    atlas_.assign((size_t)atlas_size_ * atlas_size_, 0);
    dirty_ = false;
    dirty_rect_ = {};
    overflowed_ = false;
    shelf_x_ = 0;
    shelf_y_ = 0;
    shelf_height_ = 0;
    cluster_cache_.clear();
    return true;
}

void GlyphCache::reset(FT_Face face, int pixel_size)
{
    face_ = face;
    pixel_size_ = pixel_size;
    cluster_cache_.clear();
    std::fill(atlas_.begin(), atlas_.end(), (uint8_t)0);
    shelf_x_ = 0;
    shelf_y_ = 0;
    shelf_height_ = 0;
    dirty_ = true;
    dirty_rect_ = { 0, 0, atlas_size_, atlas_size_ };
    overflowed_ = false;
}

size_t GlyphCache::ClusterKeyHash::operator()(const ClusterKey& key) const
{
    size_t h1 = std::hash<FT_Face>()(key.face);
    size_t h2 = std::hash<std::string>()(key.text);
    return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
}

const AtlasRegion& GlyphCache::get_cluster(const std::string& text, FT_Face face, TextShaper& shaper)
{
    if (text.empty() || text == " ")
        return empty_region_;

    ClusterKey key = { face, text };
    auto it = cluster_cache_.find(key);
    if (it != cluster_cache_.end())
        return it->second;

    AtlasRegion region = {};
    if (rasterize_cluster(text, face, shaper, region))
    {
        auto [ins, _] = cluster_cache_.emplace(std::move(key), region);
        return ins->second;
    }

    return empty_region_;
}

bool GlyphCache::reserve_region(int w, int h, int& atlas_x, int& atlas_y, const char* label)
{
    if (w <= 0 || h <= 0)
        return false;

    if (shelf_x_ + w + 1 > atlas_size_)
    {
        shelf_y_ += shelf_height_ + 1;
        shelf_x_ = 0;
        shelf_height_ = 0;
    }

    if (shelf_y_ + h > atlas_size_)
    {
        SPECTRE_LOG_WARN(LogCategory::Font, "Atlas full, cannot fit %s (%dx%d)", label, w, h);
        overflowed_ = true;
        return false;
    }

    atlas_x = shelf_x_;
    atlas_y = shelf_y_;
    shelf_x_ += w + 1;
    shelf_height_ = std::max(shelf_height_, h);
    return true;
}

bool GlyphCache::rasterize_cluster(const std::string& text, FT_Face face, TextShaper& shaper, AtlasRegion& region)
{
    auto shaped = shaper.shape(text);
    if (shaped.empty())
    {
        region = {};
        return true;
    }

    std::vector<RasterizedGlyph> glyphs;
    glyphs.reserve(shaped.size());

    int pen_x = 0;
    int bbox_left = INT_MAX;
    int bbox_right = INT_MIN;
    int bbox_top = INT_MIN;
    int bbox_bottom = INT_MAX;

    for (const auto& shaped_glyph : shaped)
    {
        if (FT_Load_Glyph(face, shaped_glyph.glyph_id, FT_LOAD_DEFAULT | FT_LOAD_COLOR))
            return false;
        if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP
            && FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL))
            return false;

        FT_Bitmap& bmp = face->glyph->bitmap;
        if (bmp.width > 0 && bmp.rows > 0)
        {
            RasterizedGlyph glyph;
            glyph.width = (int)bmp.width;
            glyph.height = (int)bmp.rows;
            glyph.left = pen_x + shaped_glyph.x_offset + face->glyph->bitmap_left;
            glyph.top = shaped_glyph.y_offset + face->glyph->bitmap_top;
            if (!bitmap_to_grayscale(bmp, glyph.pixels))
                return false;

            bbox_left = std::min(bbox_left, glyph.left);
            bbox_right = std::max(bbox_right, glyph.left + glyph.width);
            bbox_top = std::max(bbox_top, glyph.top);
            bbox_bottom = std::min(bbox_bottom, glyph.top - glyph.height);
            glyphs.push_back(std::move(glyph));
        }

        pen_x += shaped_glyph.x_advance;
    }

    if (glyphs.empty())
    {
        region = {};
        return true;
    }

    int cluster_width = bbox_right - bbox_left;
    int cluster_height = bbox_top - bbox_bottom;

    int atlas_x = 0;
    int atlas_y = 0;
    if (!reserve_region(cluster_width, cluster_height, atlas_x, atlas_y, "cluster"))
        return false;

    std::vector<uint8_t> composite(cluster_width * cluster_height, 0);
    for (const auto& glyph : glyphs)
    {
        int dst_x = glyph.left - bbox_left;
        int dst_y = bbox_top - glyph.top;
        for (int row = 0; row < glyph.height; row++)
        {
            for (int col = 0; col < glyph.width; col++)
            {
                auto& dst = composite[(dst_y + row) * cluster_width + dst_x + col];
                dst = std::max(dst, glyph.pixels[row * glyph.width + col]);
            }
        }
    }

    for (int row = 0; row < cluster_height; row++)
    {
        memcpy(atlas_.data() + (size_t)(atlas_y + row) * atlas_size_ + atlas_x,
            composite.data() + row * cluster_width,
            cluster_width);
    }

    float inv_size = 1.0f / atlas_size_;
    region.u0 = atlas_x * inv_size;
    region.v0 = atlas_y * inv_size;
    region.u1 = (atlas_x + cluster_width) * inv_size;
    region.v1 = (atlas_y + cluster_height) * inv_size;
    region.bearing_x = bbox_left;
    region.bearing_y = bbox_top;
    region.width = cluster_width;
    region.height = cluster_height;

    expand_dirty_rect(dirty_rect_, dirty_, atlas_x, atlas_y, cluster_width, cluster_height);
    dirty_ = true;
    return true;
}

} // namespace spectre
