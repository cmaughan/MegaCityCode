#include "support/test_support.h"

#include <megacitycode/grid.h>

using namespace megacitycode;
using namespace megacitycode::tests;

void run_grid_tests()
{
    run_test("grid resize allocates a flat cell array", []() {
        Grid grid;
        grid.resize(4, 2);

        expect_eq(grid.cols(), 4, "column count is stored");
        expect_eq(grid.rows(), 2, "row count is stored");
        expect_eq(static_cast<int>(grid.size()), 8, "grid owns a flat array of cells");
        expect_eq(static_cast<int>(grid.cells().size()), 8, "span exposes every cell");
    });

    run_test("grid stores cell text, codepoint, and highlight id", []() {
        Grid grid;
        grid.resize(3, 2);
        grid.set_cell(1, 1, "Q", 42);

        const auto& cell = grid.get_cell(1, 1);
        expect_eq(cell.text, std::string("Q"), "text is stored");
        expect_eq(cell.codepoint, static_cast<uint32_t>('Q'), "codepoint tracks the first rune");
        expect_eq(cell.hl_attr_id, static_cast<uint16_t>(42), "highlight id is stored");
    });

    run_test("grid clear resets cells back to blanks", []() {
        Grid grid;
        grid.resize(2, 2);
        grid.set_cell(0, 0, "X", 5);
        grid.set_cell(1, 1, "Y", 6);
        grid.clear();

        expect_eq(grid.get_cell(0, 0).text, std::string(" "), "first cell is blank again");
        expect_eq(grid.get_cell(1, 1).text, std::string(" "), "last cell is blank again");
        expect_eq(grid.get_cell(1, 1).hl_attr_id, static_cast<uint16_t>(0), "highlight ids reset");
    });
}
