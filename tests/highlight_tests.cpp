#include "support/test_support.h"

#include <spectre/highlight.h>

using namespace spectre;
using namespace spectre::tests;

void run_highlight_tests()
{
    run_test("highlight table resolves explicit colors and special color", []() {
        HighlightTable table;
        table.set_default_fg(Color::from_rgb(0xFFFFFF));
        table.set_default_bg(Color::from_rgb(0x101010));
        table.set_default_sp(Color::from_rgb(0x00FF00));

        HlAttr attr;
        attr.fg = Color::from_rgb(0x123456);
        attr.bg = Color::from_rgb(0x654321);
        attr.sp = Color::from_rgb(0xABCDEF);
        attr.has_fg = true;
        attr.has_bg = true;
        attr.has_sp = true;

        Color fg;
        Color bg;
        Color sp;
        table.resolve(attr, fg, bg, &sp);

        expect_eq(fg, Color::from_rgb(0x123456), "foreground resolves explicitly");
        expect_eq(bg, Color::from_rgb(0x654321), "background resolves explicitly");
        expect_eq(sp, Color::from_rgb(0xABCDEF), "special color resolves explicitly");
    });

    run_test("highlight table reverses colors without losing accent color", []() {
        HighlightTable table;
        table.set_default_fg(Color::from_rgb(0xFFFFFF));
        table.set_default_bg(Color::from_rgb(0x101010));

        HlAttr attr;
        attr.fg = Color::from_rgb(0x123456);
        attr.bg = Color::from_rgb(0x654321);
        attr.has_fg = true;
        attr.has_bg = true;
        attr.reverse = true;

        Color fg;
        Color bg;
        Color sp;
        table.resolve(attr, fg, bg, &sp);

        expect_eq(fg, Color::from_rgb(0x654321), "reverse swaps foreground");
        expect_eq(bg, Color::from_rgb(0x123456), "reverse swaps background");
        expect_eq(sp, fg, "special color falls back to resolved foreground");
    });
}
