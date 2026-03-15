#include <spectre/text_service.h>

#include "font_engine.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <spectre/log.h>
#include <spectre/unicode.h>
#include <unordered_map>
#include <utility>

namespace spectre
{

namespace
{

std::vector<std::string> default_fallback_font_candidates()
{
#ifdef _WIN32
    const char* windir = std::getenv("WINDIR");
    std::string windows_dir = windir ? windir : "C:\\Windows";
    return {
        windows_dir + "\\Fonts\\seguiemj.ttf",
        windows_dir + "\\Fonts\\seguisym.ttf",
        windows_dir + "\\Fonts\\YuGothR.ttc",
        windows_dir + "\\Fonts\\YuGothM.ttc",
        windows_dir + "\\Fonts\\meiryo.ttc",
        windows_dir + "\\Fonts\\msgothic.ttc",
        windows_dir + "\\Fonts\\msyh.ttc",
        windows_dir + "\\Fonts\\simsun.ttc",
    };
#elif defined(__APPLE__)
    return {
        "/System/Library/Fonts/Apple Color Emoji.ttc",
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
        "/System/Library/Fonts/Hiragino Sans GB.ttc",
        "/System/Library/Fonts/Supplemental/Songti.ttc",
    };
#else
    return {
        "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",
        "/usr/share/fonts/truetype/noto/NotoEmoji-Regular.ttf",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    };
#endif
}

std::vector<std::string> default_primary_font_candidates()
{
#ifdef _WIN32
    const char* windir = std::getenv("WINDIR");
    const char* local_app_data = std::getenv("LOCALAPPDATA");
    std::string windows_dir = windir ? windir : "C:\\Windows";
    std::string local_fonts = local_app_data
        ? (std::string(local_app_data) + "\\Microsoft\\Windows\\Fonts\\")
        : "";

    std::vector<std::string> candidates;
    if (!local_fonts.empty())
    {
        candidates.push_back(local_fonts + "JetBrainsMonoNerdFont-Regular.ttf");
        candidates.push_back(local_fonts + "JetBrainsMonoNerdFontMono-Regular.ttf");
    }
    candidates.push_back(windows_dir + "\\Fonts\\JetBrainsMonoNerdFont-Regular.ttf");
    candidates.push_back(windows_dir + "\\Fonts\\JetBrainsMonoNerdFontMono-Regular.ttf");
    candidates.push_back(windows_dir + "\\Fonts\\JetBrains Mono Regular Nerd Font Complete Windows Compatible.ttf");
    candidates.push_back(windows_dir + "\\Fonts\\JetBrains Mono Regular Nerd Font Complete Mono Windows Compatible.ttf");
    candidates.push_back("fonts/JetBrainsMonoNerdFont-Regular.ttf");
    return candidates;
#else
    return { "fonts/JetBrainsMonoNerdFont-Regular.ttf" };
#endif
}

std::string first_existing_path(const std::vector<std::string>& candidates)
{
    for (const auto& path : candidates)
    {
        if (std::filesystem::exists(path))
            return path;
    }
    return {};
}

bool can_render_cluster(FT_Face face, TextShaper& shaper, const std::string& text)
{
    if (!face)
        return false;

    auto shaped = shaper.shape(text);
    if (shaped.empty())
        return false;

    bool has_glyph = false;
    for (const auto& glyph : shaped)
    {
        if (glyph.glyph_id == 0)
            return false;
        if (FT_Load_Glyph(face, glyph.glyph_id, FT_LOAD_DEFAULT))
            return false;
        has_glyph = true;
    }

    return has_glyph;
}

bool cluster_prefers_color_font(std::string_view text)
{
    size_t offset = 0;
    uint32_t base = 0;
    bool have_base = false;
    bool has_vs16 = false;
    bool has_zwj = false;
    bool has_emoji_modifier = false;
    bool has_keycap = false;

    while (offset < text.size())
    {
        uint32_t cp = 0;
        if (!utf8_decode_next(text, offset, cp))
            break;

        if (cp == 0xFE0F)
            has_vs16 = true;
        else if (cp == 0x200D)
            has_zwj = true;
        else if (cp == 0x20E3)
            has_keycap = true;
        else if (is_emoji_modifier(cp))
            has_emoji_modifier = true;

        if (!have_base && !is_width_ignorable(cp) && !is_emoji_modifier(cp))
        {
            base = cp;
            have_base = true;
        }
    }

    if (!have_base)
        return false;

    if (is_default_emoji_presentation(base) || is_regional_indicator(base))
        return true;

    if ((has_vs16 || has_zwj || has_emoji_modifier) && is_emoji_text_presentation_candidate(base))
        return true;

    if (has_keycap && is_ascii_keycap_base(base))
        return true;

    return false;
}

bool font_has_color(FT_Face face)
{
    return face && FT_HAS_COLOR(face);
}

} // namespace

struct TextService::Impl
{
    struct FallbackFont
    {
        FontManager font;
        TextShaper shaper;
        std::string path;
    };

