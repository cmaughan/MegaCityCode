#pragma once
#include "window/window.h"
#include "nvim/rpc.h"
#include <string>

namespace spectre {

class NvimInput {
public:
    void initialize(NvimRpc* rpc, int cell_w, int cell_h);
    void set_cell_size(int w, int h) { cell_w_ = w; cell_h_ = h; }

    // Handle SDL events
    void on_key(const KeyEvent& event);
    void on_text_input(const TextInputEvent& event);
    void on_mouse_button(const MouseButtonEvent& event);
    void on_mouse_move(const MouseMoveEvent& event);
    void on_mouse_wheel(const MouseWheelEvent& event);

private:
    void send_input(const std::string& keys);
    std::string translate_key(int keycode, uint16_t mod);

    NvimRpc* rpc_ = nullptr;
    int cell_w_ = 10, cell_h_ = 20;
    bool suppress_next_text_ = false;
    bool mouse_pressed_ = false;
};

} // namespace spectre
