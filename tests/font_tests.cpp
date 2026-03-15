#include "support/test_support.h"

#include "font_engine.h"

#include <algorithm>
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
}
