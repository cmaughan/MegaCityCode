#pragma once
#include "renderer/types.h"
#include <unordered_map>
#include <cstdint>

namespace spectre {

struct HlAttr {
    Color fg = { 1.0f, 1.0f, 1.0f, 1.0f };
    Color bg = { 0.0f, 0.0f, 0.0f, 0.0f };  // alpha 0 = use default
    Color sp = { 1.0f, 1.0f, 1.0f, 1.0f };    // Special (underline) color
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool undercurl = false;
    bool strikethrough = false;
    bool reverse = false;

    uint32_t style_flags() const {
        uint32_t f = 0;
        if (bold) f |= 1;
        if (italic) f |= 2;
        if (underline) f |= 4;
        if (strikethrough) f |= 8;
        if (undercurl) f |= 16;
        return f;
    }
};

class HighlightTable {
public:
    void set(uint16_t id, const HlAttr& attr) { attrs_[id] = attr; }
    const HlAttr& get(uint16_t id) const {
        auto it = attrs_.find(id);
        if (it != attrs_.end()) return it->second;
        return default_;
    }

    void set_default_fg(Color c) { default_fg_ = c; default_.fg = c; }
    void set_default_bg(Color c) { default_bg_ = c; default_.bg = c; }
    void set_default_sp(Color c) { default_.sp = c; }

    Color default_fg() const { return default_fg_; }
    Color default_bg() const { return default_bg_; }

    // Resolve colors: apply defaults and reverse
    void resolve(const HlAttr& attr, Color& fg, Color& bg) const {
        fg = (attr.fg.a > 0) ? attr.fg : default_fg_;
        bg = (attr.bg.a > 0) ? attr.bg : default_bg_;
        if (attr.reverse) std::swap(fg, bg);
    }

private:
    std::unordered_map<uint16_t, HlAttr> attrs_;
    Color default_fg_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    Color default_bg_ = { 0.1f, 0.1f, 0.1f, 1.0f };
    HlAttr default_ = {};
};

} // namespace spectre
