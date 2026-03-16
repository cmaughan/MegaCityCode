#include <algorithm>
#include <megacitycode/grid.h>
#include <megacitycode/unicode.h>

namespace megacitycode
{

void Grid::resize(int cols, int rows)
{
    cols_ = std::max(0, cols);
    rows_ = std::max(0, rows);
    cells_.resize(static_cast<size_t>(cols_) * rows_);
    clear();
}

void Grid::clear()
{
    for (auto& cell : cells_)
        cell = {};
}

void Grid::set_cell(int col, int row, const std::string& text, uint16_t hl_id, bool double_width)
{
    (void)double_width;
    if (col < 0 || col >= cols_ || row < 0 || row >= rows_)
        return;

    auto& cell = cells_[static_cast<size_t>(row * cols_ + col)];
    cell.text = text.empty() ? std::string(" ") : text;
    cell.codepoint = utf8_first_codepoint(text);
    cell.hl_attr_id = hl_id;
}

const Cell& Grid::get_cell(int col, int row) const
{
    if (col < 0 || col >= cols_ || row < 0 || row >= rows_)
        return empty_cell_;
    return cells_[static_cast<size_t>(row * cols_ + col)];
}

Cell& Grid::get_cell(int col, int row)
{
    if (col < 0 || col >= cols_ || row < 0 || row >= rows_)
        return empty_cell_;
    return cells_[static_cast<size_t>(row * cols_ + col)];
}

} // namespace megacitycode
