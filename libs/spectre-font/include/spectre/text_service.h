#pragma once

#include <spectre/font.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace spectre
{

class TextService
{
public:
    static constexpr int DEFAULT_POINT_SIZE = 11;
    static constexpr int MIN_POINT_SIZE = 6;
    static constexpr int MAX_POINT_SIZE = 36;

    bool initialize(int point_size, float display_ppi);
    void shutdown();

    bool set_point_size(int point_size);
    int point_size() const;
    const FontMetrics& metrics() const;
    const std::string& primary_font_path() const;

    AtlasRegion resolve_cluster(const std::string& text);

    bool atlas_dirty() const;
    void clear_atlas_dirty();
    const uint8_t* atlas_data() const;
    int atlas_width() const;
    int atlas_height() const;
    const GlyphCache::DirtyRect& atlas_dirty_rect() const;

private:
    struct FallbackFont
    {
        FontManager font;
        TextShaper shaper;
        std::string path;
    };

    void initialize_fallback_fonts();
    std::pair<FT_Face, TextShaper*> resolve_font_for_text(const std::string& text);

    std::string font_path_;
    float display_ppi_ = 96.0f;
    FontManager font_;
    GlyphCache glyph_cache_;
    TextShaper shaper_;
    std::vector<FallbackFont> fallback_fonts_;
    std::unordered_map<std::string, int> font_choice_cache_;
};

} // namespace spectre
