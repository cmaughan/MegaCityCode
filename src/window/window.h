#pragma once
#include <string>
#include <functional>
#include <cstdint>

struct SDL_Window;

namespace spectre {

// Event types for window callbacks
struct WindowResizeEvent { int width, height; };
struct KeyEvent { int scancode; int keycode; uint16_t mod; bool pressed; };
struct TextInputEvent { const char* text; };
struct MouseButtonEvent { int button; bool pressed; int x, y; };
struct MouseMoveEvent { int x, y; };
struct MouseWheelEvent { float dx, dy; int x, y; };

class IWindow {
public:
    virtual ~IWindow() = default;
    virtual bool initialize(const std::string& title, int width, int height) = 0;
    virtual void shutdown() = 0;
    virtual bool poll_events() = 0; // returns false if quit requested
    virtual SDL_Window* native_handle() = 0;
    virtual std::pair<int,int> size_pixels() const = 0;
    virtual float display_ppi() const = 0; // Physical pixels per inch of the display

    // Callbacks
    std::function<void(const WindowResizeEvent&)> on_resize;
    std::function<void(const KeyEvent&)> on_key;
    std::function<void(const TextInputEvent&)> on_text_input;
    std::function<void(const MouseButtonEvent&)> on_mouse_button;
    std::function<void(const MouseMoveEvent&)> on_mouse_move;
    std::function<void(const MouseWheelEvent&)> on_mouse_wheel;
};

} // namespace spectre
