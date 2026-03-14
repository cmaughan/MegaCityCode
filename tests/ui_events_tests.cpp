#include "support/replay_fixture.h"
#include "support/test_support.h"

#include <spectre/grid.h>
#include <spectre/nvim.h>

using namespace spectre;
using namespace spectre::tests;

void run_ui_events_tests()
{
    run_test("ui event handler replays redraw batches into the grid", []() {
        Grid grid;
        grid.resize(4, 2);

        HighlightTable highlights;
        UiEventHandler handler;
        handler.set_grid(&grid);
        handler.set_highlights(&highlights);

        int flushes = 0;
        handler.on_flush = [&]() { ++flushes; };

        handler.process_redraw({ redraw_event("default_colors_set", { arr({ i(0xFFFFFF), i(0x101010), i(0xABCDEF) }) }),
            redraw_event("hl_attr_define", { arr({ i(7), map({ { s("foreground"), i(0x123456) }, { s("background"), i(0x654321) }, { s("bold"), b(true) } }) }) }),
            redraw_event("grid_line", { grid_line_batch(1, 0, 0, { cell("A", 7), cell("B", std::nullopt, 2) }) }),
            redraw_event("grid_cursor_goto", { arr({ i(1), i(1), i(2) }) }),
            redraw_event("mode_info_set", { arr({ b(true), arr({ map({ { s("name"), s("normal") }, { s("cursor_shape"), s("vertical") }, { s("attr_id"), i(7) } }) }) }) }),
            redraw_event("mode_change", { arr({ s("normal"), i(0) }) }),
            redraw_event("flush", {}) });

        expect_eq(flushes, 1, "flush callback fires once");
        expect_eq(grid.get_cell(0, 0).codepoint, static_cast<uint32_t>('A'), "first cell is written");
        expect_eq(grid.get_cell(1, 0).codepoint, static_cast<uint32_t>('B'), "repeat cell writes first copy");
        expect_eq(grid.get_cell(2, 0).codepoint, static_cast<uint32_t>('B'), "repeat cell writes second copy");
        expect_eq(handler.cursor_row(), 1, "cursor row is updated");
        expect_eq(handler.cursor_col(), 2, "cursor col is updated");
        expect_eq(handler.current_mode(), 0, "mode index is updated");
        expect_eq(static_cast<int>(handler.modes().size()), 1, "mode table is populated");
        expect_eq(handler.modes()[0].attr_id, 7, "mode attr id is preserved");
        expect_eq(highlights.get(7).bold, true, "highlight attributes are stored");
        expect_eq(highlights.default_bg(), Color::from_rgb(0x101010), "default background is updated");
    });

    run_test("ui event handler forwards grid resize callbacks", []() {
        Grid grid;
        grid.resize(2, 2);

        HighlightTable highlights;
        UiEventHandler handler;
        handler.set_grid(&grid);
        handler.set_highlights(&highlights);

        int resized_cols = 0;
        int resized_rows = 0;
        handler.on_grid_resize = [&](int cols, int rows) {
            resized_cols = cols;
            resized_rows = rows;
        };

        handler.process_redraw({ redraw_event("grid_resize", { arr({ i(1), i(6), i(4) }) }) });

        expect_eq(grid.cols(), 6, "grid resize updates columns");
        expect_eq(grid.rows(), 4, "grid resize updates rows");
        expect_eq(resized_cols, 6, "resize callback receives columns");
        expect_eq(resized_rows, 4, "resize callback receives rows");
    });
}
