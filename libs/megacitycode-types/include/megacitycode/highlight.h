#pragma once

#include <cstdint>
#include <megacitycode/types.h>
#include <unordered_map>
#include <utility>

namespace megacitycode
{

struct HlAttr
{
    Color fg = { 1.0f, 1.0f, 1.0f, 1.0f };
    Color bg = { 0.0f, 0.0f, 0.0f, 1.0f };
    Color sp = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool has_fg = false;
    bool has_bg = false;
    bool has_sp = false;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool undercurl = false;
    bool strikethrough = false;
    bool reverse = false;

    uint32_t style_flags() const
    {
        uint32_t f = 0;
        if (bold)
            f |= 1;
        if (italic)
            f |= 2;
        if (underline)
            f |= 4;
        if (strikethrough)
            f |= 8;
        if (undercurl)
            f |= 16;
        return f;
    }
};

class HighlightTable
{
public:
    void set(uint16_t id, const HlAttr& attr)
    {
        attrs_[id] = attr;
    }

    const HlAttr& get(uint16_t id) const
    {
        auto it = attrs_.find(id);
        if (it != attrs_.end())
            return it->second;
        return default_;
    }

    void set_default_fg(Color c)
    {
        default_fg_ = c;
        default_.fg = c;
        default_.has_fg = true;
    }

    void set_default_bg(Color c)
    {
        default_bg_ = c;
        default_.bg = c;
        default_.has_bg = true;
    }

    void set_default_sp(Color c)
    {
        default_.sp = c;
        default_.has_sp = true;
    }

    Color default_fg() const
    {
        return default_fg_;
    }

    Color default_bg() const
    {
        return default_bg_;
    }

    void resolve(const HlAttr& attr, Color& fg, Color& bg, Color* sp = nullptr) const
    {
        fg = attr.has_fg ? attr.fg : default_fg_;
        bg = attr.has_bg ? attr.bg : default_bg_;
        if (attr.reverse)
            std::swap(fg, bg);
        if (sp)
            *sp = attr.has_sp ? attr.sp : fg;
    }

private:
    std::unordered_map<uint16_t, HlAttr> attrs_;
    Color default_fg_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    Color default_bg_ = { 0.1f, 0.1f, 0.1f, 1.0f };
    HlAttr default_ = {};
};

} // namespace megacitycode