    TextServiceConfig config;
    std::string font_path;
    float display_ppi = 96.0f;
    FontManager font;
    GlyphCache glyph_cache;
    TextShaper shaper;
    std::vector<FallbackFont> fallback_fonts;
    std::unordered_map<std::string, int> font_choice_cache;
    bool atlas_reset_pending = false;
    int atlas_reset_count = 0;

    bool initialize(int point_size, float ppi)
    {
        display_ppi = ppi;
        font_path = config.font_path.empty() ? first_existing_path(default_primary_font_candidates()) : config.font_path;
        if (font_path.empty())
            font_path = "fonts/JetBrainsMonoNerdFont-Regular.ttf";

        SPECTRE_LOG_INFO(LogCategory::Font, "Primary font path: %s", font_path.c_str());
        if (!font.initialize(font_path, point_size, display_ppi))
            return false;

        glyph_cache.initialize(font.face(), font.point_size());
        shaper.initialize(font.hb_font());
        initialize_fallback_fonts();
        atlas_reset_pending = false;
        atlas_reset_count = 0;
        return true;
    }

    void shutdown()
    {
        shaper.shutdown();
        for (auto& fallback : fallback_fonts)
        {
            fallback.shaper.shutdown();
            fallback.font.shutdown();
        }
        fallback_fonts.clear();
        font_choice_cache.clear();
        font.shutdown();
        atlas_reset_pending = false;
    }

    bool set_point_size(int point_size)
    {
        point_size = std::max(TextService::MIN_POINT_SIZE, std::min(TextService::MAX_POINT_SIZE, point_size));
        if (point_size == font.point_size())
            return true;

        if (!font.set_point_size(point_size))
            return false;

        glyph_cache.reset(font.face(), font.point_size());
        shaper.initialize(font.hb_font());
        font_choice_cache.clear();
        atlas_reset_pending = true;

        for (auto& fallback : fallback_fonts)
        {
            fallback.font.set_point_size(point_size);
            fallback.shaper.initialize(fallback.font.hb_font());
        }

        return true;
    }

    AtlasRegion resolve_cluster(const std::string& text)
    {
        auto [face, text_shaper] = resolve_font_for_text(text);
        AtlasRegion region = glyph_cache.get_cluster(text, face, *text_shaper);
        if (region.width > 0 || region.height > 0 || !glyph_cache.consume_overflowed())
            return region;

        glyph_cache.reset(font.face(), font.point_size());
        atlas_reset_pending = true;
        atlas_reset_count++;
        SPECTRE_LOG_WARN(LogCategory::Font, "Glyph atlas reset after exhaustion (count=%d)", atlas_reset_count);

        std::tie(face, text_shaper) = resolve_font_for_text(text);
        return glyph_cache.get_cluster(text, face, *text_shaper);
    }

    void initialize_fallback_fonts()
    {
        fallback_fonts.clear();
        font_choice_cache.clear();

        std::vector<std::string> candidates = config.fallback_paths.empty()
            ? default_fallback_font_candidates()
            : config.fallback_paths;

        fallback_fonts.reserve(candidates.size());
        for (const auto& path : candidates)
        {
            if (path == font_path || !std::filesystem::exists(path))
                continue;

            fallback_fonts.emplace_back();
            auto& fallback = fallback_fonts.back();
            fallback.path = path;
            if (!fallback.font.initialize(path, font.point_size(), display_ppi))
            {
                fallback.font.shutdown();
                fallback_fonts.pop_back();
                continue;
            }

            fallback.shaper.initialize(fallback.font.hb_font());
            SPECTRE_LOG_INFO(LogCategory::Font, "Fallback font loaded: %s", path.c_str());
        }
    }

