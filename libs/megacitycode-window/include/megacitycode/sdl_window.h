#pragma once
#include <SDL3/SDL.h>
#include <megacitycode/window.h>

struct SDL_Window;

namespace megacitycode
{

class SdlWindow : public IWindow
{
public:
    void set_clamp_to_display(bool clamp)
    {
        clamp_to_display_ = clamp;
    }
    bool initialize(const std::string& title, int width, int height) override;
    void shutdown() override;
    bool poll_events() override;
    bool wait_events(int timeout_ms);
    void activate();
    void wake();
    void* native_handle() override
    {
        return window_;
    }
    std::pair<int, int> size_logical() const override;
    std::pair<int, int> size_pixels() const override;
    float display_ppi() const override;
    void set_title(const std::string& title) override;
    std::string clipboard_text() const override;
    bool set_clipboard_text(const std::string& text) override;
    void set_text_input_area(int x, int y, int w, int h) override;

private:
    bool handle_event(const SDL_Event& event);
    SDL_Window* window_ = nullptr;
    unsigned int wake_event_type_ = 0;
    bool clamp_to_display_ = true;
};

} // namespace megacitycode
