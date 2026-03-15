#include <cstdio>
#include <spectre/nvim.h>
#include <spectre/unicode.h>

namespace spectre
{

namespace
{

const MpackValue::ArrayStorage* try_get_array(const MpackValue& value)
{
    if (value.type() != MpackValue::Array)
        return nullptr;
    return &value.as_array();
}

const std::string* try_get_string(const MpackValue& value)
{
    if (value.type() != MpackValue::String)
        return nullptr;
    return &value.as_str();
}

bool try_get_int(const MpackValue& value, int& out)
{
    if (value.type() != MpackValue::Int && value.type() != MpackValue::UInt)
        return false;
    out = (int)value.as_int();
    return true;
}

} // namespace

void UiEventHandler::process_redraw(const std::vector<MpackValue>& params)
{
    for (size_t ei = 0; ei < params.size(); ei++)
    {
        auto& event = params[ei];
        const auto* event_array = try_get_array(event);
        if (!event_array || event_array->empty())
            continue;

        const auto* name = try_get_string((*event_array)[0]);
        if (!name)
            continue;

        if (*name == "flush")
        {
            if (on_flush)
                on_flush();
            continue;
        }

        if (*name == "busy_start")
        {
            if (on_busy)
                on_busy(true);
            continue;
        }

        if (*name == "busy_stop")
        {
            if (on_busy)
                on_busy(false);
            continue;
        }

        for (size_t i = 1; i < event_array->size(); i++)
        {
            const auto& args = (*event_array)[i];

            if (*name == "grid_line")
                handle_grid_line(args);
            else if (*name == "grid_cursor_goto")
                handle_grid_cursor_goto(args);
            else if (*name == "grid_scroll")
                handle_grid_scroll(args);
            else if (*name == "grid_clear")
                handle_grid_clear(args);
            else if (*name == "grid_resize")
                handle_grid_resize(args);
            else if (*name == "hl_attr_define")
                handle_hl_attr_define(args);
            else if (*name == "default_colors_set")
                handle_default_colors_set(args);
            else if (*name == "mode_info_set")
                handle_mode_info_set(args);
            else if (*name == "mode_change")
                handle_mode_change(args);
            else if (*name == "option_set")
                handle_option_set(args);
            else if (*name == "set_title")
                handle_set_title(args);
        }
    }
}

void UiEventHandler::handle_grid_line(const MpackValue& args)
{
    if (!grid_)
        return;

    const auto* args_array = try_get_array(args);
    if (!args_array || args_array->size() < 4)
        return;

    int row = 0;
    int col_start = 0;
    if (!try_get_int((*args_array)[1], row) || !try_get_int((*args_array)[2], col_start))
        return;

    int col = col_start;
    uint16_t current_hl = 0;

    const auto* cells = try_get_array((*args_array)[3]);
    if (!cells)
        return;

    for (const auto& cell : *cells)
    {
        const auto* cell_array = try_get_array(cell);
        if (!cell_array || cell_array->empty())
            continue;

        const auto* text = try_get_string((*cell_array)[0]);
        if (!text)
            continue;

        if (cell_array->size() >= 2 && !(*cell_array)[1].is_nil())
        {
            int hl_id = 0;
            if (!try_get_int((*cell_array)[1], hl_id))
                continue;
            current_hl = (uint16_t)hl_id;
        }

        int repeat = 1;
        if (cell_array->size() >= 3)
        {
            if (!try_get_int((*cell_array)[2], repeat) || repeat < 1)
                continue;
        }

        bool dw = cluster_cell_width(*text, options_ ? *options_ : UiOptions{}) == 2;
        for (int r = 0; r < repeat; r++)
        {
            grid_->set_cell(col, row, *text, current_hl, dw);
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
    int cols = args_array.size() >= 7 ? (int)args_array[6].as_int() : 0;

    grid_->scroll(top, bot, left, right, rows, cols);
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
                hl.has_fg = true;
            }
            else if (k == "background")
            {
                hl.bg = Color::from_rgb((uint32_t)val.as_int());
                hl.has_bg = true;
            }
            else if (k == "special")
            {
                hl.sp = Color::from_rgb((uint32_t)val.as_int());
                hl.has_sp = true;
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

void UiEventHandler::handle_set_title(const MpackValue& args)
{
    if (args.type() != MpackValue::Array || args.as_array().empty())
        return;

    const auto& args_array = args.as_array();
    if (on_title)
        on_title(args_array[0].as_str());
}

} // namespace spectre
