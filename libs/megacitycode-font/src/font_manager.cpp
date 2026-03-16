#include "font_engine.h"
#include <cmath>
#include <megacitycode/log.h>
#include <utility>

namespace megacitycode
{

FontManager::FontManager(FontManager&& other) noexcept
{
    *this = std::move(other);
}

FontManager& FontManager::operator=(FontManager&& other) noexcept
{
    if (this == &other)
        return *this;

    shutdown();

    ft_lib_ = other.ft_lib_;
    face_ = other.face_;
    hb_font_ = other.hb_font_;
    metrics_ = other.metrics_;
    point_size_ = other.point_size_;
    display_ppi_ = other.display_ppi_;

    other.ft_lib_ = nullptr;
    other.face_ = nullptr;
    other.hb_font_ = nullptr;
    other.metrics_ = {};
    other.point_size_ = 0;
    other.display_ppi_ = 96.0f;

    return *this;
}

bool FontManager::initialize(const std::string& font_path, int point_size, float display_ppi)
{
    point_size_ = point_size;
    display_ppi_ = display_ppi;

    if (FT_Init_FreeType(&ft_lib_))
    {
        MEGACITYCODE_LOG_ERROR(LogCategory::Font, "Failed to init FreeType");
        return false;
    }

    if (FT_New_Face(ft_lib_, font_path.c_str(), 0, &face_))
    {
        MEGACITYCODE_LOG_ERROR(LogCategory::Font, "Failed to load font: %s", font_path.c_str());
        return false;
    }

    FT_Set_Char_Size(face_, 0, point_size * 64, (FT_UInt)display_ppi, (FT_UInt)display_ppi);
    select_best_fixed_size();

    update_metrics();

    hb_font_ = hb_ft_font_create(face_, nullptr);
    if (!hb_font_)
    {
        MEGACITYCODE_LOG_ERROR(LogCategory::Font, "Failed to create HarfBuzz font");
        return false;
    }

    int device_px = (int)std::round(point_size * display_ppi / 72.0f);
    const char* family_name = face_->family_name ? face_->family_name : font_path.c_str();
    MEGACITYCODE_LOG_INFO(LogCategory::Font, "Font loaded: %s %dpt @ %.0f PPI (%d device px), cell=%dx%d, asc=%d, desc=%d",
        family_name, point_size, display_ppi, device_px,
        metrics_.cell_width, metrics_.cell_height,
        metrics_.ascender, metrics_.descender);

    return true;
}

bool FontManager::set_point_size(int point_size)
{
    point_size_ = point_size;
    FT_Set_Char_Size(face_, 0, point_size * 64, (FT_UInt)display_ppi_, (FT_UInt)display_ppi_);
    select_best_fixed_size();

    update_metrics();

    if (hb_font_)
        hb_font_destroy(hb_font_);
    hb_font_ = hb_ft_font_create(face_, nullptr);

    return hb_font_ != nullptr;
}

void FontManager::select_best_fixed_size()
{
    if (!FT_HAS_FIXED_SIZES(face_) || face_->num_fixed_sizes <= 0)
        return;

    int target_px = (int)std::round(point_size_ * display_ppi_ / 72.0f);
    int best = 0;
    for (int i = 1; i < face_->num_fixed_sizes; i++)
    {
        if (std::abs(face_->available_sizes[i].height - target_px) < std::abs(face_->available_sizes[best].height - target_px))
            best = i;
    }
    FT_Select_Size(face_, best);
}

void FontManager::update_metrics()
{
    metrics_.cell_width = (int)(face_->size->metrics.max_advance >> 6);
    metrics_.cell_height = (int)(face_->size->metrics.height >> 6);
    metrics_.ascender = (int)(face_->size->metrics.ascender >> 6);
    metrics_.descender = (int)(-face_->size->metrics.descender >> 6);

    if (metrics_.cell_width == 0)
    {
        FT_Load_Char(face_, 'M', FT_LOAD_DEFAULT);
        metrics_.cell_width = (int)(face_->glyph->advance.x >> 6);
    }
}

void FontManager::shutdown()
{
    if (hb_font_)
    {
        hb_font_destroy(hb_font_);
        hb_font_ = nullptr;
    }
    if (face_)
    {
        FT_Done_Face(face_);
        face_ = nullptr;
    }
    if (ft_lib_)
    {
        FT_Done_FreeType(ft_lib_);
        ft_lib_ = nullptr;
    }
}

} // namespace megacitycode
