#include "support/test_support.h"

#include <spectre/grid.h>

using namespace spectre;
using namespace spectre::tests;

void run_grid_tests()
{
    run_test("grid tracks double-width continuations", []() {
        Grid grid;
        grid.resize(4, 2);
        grid.clear_dirty();
        grid.set_cell(1, 0, "A", 7, true);

        const auto& cell = grid.get_cell(1, 0);
        const auto& cont = grid.get_cell(2, 0);
        expect_eq(cell.text, std::string("A"), "double-width cell keeps text");
        expect_eq(cell.codepoint, static_cast<uint32_t>('A'), "double-width cell keeps codepoint");
        expect(cell.double_width, "double-width flag is set");
        expect(cont.double_width_cont, "continuation cell is marked");
        expect_eq(cont.hl_attr_id, static_cast<uint16_t>(7), "continuation cell carries highlight");
    });

    run_test("grid scroll shifts rows and blanks the tail", []() {
        Grid grid;
        grid.resize(3, 3);

        const char rows[3][3] = {
            { 'a', 'b', 'c' },
            { 'd', 'e', 'f' },
            { 'g', 'h', 'i' },
        };

        for (int row = 0; row < 3; ++row)
        {
            for (int col = 0; col < 3; ++col)
            {
                grid.set_cell(col, row, std::string(1, rows[row][col]), 0);
            }
        }

        grid.clear_dirty();
        grid.scroll(0, 3, 0, 3, 1);

        expect_eq(grid.get_cell(0, 0).text, std::string("d"), "row 1 text moves into row 0");
        expect_eq(grid.get_cell(0, 0).codepoint, static_cast<uint32_t>('d'), "row 1 moves into row 0");
        expect_eq(grid.get_cell(2, 1).codepoint, static_cast<uint32_t>('i'), "row 2 moves into row 1");
        expect_eq(grid.get_cell(1, 2).text, std::string(" "), "scrolled-in row text is cleared");
        expect(grid.is_dirty(0, 0), "scroll marks destination cells dirty");
    });

    run_test("grid clears stale continuations when overwriting double-width cells", []() {
        Grid grid;
        grid.resize(4, 1);

        grid.set_cell(1, 0, "X", 3, true);
        grid.set_cell(1, 0, "Y", 4, false);

        expect_eq(grid.get_cell(1, 0).text, std::string("Y"), "overwriting cell replaces text");
        expect_eq(grid.get_cell(1, 0).double_width, false, "overwriting cell clears double-width flag");
        expect_eq(grid.get_cell(2, 0).text, std::string(), "continuation cell text is cleared");
        expect_eq(grid.get_cell(2, 0).double_width_cont, false, "continuation marker is cleared");
    });

    run_test("grid scroll preserves double-width cells and continuations together", []() {
        Grid grid;
        grid.resize(4, 2);
        grid.set_cell(0, 1, "Z", 9, true);
        grid.clear_dirty();

        grid.scroll(0, 2, 0, 4, 1);

        expect_eq(grid.get_cell(0, 0).text, std::string("Z"), "double-width source cell scrolls into destination row");
        expect_eq(grid.get_cell(0, 0).double_width, true, "double-width flag scrolls with the cell");
        expect(grid.get_cell(1, 0).double_width_cont, "continuation cell scrolls with the source cell");
        expect_eq(grid.get_cell(0, 1).text, std::string(" "), "vacated row is cleared after scroll");
    });

    run_test("grid scroll supports horizontal shifts within a partial region", []() {
        Grid grid;
        grid.resize(5, 1);
        grid.set_cell(0, 0, "a", 1);
        grid.set_cell(1, 0, "b", 1);
        grid.set_cell(2, 0, "c", 1);
        grid.set_cell(3, 0, "d", 1);
        grid.set_cell(4, 0, "e", 1);
        grid.clear_dirty();

        grid.scroll(0, 1, 1, 5, 0, 1);

        expect_eq(grid.get_cell(0, 0).text, std::string("a"), "cells outside the scrolled region stay unchanged");
        expect_eq(grid.get_cell(1, 0).text, std::string("c"), "horizontal scroll shifts region content left");
        expect_eq(grid.get_cell(2, 0).text, std::string("d"), "middle cells shift left");
        expect_eq(grid.get_cell(3, 0).text, std::string("e"), "tail cell shifts left");
        expect_eq(grid.get_cell(4, 0).text, std::string(" "), "newly uncovered cell is cleared");
    });

    run_test("grid ignores double-width continuation when placed at the right edge", []() {
        Grid grid;
        grid.resize(2, 1);

        grid.set_cell(1, 0, "X", 7, true);

        expect_eq(grid.get_cell(1, 0).text, std::string("X"), "edge cell text is written");
        expect_eq(grid.get_cell(1, 0).double_width, true, "edge cell keeps the double-width flag");
        expect_eq(grid.get_cell(0, 0).text, std::string(" "), "neighboring cells are untouched");
    });

    run_test("grid clear resets dirty bookkeeping for later cell updates", []() {
        Grid grid;
        grid.resize(4, 2);
        grid.clear_dirty();

        grid.clear();
        grid.set_cell(0, 0, "X", 1);

        auto dirty = grid.get_dirty_cells();
        expect(!dirty.empty(), "dirty list is repopulated after clear");

        bool saw_origin = false;
        for (const auto& cell : dirty)
        {
            if (cell.col == 0 && cell.row == 0)
            {
                saw_origin = true;
                break;
            }
        }

        expect(saw_origin, "updated origin cell is present in dirty list");
        expect_eq(grid.get_cell(0, 0).text, std::string("X"), "updated cell text is preserved");
    });
}
