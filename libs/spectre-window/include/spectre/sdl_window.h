#pragma once
#include <SDL3/SDL.h>
#include <spectre/window.h>

struct SDL_Window;

namespace spectre
{

class SdlWindow : public IWindow
{
public:
    bool initialize(const std::string& title, int width, int height) override;
    void shutdown() override;
    bool poll_events() override;
    bool wait_events(int timeout_ms);
    void activate();
    void wake();
    SDL_Window* native_handle() override
    {
        return window_;
    }
    std::pair<int, int> size_pixels() const override;
    float display_ppi() const override;

private:
    bool handle_event(const SDL_Event& event);
    SDL_Window* window_ = nullptr;
    unsigned int wake_event_type_ = 0;
};

} // namespace spectre
