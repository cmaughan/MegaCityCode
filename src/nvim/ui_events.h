#pragma once
#include "nvim/rpc.h"
#include "grid/grid.h"
#include "grid/highlight.h"
#include "renderer/types.h"
#include <vector>
#include <functional>

namespace spectre {

struct ModeInfo {
    std::string name;
    CursorShape cursor_shape = CursorShape::Block;
    int cell_percentage = 0;
    int attr_id = 0;
};

class UiEventHandler {
public:
    void set_grid(Grid* grid) { grid_ = grid; }
    void set_highlights(HighlightTable* hl) { highlights_ = hl; }

    // Process a "redraw" notification batch
    void process_redraw(const std::vector<MpackValue>& params);

    // Callbacks
    std::function<void()> on_flush;
    std::function<void(int, int)> on_grid_resize;
    std::function<void(int, int)> on_cursor_goto;
    std::function<void(const std::string&, const MpackValue&)> on_option_set;

    const std::vector<ModeInfo>& modes() const { return modes_; }
    int current_mode() const { return current_mode_; }
    int cursor_col() const { return cursor_col_; }
    int cursor_row() const { return cursor_row_; }

private:
    void handle_grid_line(const MpackValue& args);
    void handle_grid_cursor_goto(const MpackValue& args);
    void handle_grid_scroll(const MpackValue& args);
    void handle_grid_clear(const MpackValue& args);
    void handle_grid_resize(const MpackValue& args);
    void handle_hl_attr_define(const MpackValue& args);
    void handle_default_colors_set(const MpackValue& args);
    void handle_mode_info_set(const MpackValue& args);
    void handle_mode_change(const MpackValue& args);
    void handle_option_set(const MpackValue& args);

    Grid* grid_ = nullptr;
    HighlightTable* highlights_ = nullptr;

    std::vector<ModeInfo> modes_;
    int current_mode_ = 0;
    int cursor_col_ = 0, cursor_row_ = 0;
};

} // namespace spectre
