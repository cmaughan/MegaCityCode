#include "support/test_support.h"

#include <algorithm>
#include <filesystem>
#include <spectre/font.h>

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

        FontManager font;
        expect(font.initialize(font_path.string(), 11, 96.0f), "font initializes");

        TextShaper shaper;
        shaper.initialize(font.hb_font());

        const std::string lazy_icon = "\xF3\xB0\x92\xB2"; // U+F04B2
        auto shaped = shaper.shape(lazy_icon);
        expect(!shaped.empty(), "lazy icon shapes");
        expect(shaped[0].glyph_id != 0, "lazy icon does not map to .notdef");

        GlyphCache cache;
        expect(cache.initialize(font.face(), font.point_size()), "glyph cache initializes");
        const auto& region = cache.get_cluster(lazy_icon, font.face(), shaper);
        expect(region.width > 0, "lazy icon rasterizes");
        expect(region.height > 0, "lazy icon has height");

        shaper.shutdown();
        font.shutdown();
    });

    run_test("glyph cache dirty rect accumulates newly rasterized glyphs", []() {
        auto font_path = repo_root() / "fonts" / "JetBrainsMonoNerdFont-Regular.ttf";
        expect(std::filesystem::exists(font_path), "bundled font exists");

        FontManager font;
        expect(font.initialize(font_path.string(), 11, 96.0f), "font initializes");

        TextShaper shaper;
        shaper.initialize(font.hb_font());

        GlyphCache cache;
        expect(cache.initialize(font.face(), font.point_size()), "glyph cache initializes");

        const auto& region_l = cache.get_cluster("L", font.face(), shaper);
        const auto& region_a = cache.get_cluster("a", font.face(), shaper);
        const auto& dirty = cache.dirty_rect();

        int atlas_width = cache.atlas_width();
        int l_x = static_cast<int>(region_l.u0 * atlas_width);
        int a_x = static_cast<int>(region_a.u0 * atlas_width);
        int left = std::min(l_x, a_x);
        int right = std::max(l_x + region_l.width, a_x + region_a.width);

        expect_eq(dirty.x, left, "dirty rect starts at the leftmost new glyph");
        expect_eq(dirty.w, right - left, "dirty rect spans all newly rasterized glyphs");

        cache.clear_dirty();
        expect(!cache.atlas_dirty(), "clearing dirtiness resets the dirty flag");
        expect_eq(cache.dirty_rect().w, 0, "clearing dirtiness resets the dirty rect");

        shaper.shutdown();
        font.shutdown();
    });
}
