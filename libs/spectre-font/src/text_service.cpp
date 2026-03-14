#include <spectre/text_service.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace spectre
{

namespace
{

std::vector<std::string> fallback_font_candidates()
{
#ifdef _WIN32
    const char* windir = std::getenv("WINDIR");
    std::string windows_dir = windir ? windir : "C:\\Windows";
    return {
        windows_dir + "\\Fonts\\seguisym.ttf",
        windows_dir + "\\Fonts\\seguiemj.ttf",
    };
#elif defined(__APPLE__)
    return {
        "/System/Library/Fonts/Apple Color Emoji.ttc",
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
    };
#else
    return {
        "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",
        "/usr/share/fonts/truetype/noto/NotoEmoji-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    };
#endif
}

std::vector<std::string> primary_font_candidates()
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
        candidates.push_back(local_fonts + "JetBrainsMonoNerdFontMono-Regular.ttf");
        candidates.push_back(local_fonts + "JetBrainsMonoNerdFont-Regular.ttf");
    }
    candidates.push_back(windows_dir + "\\Fonts\\JetBrainsMonoNerdFontMono-Regular.ttf");
    candidates.push_back(windows_dir + "\\Fonts\\JetBrainsMonoNerdFont-Regular.ttf");
    candidates.push_back(windows_dir + "\\Fonts\\JetBrains Mono Regular Nerd Font Complete Mono Windows Compatible.ttf");
    candidates.push_back(windows_dir + "\\Fonts\\JetBrains Mono Regular Nerd Font Complete Windows Compatible.ttf");
    candidates.push_back("fonts/JetBrainsMonoNerdFont-Regular.ttf");
    return candidates;
#else
    return { "fonts/JetBrainsMonoNerdFont-Regular.ttf" };
#endif
}

std::string resolve_primary_font_path()
{
    for (const auto& path : primary_font_candidates())
    {
        if (std::filesystem::exists(path))
            return path;
    }

    return "fonts/JetBrainsMonoNerdFont-Regular.ttf";
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

} // namespace

bool TextService::initialize(int point_size, float display_ppi)
{
    display_ppi_ = display_ppi;
    font_path_ = resolve_primary_font_path();
    fprintf(stderr, "[spectre] Primary font path: %s\n", font_path_.c_str());

    if (!font_.initialize(font_path_, point_size, display_ppi_))
        return false;

    glyph_cache_.initialize(font_.face(), font_.point_size());
    shaper_.initialize(font_.hb_font());
    initialize_fallback_fonts();
    return true;
}

void TextService::shutdown()
{
    shaper_.shutdown();
    for (auto& fallback : fallback_fonts_)
    {
        fallback.shaper.shutdown();
        fallback.font.shutdown();
    }
    fallback_fonts_.clear();
    font_choice_cache_.clear();
    font_.shutdown();
}

bool TextService::set_point_size(int point_size)
{
    point_size = std::max(MIN_POINT_SIZE, std::min(MAX_POINT_SIZE, point_size));
    if (point_size == font_.point_size())
        return true;

    if (!font_.set_point_size(point_size))
        return false;

    glyph_cache_.reset(font_.face(), font_.point_size());
    shaper_.initialize(font_.hb_font());
    font_choice_cache_.clear();

    for (auto& fallback : fallback_fonts_)
    {
        fallback.font.set_point_size(point_size);
        fallback.shaper.initialize(fallback.font.hb_font());
    }

    return true;
}

int TextService::point_size() const
{
    return font_.point_size();
}

const FontMetrics& TextService::metrics() const
{
    return font_.metrics();
}

const std::string& TextService::primary_font_path() const
{
    return font_path_;
}

AtlasRegion TextService::resolve_cluster(const std::string& text)
{
    auto [face, text_shaper] = resolve_font_for_text(text);
    return glyph_cache_.get_cluster(text, face, *text_shaper);
}

bool TextService::atlas_dirty() const
{
    return glyph_cache_.atlas_dirty();
}

void TextService::clear_atlas_dirty()
{
    glyph_cache_.clear_dirty();
}

const uint8_t* TextService::atlas_data() const
{
    return glyph_cache_.atlas_data();
}

int TextService::atlas_width() const
{
    return glyph_cache_.atlas_width();
}

int TextService::atlas_height() const
{
    return glyph_cache_.atlas_height();
}

const GlyphCache::DirtyRect& TextService::atlas_dirty_rect() const
{
    return glyph_cache_.dirty_rect();
}

void TextService::initialize_fallback_fonts()
{
    fallback_fonts_.clear();
    font_choice_cache_.clear();

    auto candidates = fallback_font_candidates();
    fallback_fonts_.reserve(candidates.size());

    for (const auto& path : candidates)
    {
        if (path == font_path_ || !std::filesystem::exists(path))
            continue;

        fallback_fonts_.emplace_back();
        auto& fallback = fallback_fonts_.back();
        fallback.path = path;
        if (!fallback.font.initialize(path, font_.point_size(), display_ppi_))
        {
            fallback.font.shutdown();
            fallback_fonts_.pop_back();
            continue;
        }

        fallback.shaper.initialize(fallback.font.hb_font());
        fprintf(stderr, "[spectre] Fallback font loaded: %s\n", path.c_str());
    }
}

std::pair<FT_Face, TextShaper*> TextService::resolve_font_for_text(const std::string& text)
{
    auto cached = font_choice_cache_.find(text);
    if (cached != font_choice_cache_.end())
    {
        if (cached->second < 0)
            return { font_.face(), &shaper_ };

        int idx = cached->second;
        if (idx >= 0 && idx < (int)fallback_fonts_.size())
            return { fallback_fonts_[idx].font.face(), &fallback_fonts_[idx].shaper };
    }

    if (can_render_cluster(font_.face(), shaper_, text))
    {
        font_choice_cache_[text] = -1;
        return { font_.face(), &shaper_ };
    }

    for (int i = 0; i < (int)fallback_fonts_.size(); i++)
    {
        if (can_render_cluster(fallback_fonts_[i].font.face(), fallback_fonts_[i].shaper, text))
        {
            font_choice_cache_[text] = i;
            return { fallback_fonts_[i].font.face(), &fallback_fonts_[i].shaper };
        }
    }

    font_choice_cache_[text] = -1;
    return { font_.face(), &shaper_ };
}

} // namespace spectre
