#pragma once
#include <cstdint>
#include <vector>

namespace spectre
{

struct Color
{
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

    bool operator==(const Color&) const = default;

    static Color from_rgb(uint32_t rgb)
    {
        return {
            ((rgb >> 16) & 0xFF) / 255.0f,
            ((rgb >> 8) & 0xFF) / 255.0f,
            (rgb & 0xFF) / 255.0f,
            1.0f
        };
    }

    static Color from_rgba(uint32_t rgba)
    {
        return {
            ((rgba >> 24) & 0xFF) / 255.0f,
            ((rgba >> 16) & 0xFF) / 255.0f,
            ((rgba >> 8) & 0xFF) / 255.0f,
            (rgba & 0xFF) / 255.0f
        };
    }
};

struct AtlasRegion
{
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0; // UV coordinates in atlas
    int bearing_x = 0, bearing_y = 0; // Glyph bearing from baseline
    int width = 0, height = 0; // Pixel dimensions
    bool is_color = false;
};

enum class AmbiWidth
{
    Single,
    Double
};

struct UiOptions
{
    AmbiWidth ambiwidth = AmbiWidth::Single;
};

enum class CursorShape
{
    Block,
    Horizontal,
    Vertical
};

struct CursorStyle
{
    CursorShape shape = CursorShape::Block;
    Color fg = { 0.0f, 0.0f, 0.0f, 1.0f };
    Color bg = { 1.0f, 1.0f, 1.0f, 1.0f };
    int cell_percentage = 0;
    bool use_explicit_colors = false;
};

inline constexpr uint32_t STYLE_FLAG_BOLD = 1u << 0;
inline constexpr uint32_t STYLE_FLAG_ITALIC = 1u << 1;
inline constexpr uint32_t STYLE_FLAG_UNDERLINE = 1u << 2;
inline constexpr uint32_t STYLE_FLAG_STRIKETHROUGH = 1u << 3;
inline constexpr uint32_t STYLE_FLAG_UNDERCURL = 1u << 4;
inline constexpr uint32_t STYLE_FLAG_COLOR_GLYPH = 1u << 5;

// Data sent to the GPU per cell (matches SSBO layout)
struct alignas(16) GpuCell
{
    float pos_x, pos_y; // Screen position in pixels
    float size_x, size_y; // Cell size in pixels
    float bg_r, bg_g, bg_b, bg_a;
    float fg_r, fg_g, fg_b, fg_a;
    float sp_r, sp_g, sp_b, sp_a;
    float uv_x0, uv_y0, uv_x1, uv_y1; // Atlas UVs
    float glyph_offset_x, glyph_offset_y;
    float glyph_size_x, glyph_size_y;
    uint32_t style_flags;
    uint32_t _pad[3];
};
static_assert(sizeof(GpuCell) == 112, "GpuCell must be 112 bytes for SSBO alignment");

struct CellUpdate
{
    int col, row;
    Color bg, fg;
    Color sp;
    AtlasRegion glyph;
    uint32_t style_flags = 0;
};

struct CapturedFrame
{
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;

    bool valid() const
    {
        return width > 0 && height > 0 && rgba.size() == static_cast<size_t>(width * height * 4);
    }
};

} // namespace spectre