    std::pair<FT_Face, TextShaper*> resolve_font_for_text(const std::string& text)
    {
        auto cached = font_choice_cache.find(text);
        if (cached != font_choice_cache.end())
        {
            if (cached->second < 0)
                return { font.face(), &shaper };

            int idx = cached->second;
            if (idx >= 0 && idx < (int)fallback_fonts.size())
                return { fallback_fonts[(size_t)idx].font.face(), &fallback_fonts[(size_t)idx].shaper };
        }

        if (cluster_prefers_color_font(text))
        {
            if (font_has_color(font.face()) && can_render_cluster(font.face(), shaper, text))
            {
                font_choice_cache[text] = -1;
                return { font.face(), &shaper };
            }

            for (int i = 0; i < (int)fallback_fonts.size(); i++)
            {
                auto& fallback = fallback_fonts[(size_t)i];
                if (font_has_color(fallback.font.face())
                    && can_render_cluster(fallback.font.face(), fallback.shaper, text))
                {
                    font_choice_cache[text] = i;
                    return { fallback.font.face(), &fallback.shaper };
                }
            }
        }

        if (can_render_cluster(font.face(), shaper, text))
        {
            font_choice_cache[text] = -1;
            return { font.face(), &shaper };
        }

        for (int i = 0; i < (int)fallback_fonts.size(); i++)
        {
            if (can_render_cluster(fallback_fonts[(size_t)i].font.face(), fallback_fonts[(size_t)i].shaper, text))
            {
                font_choice_cache[text] = i;
                return { fallback_fonts[(size_t)i].font.face(), &fallback_fonts[(size_t)i].shaper };
            }
        }

        font_choice_cache[text] = -1;
        return { font.face(), &shaper };
    }
};

TextService::TextService()
    : impl_(std::make_unique<Impl>())
{
}

TextService::~TextService() = default;

TextService::TextService(TextService&& other) noexcept = default;

TextService& TextService::operator=(TextService&& other) noexcept = default;

bool TextService::initialize(int point_size, float display_ppi)
{
    return initialize(TextServiceConfig{}, point_size, display_ppi);
}

bool TextService::initialize(const TextServiceConfig& config, int point_size, float display_ppi)
{
    impl_->config = config;
    return impl_->initialize(point_size, display_ppi);
}

void TextService::shutdown()
{
    impl_->shutdown();
}

bool TextService::set_point_size(int point_size)
{
    return impl_->set_point_size(point_size);
}

int TextService::point_size() const
{
    return impl_->font.point_size();
}

const FontMetrics& TextService::metrics() const
{
    return impl_->font.metrics();
}

const std::string& TextService::primary_font_path() const
{
    return impl_->font_path;
}

AtlasRegion TextService::resolve_cluster(const std::string& text)
{
    return impl_->resolve_cluster(text);
}

bool TextService::atlas_dirty() const
{
    return impl_->glyph_cache.atlas_dirty();
}

void TextService::clear_atlas_dirty()
{
    impl_->glyph_cache.clear_dirty();
}

const uint8_t* TextService::atlas_data() const
{
    return impl_->glyph_cache.atlas_data();
}

int TextService::atlas_width() const
{
    return impl_->glyph_cache.atlas_width();
}

int TextService::atlas_height() const
{
    return impl_->glyph_cache.atlas_height();
}

AtlasDirtyRect TextService::atlas_dirty_rect() const
{
    const auto& dirty = impl_->glyph_cache.dirty_rect();
    return { dirty.x, dirty.y, dirty.w, dirty.h };
}

bool TextService::consume_atlas_reset()
{
    bool reset = impl_->atlas_reset_pending;
    impl_->atlas_reset_pending = false;
    return reset;
}

} // namespace spectre
