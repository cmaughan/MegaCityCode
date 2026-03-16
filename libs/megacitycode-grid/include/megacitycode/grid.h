#pragma once
#include <cstddef>
#include <cstdint>
#include <megacitycode/highlight.h>
#include <span>
#include <string>
#include <vector>

namespace megacitycode
{

struct Cell
{
    std::string text = " ";
    uint32_t codepoint = ' ';
    uint16_t hl_attr_id = 0;
};

class Grid
{
public:
    void resize(int cols, int rows);
    void clear();

    void set_cell(int col, int row, const std::string& text, uint16_t hl_id, bool double_width = false);
    const Cell& get_cell(int col, int row) const;
    Cell& get_cell(int col, int row);

    std::span<Cell> cells()
    {
        return cells_;
    }
    std::span<const Cell> cells() const
    {
        return cells_;
    }

    int cols() const
    {
        return cols_;
    }
    int rows() const
    {
        return rows_;
    }
    size_t size() const
    {
        return cells_.size();
    }

private:
    int cols_ = 0, rows_ = 0;
    std::vector<Cell> cells_;
    Cell empty_cell_;
};

} // namespace megacitycode
