#pragma once

#include <spectre/renderer.h>
#include <vector>

namespace spectre
{

class RendererState
{
public:
    void set_grid_size(int cols, int rows, int padding);
    void set_cell_size(int w, int h);
    void set_ascender(int a);

    void update_cells(std::span<const CellUpdate> updates);
    void set_cursor(int col, int row, const CursorStyle& style);
    void restore_cursor();
    void apply_cursor();

    int grid_cols() const
    {
        return grid_cols_;
    }

    int grid_rows() const
    {
        return grid_rows_;
    }

    int total_cells() const
    {
        return grid_cols_ * grid_rows_;
    }

    int bg_instances() const
    {
        return total_cells() + (cursor_overlay_active_ ? 1 : 0);
    }

    size_t buffer_size_bytes() const
    {
        return (gpu_cells_.size() + 1) * sizeof(GpuCell);
    }

    void copy_to(void* dst) const;
    bool has_dirty_cells() const;
    size_t dirty_cell_offset_bytes() const;
    size_t dirty_cell_size_bytes() const;
    void copy_dirty_cells_to(void* dst) const;
    bool overlay_slot_dirty() const;
    size_t overlay_offset_bytes() const;
    void copy_overlay_cell_to(void* dst) const;
    void clear_dirty();

private:
    void relayout();
    void mark_all_cells_dirty();
    void mark_cell_dirty(size_t index);

    int grid_cols_ = 0;
    int grid_rows_ = 0;
    int cell_w_ = 10;
    int cell_h_ = 20;
    int ascender_ = 16;
    int padding_ = 1;

    int cursor_col_ = 0;
    int cursor_row_ = 0;
    CursorStyle cursor_style_ = {};

    std::vector<GpuCell> gpu_cells_;
    GpuCell overlay_cell_ = {};
    GpuCell cursor_saved_cell_ = {};
    bool cursor_applied_ = false;
    bool cursor_overlay_active_ = false;
    size_t dirty_cell_begin_ = 0;
    size_t dirty_cell_end_ = 0;
    bool overlay_dirty_ = false;
};

} // namespace spectre
