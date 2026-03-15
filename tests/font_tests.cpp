#include "support/test_support.h"

#include "font_engine.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <spectre/text_service.h>

using namespace spectre;
using namespace spectre::tests;

namespace
{

std::filesystem::path repo_root()
{
    auto here = std::filesystem::path(__FILE__).parent_path();
    return here.parent_path();
}

std::filesystem::path color_emoji_font_path()
{
#ifdef _WIN32
    const char* windir = std::getenv("WINDIR");
    auto windows_dir = std::filesystem::path(windir ? windir : "C:\\Windows");
    return windows_dir / "Fonts" / "seguiemj.ttf";
#elif defined(__APPLE__)
    return "/System/Library/Fonts/Apple Color Emoji.ttc";
#else
    auto noto_color = std::filesystem::path("/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf");
    if (std::filesystem::exists(noto_color))
        return noto_color;
    return "/usr/share/fonts/truetype/noto/NotoEmoji-Regular.ttf";
#endif
}

} // namespace

void run_font_tests()
{
    run_test("bundled nerd font shapes and rasterizes current lazy icon", []() {
        auto font_path = repo_root() / "fonts" / "JetBrainsMonoNerdFont-Regular.ttf";
        expect(std::filesystem::exists(font_path), "bundled font exists");

        TextService service;
        TextServiceConfig config;
        config.font_path = font_path.string();
        expect(service.initialize(config, 11, 96.0f), "text service initializes");
        const std::string lazy_icon = "\xF3\xB0\x92\xB2"; // U+F04B2
        const auto region = service.resolve_cluster(lazy_icon);
        expect(region.width > 0, "lazy icon rasterizes");
        expect(region.height > 0, "lazy icon has height");
        expect_eq(service.primary_font_path(), font_path.string(), "configured font path is used");
        service.shutdown();
    });

    run_test("glyph cache dirty rect accumulates newly rasterized glyphs", []() {
        auto font_path = repo_root() / "fonts" / "JetBrainsMonoNerdFont-Regular.ttf";
        expect(std::filesystem::exists(font_path), "bundled font exists");

        TextService service;
        TextServiceConfig config;
        config.font_path = font_path.string();
        expect(service.initialize(config, 11, 96.0f), "text service initializes");

        const auto region_l = service.resolve_cluster("L");
        const auto region_a = service.resolve_cluster("a");
        const auto dirty = service.atlas_dirty_rect();

        int atlas_width = service.atlas_width();
        int l_x = static_cast<int>(region_l.u0 * atlas_width);
        int a_x = static_cast<int>(region_a.u0 * atlas_width);
        int left = std::min(l_x, a_x);
        int right = std::max(l_x + region_l.width, a_x + region_a.width);

        expect_eq(dirty.x, left, "dirty rect starts at the leftmost new glyph");
        expect_eq(dirty.w, right - left, "dirty rect spans all newly rasterized glyphs");

        service.clear_atlas_dirty();
        expect(!service.atlas_dirty(), "clearing dirtiness resets the dirty flag");
        expect_eq(service.atlas_dirty_rect().w, 0, "clearing dirtiness resets the dirty rect");
        service.shutdown();
    });

    run_test("glyph cache reports overflow so the atlas can be rebuilt", []() {
        auto font_path = repo_root() / "fonts" / "JetBrainsMonoNerdFont-Regular.ttf";
        expect(std::filesystem::exists(font_path), "bundled font exists");

        FontManager font;
        expect(font.initialize(font_path.string(), 11, 96.0f), "font initializes");

        TextShaper shaper;
        shaper.initialize(font.hb_font());

        GlyphCache cache;
        expect(cache.initialize(font.face(), font.point_size(), 32), "small glyph cache initializes");

        for (char ch = '!'; ch <= '~'; ch++)
        {
            std::string text(1, ch);
            cache.get_cluster(text, font.face(), shaper);
            if (cache.consume_overflowed())
            {
                expect(true, "small atlas overflow is surfaced explicitly");
                shaper.shutdown();
                font.shutdown();
                return;
            }
        }

        expect(false, "small atlas should overflow under pressure");
    });

    run_test("emoji fallback preserves color glyph pixels in the atlas", []() {
        auto primary_font_path = repo_root() / "fonts" / "JetBrainsMonoNerdFont-Regular.ttf";
        auto emoji_font_path = color_emoji_font_path();
        expect(std::filesystem::exists(primary_font_path), "bundled font exists");
        if (!std::filesystem::exists(emoji_font_path))
            skip("color emoji font not available on this machine");

        TextService service;
        TextServiceConfig config;
        config.font_path = primary_font_path.string();
#ifdef _WIN32
        const char* windir = std::getenv("WINDIR");
        auto windows_dir = std::filesystem::path(windir ? windir : "C:\\Windows");
        auto symbol_font_path = windows_dir / "Fonts" / "seguisym.ttf";
        if (std::filesystem::exists(symbol_font_path))
            config.fallback_paths.push_back(symbol_font_path.string());
#endif
        config.fallback_paths.push_back(emoji_font_path.string());
        expect(service.initialize(config, 11, 96.0f), "text service initializes");

        const std::string sleep_emoji = "\xF0\x9F\x92\xA4"; // U+1F4A4
        const auto region = service.resolve_cluster(sleep_emoji);
        expect(region.width > 0, "emoji rasterizes");
        expect(region.height > 0, "emoji has height");
        expect(region.is_color, "emoji region is flagged as color");

        const auto* atlas = service.atlas_data();
        const int atlas_width = service.atlas_width();
        const int atlas_x = static_cast<int>(region.u0 * atlas_width + 0.5f);
        const int atlas_y = static_cast<int>(region.v0 * atlas_width + 0.5f);

        bool found_colored_pixel = false;
        for (int row = 0; row < region.height && !found_colored_pixel; row++)
        {
            for (int col = 0; col < region.width; col++)
            {
                const size_t pixel_index = (((size_t)(atlas_y + row) * atlas_width) + atlas_x + col) * 4;
                const uint8_t r = atlas[pixel_index + 0];
                const uint8_t g = atlas[pixel_index + 1];
                const uint8_t b = atlas[pixel_index + 2];
                const uint8_t a = atlas[pixel_index + 3];
                if (a > 0 && !(r == 255 && g == 255 && b == 255))
                {
                    found_colored_pixel = true;
                    break;
                }
            }
        }

        expect(found_colored_pixel, "emoji atlas region contains non-monochrome pixels");
        service.shutdown();
    });
}
