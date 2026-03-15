#pragma once
#include <cstdint>
#include <spectre/grid_sink.h>
#include <spectre/highlight.h>
#include <string>
#include <vector>

namespace spectre
{

struct Cell
{
    std::string text = " ";
    uint32_t codepoint = ' ';
    uint16_t hl_attr_id = 0;
    bool dirty = true;
    bool double_width = false;
    bool double_width_cont = false;
};

class Grid : public IGridSink
{
public:
    void resize(int cols, int rows) override;
    void clear() override;

    void set_cell(int col, int row, const std::string& text, uint16_t hl_id, bool double_width = false) override;
    const Cell& get_cell(int col, int row) const;

    void scroll(int top, int bot, int left, int right, int rows, int cols = 0) override;

    int cols() const
    {
        return cols_;
    }
    int rows() const
    {
        return rows_;
    }

    bool is_dirty(int col, int row) const;
    void mark_dirty(int col, int row);
    void mark_all_dirty();
    void clear_dirty();
    size_t dirty_cell_count() const
    {
        return dirty_cells_.size();
    }

    struct DirtyCell
    {
        int col, row;
    };
    std::vector<DirtyCell> get_dirty_cells() const;

private:
    void mark_dirty_index(int index);

    int cols_ = 0, rows_ = 0;
    std::vector<Cell> cells_;
    std::vector<DirtyCell> dirty_cells_;
    std::vector<uint8_t> dirty_marks_;
    Cell empty_cell_;
};

} // namespace spectre
