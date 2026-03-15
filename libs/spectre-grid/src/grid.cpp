#include <algorithm>
#include <cassert>
#include <spectre/grid.h>
#include <spectre/unicode.h>

namespace spectre
{

namespace
{

Cell make_blank_cell()
{
    return Cell{};
}

void clear_continuation(Cell& cell)
{
    cell = make_blank_cell();
    cell.text.clear();
    cell.codepoint = ' ';
    cell.dirty = true;
}

} // namespace

void Grid::resize(int cols, int rows)
{
    cols_ = cols;
    rows_ = rows;
    cells_.resize(cols * rows);
    dirty_marks_.assign((size_t)cols * rows, 0);
    dirty_cells_.clear();
    clear();
}

void Grid::clear()
{
    dirty_cells_.clear();
    std::fill(dirty_marks_.begin(), dirty_marks_.end(), (uint8_t)0);
    for (size_t i = 0; i < cells_.size(); i++)
    {
        cells_[i] = make_blank_cell();
        mark_dirty_index((int)i);
    }
}

void Grid::set_cell(int col, int row, const std::string& text, uint16_t hl_id, bool double_width)
{
    if (col < 0 || col >= cols_ || row < 0 || row >= rows_)
        return;

    const int index = row * cols_ + col;
    auto& cell = cells_[index];
    if (col > 0)
    {
        auto& prev = cells_[index - 1];
        if (cell.double_width_cont || prev.double_width)
        {
            prev = make_blank_cell();
            mark_dirty_index(index - 1);
        }
    }

    if (col + 1 < cols_)
    {
        auto& next = cells_[index + 1];
        if (next.double_width_cont)
        {
            clear_continuation(next);
            mark_dirty_index(index + 1);
        }
    }

    cell.text = text;
    cell.codepoint = utf8_first_codepoint(text);
    cell.hl_attr_id = hl_id;
    cell.dirty = false;
    cell.double_width = double_width;
    cell.double_width_cont = false;
    mark_dirty_index(index);

    if (double_width && col + 1 < cols_)
    {
        auto& next = cells_[index + 1];
        next.text.clear();
        next.codepoint = ' ';
        next.hl_attr_id = hl_id;
        next.dirty = false;
        next.double_width = false;
        next.double_width_cont = true;
        mark_dirty_index(index + 1);
    }
}

const Cell& Grid::get_cell(int col, int row) const
{
    if (col < 0 || col >= cols_ || row < 0 || row >= rows_)
        return empty_cell_;
    return cells_[row * cols_ + col];
}

void Grid::scroll(int top, int bot, int left, int right, int rows, int cols)
{
    if (rows == 0 && cols == 0)
        return;

    bool valid = top >= 0 && top < bot && bot <= rows_
        && left >= 0 && left < right && right <= cols_;
    assert(valid && "Grid::scroll received out-of-bounds region");
    if (!valid)
        return;

    if (rows > 0)
    {
        for (int r = top; r < bot - rows; r++)
        {
            for (int c = left; c < right; c++)
            {
                cells_[r * cols_ + c] = cells_[(r + rows) * cols_ + c];
                mark_dirty_index(r * cols_ + c);
            }
        }
        for (int r = bot - rows; r < bot; r++)
        {
            for (int c = left; c < right; c++)
            {
                cells_[r * cols_ + c] = make_blank_cell();
                mark_dirty_index(r * cols_ + c);
            }
        }
    }
    else if (rows < 0)
    {
        int shift = -rows;
        for (int r = bot - 1; r >= top + shift; r--)
        {
            for (int c = left; c < right; c++)
            {
                cells_[r * cols_ + c] = cells_[(r - shift) * cols_ + c];
                mark_dirty_index(r * cols_ + c);
            }
        }
        for (int r = top; r < top + shift; r++)
        {
            for (int c = left; c < right; c++)
            {
                cells_[r * cols_ + c] = make_blank_cell();
                mark_dirty_index(r * cols_ + c);
            }
        }
    }

    if (cols > 0)
    {
        for (int r = top; r < bot; r++)
        {
            for (int c = left; c < right - cols; c++)
            {
                cells_[r * cols_ + c] = cells_[r * cols_ + c + cols];
                mark_dirty_index(r * cols_ + c);
            }
            for (int c = right - cols; c < right; c++)
            {
                cells_[r * cols_ + c] = make_blank_cell();
                mark_dirty_index(r * cols_ + c);
            }
        }
    }
    else if (cols < 0)
    {
        int shift = -cols;
        for (int r = top; r < bot; r++)
        {
            for (int c = right - 1; c >= left + shift; c--)
            {
                cells_[r * cols_ + c] = cells_[r * cols_ + c - shift];
                mark_dirty_index(r * cols_ + c);
            }
            for (int c = left; c < left + shift; c++)
            {
                cells_[r * cols_ + c] = make_blank_cell();
                mark_dirty_index(r * cols_ + c);
            }
        }
    }

    for (int r = top; r < bot; ++r)
    {
        for (int c = 0; c < cols_; ++c)
        {
            const int index = r * cols_ + c;
            auto& cell = cells_[(size_t)index];

            if (cell.double_width && c + 1 < cols_ && !cells_[(size_t)index + 1].double_width_cont)
            {
                cell = make_blank_cell();
                mark_dirty_index(index);
                continue;
            }

            if (cell.double_width_cont && (c == 0 || !cells_[(size_t)index - 1].double_width))
            {
                clear_continuation(cell);
                mark_dirty_index(index);
            }
        }
    }
}

bool Grid::is_dirty(int col, int row) const
{
    if (col < 0 || col >= cols_ || row < 0 || row >= rows_)
        return false;
    return cells_[row * cols_ + col].dirty;
}

void Grid::mark_dirty(int col, int row)
{
    if (col < 0 || col >= cols_ || row < 0 || row >= rows_)
        return;
    mark_dirty_index(row * cols_ + col);
}

void Grid::mark_all_dirty()
{
    dirty_cells_.clear();
    std::fill(dirty_marks_.begin(), dirty_marks_.end(), (uint8_t)0);
    for (size_t i = 0; i < cells_.size(); i++)
        mark_dirty_index((int)i);
}

void Grid::clear_dirty()
{
    for (auto& c : cells_)
        c.dirty = false;
    std::fill(dirty_marks_.begin(), dirty_marks_.end(), (uint8_t)0);
    dirty_cells_.clear();
}

std::vector<Grid::DirtyCell> Grid::get_dirty_cells() const
{
    return dirty_cells_;
}

void Grid::mark_dirty_index(int index)
{
    if (index < 0 || index >= (int)cells_.size())
        return;

    auto& cell = cells_[(size_t)index];
    cell.dirty = true;

    if (dirty_marks_[(size_t)index])
        return;

    dirty_marks_[(size_t)index] = 1;
    dirty_cells_.push_back({ index % cols_, index / cols_ });
}

} // namespace spectre
