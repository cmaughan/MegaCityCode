#include <cstdio>
#include <spectre/nvim.h>
#include <spectre/unicode.h>

namespace spectre
{

void UiEventHandler::process_redraw(const std::vector<MpackValue>& params)
{
    for (size_t ei = 0; ei < params.size(); ei++)
    {
        auto& event = params[ei];
        if (event.type() != MpackValue::Array || event.as_array().empty())
            continue;

        const auto& event_array = event.as_array();
        const std::string& name = event_array[0].as_str();

        if (name == "flush")
        {
            if (on_flush)
                on_flush();
            continue;
        }

        if (name == "busy_start")
        {
            if (on_busy)
                on_busy(true);
            continue;
        }

        if (name == "busy_stop")
        {
            if (on_busy)
                on_busy(false);
            continue;
        }

        for (size_t i = 1; i < event_array.size(); i++)
        {
            const auto& args = event_array[i];

            if (name == "grid_line")
                handle_grid_line(args);
            else if (name == "grid_cursor_goto")
                handle_grid_cursor_goto(args);
            else if (name == "grid_scroll")
                handle_grid_scroll(args);
            else if (name == "grid_clear")
                handle_grid_clear(args);
            else if (name == "grid_resize")
                handle_grid_resize(args);
            else if (name == "hl_attr_define")
                handle_hl_attr_define(args);
            else if (name == "default_colors_set")
                handle_default_colors_set(args);
            else if (name == "mode_info_set")
                handle_mode_info_set(args);
            else if (name == "mode_change")
                handle_mode_change(args);
            else if (name == "option_set")
                handle_option_set(args);
        }
    }
}

void UiEventHandler::handle_grid_line(const MpackValue& args)
{
    if (!grid_ || args.type() != MpackValue::Array || args.as_array().size() < 4)
        return;

    const auto& args_array = args.as_array();
    int row = (int)args_array[1].as_int();
    int col_start = (int)args_array[2].as_int();

    int col = col_start;
    uint16_t current_hl = 0;

    const auto& cells = args_array[3].as_array();
    for (auto& cell : cells)
    {
        if (cell.type() != MpackValue::Array || cell.as_array().empty())
            continue;

        const auto& cell_array = cell.as_array();
        const std::string& text = cell_array[0].as_str();

        if (cell_array.size() >= 2 && !cell_array[1].is_nil())
        {
            current_hl = (uint16_t)cell_array[1].as_int();
        }

        int repeat = 1;
        if (cell_array.size() >= 3)
        {
            repeat = (int)cell_array[2].as_int();
        }

        bool dw = cluster_cell_width(text, options_ ? *options_ : UiOptions{}) == 2;
        for (int r = 0; r < repeat; r++)
        {
            grid_->set_cell(col, row, text, current_hl, dw);
            col++;
            if (dw)
                col++;
        }
    }
}

void UiEventHandler::handle_grid_cursor_goto(const MpackValue& args)
{
    if (args.type() != MpackValue::Array || args.as_array().size() < 3)
        return;
    const auto& args_array = args.as_array();
    cursor_row_ = (int)args_array[1].as_int();
    cursor_col_ = (int)args_array[2].as_int();
    if (on_cursor_goto)
        on_cursor_goto(cursor_col_, cursor_row_);
}

void UiEventHandler::handle_grid_scroll(const MpackValue& args)
{
    if (!grid_ || args.type() != MpackValue::Array || args.as_array().size() < 7)
        return;
    const auto& args_array = args.as_array();
    int top = (int)args_array[1].as_int();
    int bot = (int)args_array[2].as_int();
    int left = (int)args_array[3].as_int();
    int right = (int)args_array[4].as_int();
    int rows = (int)args_array[5].as_int();

    grid_->scroll(top, bot, left, right, rows);
}

void UiEventHandler::handle_grid_clear(const MpackValue& args)
{
    if (!grid_)
        return;
    grid_->clear();
}

void UiEventHandler::handle_grid_resize(const MpackValue& args)
{
    if (args.type() != MpackValue::Array || args.as_array().size() < 3)
        return;
    const auto& args_array = args.as_array();
    int cols = (int)args_array[1].as_int();
    int rows = (int)args_array[2].as_int();
    if (grid_)
        grid_->resize(cols, rows);
    if (on_grid_resize)
        on_grid_resize(cols, rows);
}

void UiEventHandler::handle_hl_attr_define(const MpackValue& args)
{
    if (!highlights_ || args.type() != MpackValue::Array || args.as_array().size() < 2)
        return;
    const auto& args_array = args.as_array();
    uint16_t id = (uint16_t)args_array[0].as_int();
    const auto& attrs = args_array[1];

    HlAttr hl = {};
    if (attrs.type() == MpackValue::Map)
    {
        for (auto& [key, val] : attrs.as_map())
        {
            const std::string& k = key.as_str();
            if (k == "foreground")
            {
                hl.fg = Color::from_rgb((uint32_t)val.as_int());
            }
            else if (k == "background")
            {
                hl.bg = Color::from_rgb((uint32_t)val.as_int());
            }
            else if (k == "special")
            {
                hl.sp = Color::from_rgb((uint32_t)val.as_int());
            }
            else if (k == "bold")
            {
                hl.bold = val.as_bool();
            }
            else if (k == "italic")
            {
                hl.italic = val.as_bool();
            }
            else if (k == "underline")
            {
                hl.underline = val.as_bool();
            }
            else if (k == "undercurl")
            {
                hl.undercurl = val.as_bool();
            }
            else if (k == "strikethrough")
            {
                hl.strikethrough = val.as_bool();
            }
            else if (k == "reverse")
            {
                hl.reverse = val.as_bool();
            }
        }
    }

    highlights_->set(id, hl);
}

void UiEventHandler::handle_default_colors_set(const MpackValue& args)
{
    if (!highlights_ || args.type() != MpackValue::Array || args.as_array().size() < 3)
        return;
    const auto& args_array = args.as_array();
    uint32_t fg = (uint32_t)args_array[0].as_int();
    uint32_t bg = (uint32_t)args_array[1].as_int();
    uint32_t sp = (uint32_t)args_array[2].as_int();

    highlights_->set_default_fg(Color::from_rgb(fg));
    highlights_->set_default_bg(Color::from_rgb(bg));
    highlights_->set_default_sp(Color::from_rgb(sp));
}

void UiEventHandler::handle_mode_info_set(const MpackValue& args)
{
    if (args.type() != MpackValue::Array || args.as_array().size() < 2)
        return;
    const auto& args_array = args.as_array();
    const auto& modes = args_array[1].as_array();
    modes_.clear();
    for (auto& m : modes)
    {
        ModeInfo info;
        if (m.type() == MpackValue::Map)
        {
            for (auto& [key, val] : m.as_map())
            {
                const std::string& k = key.as_str();
                if (k == "name")
                    info.name = val.as_str();
                else if (k == "cursor_shape")
                {
                    const std::string& shape = val.as_str();
                    if (shape == "block")
                        info.cursor_shape = CursorShape::Block;
                    else if (shape == "horizontal")
                        info.cursor_shape = CursorShape::Horizontal;
                    else if (shape == "vertical")
                        info.cursor_shape = CursorShape::Vertical;
                }
                else if (k == "cell_percentage")
                    info.cell_percentage = (int)val.as_int();
                else if (k == "attr_id")
                    info.attr_id = (int)val.as_int();
                else if (k == "blinkwait")
                    info.blinkwait = (int)val.as_int();
                else if (k == "blinkon")
                    info.blinkon = (int)val.as_int();
                else if (k == "blinkoff")
                    info.blinkoff = (int)val.as_int();
            }
        }
        modes_.push_back(std::move(info));
    }
}

void UiEventHandler::handle_mode_change(const MpackValue& args)
{
    if (args.type() != MpackValue::Array || args.as_array().size() < 2)
        return;
    current_mode_ = (int)args.as_array()[1].as_int();
    if (on_mode_change)
        on_mode_change(current_mode_);
}

void UiEventHandler::handle_option_set(const MpackValue& args)
{
    if (args.type() != MpackValue::Array || args.as_array().size() < 2)
        return;
    const auto& args_array = args.as_array();
    const std::string& name = args_array[0].as_str();
    if (on_option_set)
        on_option_set(name, args_array[1]);
}

} // namespace spectre
