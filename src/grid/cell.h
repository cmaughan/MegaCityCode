#pragma once
#include <cstdint>

namespace spectre {

struct Cell {
    uint32_t codepoint = ' ';
    uint16_t hl_attr_id = 0;
    bool dirty = true;
    bool double_width = false;   // This cell is left half of a double-width char
    bool double_width_cont = false; // This cell is right half (continuation)
};

} // namespace spectre
