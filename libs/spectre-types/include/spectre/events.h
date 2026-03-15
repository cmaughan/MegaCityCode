#pragma once
#include <cstdint>

namespace spectre
{

// Event types for window callbacks
struct WindowResizeEvent
{
    int width, height;
};
struct KeyEvent
{
    int scancode;
    int keycode;
    uint16_t mod;
    bool pressed;
};
struct TextInputEvent
{
    const char* text;
};
struct TextEditingEvent
{
    const char* text;
    int start = 0;
    int length = 0;
};
struct MouseButtonEvent
{
    int button;
    bool pressed;
    uint16_t mod;
    int x, y;
};
struct MouseMoveEvent
{
    uint16_t mod;
    int x, y;
};
struct MouseWheelEvent
{
    float dx, dy;
    uint16_t mod;
    int x, y;
};

} // namespace spectre
