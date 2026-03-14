#pragma once
#include "grid/cell.h"
#include <vector>
#include <cstdint>

namespace spectre {

class Grid {
public:
    void resize(int cols, int rows);
    void clear();

    void set_cell(int col, int row, uint32_t codepoint, uint16_t hl_id, bool double_width = false);
    const Cell& get_cell(int col, int row) const;

    // Scroll region [top, bot) by `rows` lines (positive = content moves up)
    void scroll(int top, int bot, int left, int right, int rows);

    int cols() const { return cols_; }
    int rows() const { return rows_; }

    // Dirty tracking
    bool is_dirty(int col, int row) const;
    void mark_dirty(int col, int row);
    void mark_all_dirty();
    void clear_dirty();

    // Get all dirty cell coordinates
    struct DirtyCell { int col, row; };
    std::vector<DirtyCell> get_dirty_cells() const;

private:
    int cols_ = 0, rows_ = 0;
    std::vector<Cell> cells_;
    Cell empty_cell_;
};

} // namespace spectre
