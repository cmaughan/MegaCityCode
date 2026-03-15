#include <SDL3/SDL.h>
#include <spectre/sdl_window.h>
#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <SDL3/SDL_metal.h>
#elif defined(_WIN32)
#include <SDL3/SDL_vulkan.h>
#include <shellscalingapi.h>
#include <windows.h>
#else
#include <SDL3/SDL_vulkan.h>
#endif
#include <cmath>
#include <spectre/log.h>

namespace spectre
{

#if defined(_WIN32) || defined(__APPLE__)
static void log_display_info(SDL_Window* window)
{
    SPECTRE_LOG_DEBUG(LogCategory::Window, "Display / DPI diagnostics begin");

    // SDL window sizes
    int lw = 0, lh = 0, pw = 0, ph = 0;
    SDL_GetWindowSize(window, &lw, &lh);
    SDL_GetWindowSizeInPixels(window, &pw, &ph);
    float sdl_scale = SDL_GetWindowDisplayScale(window);
    SPECTRE_LOG_DEBUG(LogCategory::Window, "SDL logical size: %d x %d", lw, lh);
    SPECTRE_LOG_DEBUG(LogCategory::Window, "SDL pixel size: %d x %d", pw, ph);
    SPECTRE_LOG_DEBUG(LogCategory::Window, "SDL display scale: %.3f", sdl_scale);
    SPECTRE_LOG_DEBUG(LogCategory::Window, "SDL effective DPI: %.1f", 96.0f * sdl_scale);

#ifdef __APPLE__
    // CoreGraphics display info
    CGDirectDisplayID displayID = CGMainDisplayID();
    CGSize physicalSize = CGDisplayScreenSize(displayID); // millimeters
    size_t cgLogicalW = CGDisplayPixelsWide(displayID);
    size_t cgLogicalH = CGDisplayPixelsHigh(displayID);
    size_t cgPhysicalW = cgLogicalW, cgPhysicalH = cgLogicalH;
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displayID);
    if (mode)
    {
        cgPhysicalW = CGDisplayModeGetPixelWidth(mode);
        cgPhysicalH = CGDisplayModeGetPixelHeight(mode);
        CGDisplayModeRelease(mode);
    }
    SPECTRE_LOG_DEBUG(LogCategory::Window, "CG logical pixels: %zu x %zu", cgLogicalW, cgLogicalH);
    SPECTRE_LOG_DEBUG(LogCategory::Window, "CG physical pixels: %zu x %zu", cgPhysicalW, cgPhysicalH);
    SPECTRE_LOG_DEBUG(LogCategory::Window, "CG physical size: %.1f x %.1f mm", physicalSize.width, physicalSize.height);
    if (physicalSize.width > 0)
    {
        float ppi_logical = (float)(cgLogicalW / (physicalSize.width / 25.4));
        float ppi_physical = (float)(cgPhysicalW / (physicalSize.width / 25.4));
        SPECTRE_LOG_DEBUG(LogCategory::Window, "Computed PPI (logical): %.1f", ppi_logical);
        SPECTRE_LOG_DEBUG(LogCategory::Window, "Computed PPI (physical): %.1f", ppi_physical);
    }
    SDL_DisplayID sdlDisplay = SDL_GetDisplayForWindow(window);
    float contentScale = sdlDisplay ? SDL_GetDisplayContentScale(sdlDisplay) : 1.0f;
    SPECTRE_LOG_DEBUG(LogCategory::Window, "SDL content scale: %.3f", contentScale);
#endif

#ifdef _WIN32
    // Win32 HWND-based queries
    HWND hwnd = (HWND)SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if (hwnd)
    {
        // Per-monitor DPI variants
        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        UINT eff_x = 0, eff_y = 0, ang_x = 0, ang_y = 0, raw_x = 0, raw_y = 0;
        GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &eff_x, &eff_y);
        GetDpiForMonitor(monitor, MDT_ANGULAR_DPI, &ang_x, &ang_y);
        GetDpiForMonitor(monitor, MDT_RAW_DPI, &raw_x, &raw_y);
        SPECTRE_LOG_DEBUG(LogCategory::Window, "MDT_EFFECTIVE_DPI: %u x %u", eff_x, eff_y);
        SPECTRE_LOG_DEBUG(LogCategory::Window, "MDT_ANGULAR_DPI: %u x %u", ang_x, ang_y);
        SPECTRE_LOG_DEBUG(LogCategory::Window, "MDT_RAW_DPI: %u x %u", raw_x, raw_y);

        // Monitor logical/physical rect
        MONITORINFOEX mi = {};
        mi.cbSize = sizeof(mi);
        GetMonitorInfo(monitor, &mi);
        int mon_lw = mi.rcMonitor.right - mi.rcMonitor.left;
        int mon_lh = mi.rcMonitor.bottom - mi.rcMonitor.top;
        SPECTRE_LOG_DEBUG(LogCategory::Window, "Monitor logical rect: %d x %d (%s)",
            mon_lw, mon_lh,
            (mi.dwFlags & MONITORINFOF_PRIMARY) ? "primary" : "non-primary");

        // Physical pixel resolution via DEVMODE
        DEVMODE dm = {};
        dm.dmSize = sizeof(dm);
        if (EnumDisplaySettings(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm))
        {
            SPECTRE_LOG_DEBUG(LogCategory::Window, "Monitor device pixels: %lu x %lu",
                dm.dmPelsWidth, dm.dmPelsHeight);
            // Compute PPI from physical pixels + physical mm (via GetDeviceCaps)
            HDC hdc = CreateDC(NULL, mi.szDevice, NULL, NULL);
            if (hdc)
            {
                int mm_w = GetDeviceCaps(hdc, HORZSIZE);
                int mm_h = GetDeviceCaps(hdc, VERTSIZE);
                int log_px_w = GetDeviceCaps(hdc, HORZRES);
                int log_px_h = GetDeviceCaps(hdc, VERTRES);
                int desk_px_w = GetDeviceCaps(hdc, DESKTOPHORZRES);
                int desk_px_h = GetDeviceCaps(hdc, DESKTOPVERTRES);
                int logpx_dpi = GetDeviceCaps(hdc, LOGPIXELSX);
                DeleteDC(hdc);
                SPECTRE_LOG_DEBUG(LogCategory::Window, "GetDeviceCaps HORZSIZE/VERTSIZE: %d x %d mm", mm_w, mm_h);
                SPECTRE_LOG_DEBUG(LogCategory::Window, "GetDeviceCaps HORZRES/VERTRES: %d x %d", log_px_w, log_px_h);
                SPECTRE_LOG_DEBUG(LogCategory::Window, "GetDeviceCaps DESKTOPHORZRES/VERTRES: %d x %d", desk_px_w, desk_px_h);
                SPECTRE_LOG_DEBUG(LogCategory::Window, "GetDeviceCaps LOGPIXELSX: %d", logpx_dpi);
                if (mm_w > 0)
                {
                    float phys_ppi = dm.dmPelsWidth / (mm_w / 25.4f);
                    SPECTRE_LOG_DEBUG(LogCategory::Window, "Computed physical PPI (DEVMODE/mm): %.1f", phys_ppi);
                }
            }
        }
    }
#endif
    SPECTRE_LOG_DEBUG(LogCategory::Window, "Display / DPI diagnostics end");
}
#endif // _WIN32 || __APPLE__

bool SdlWindow::initialize(const std::string& title, int width, int height)
{
    SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "1");
    SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_RAISED, "1");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        SPECTRE_LOG_ERROR(LogCategory::Window, "SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    wake_event_type_ = SDL_RegisterEvents(1);
    if (wake_event_type_ == static_cast<Uint32>(-1))
    {
        SPECTRE_LOG_ERROR(LogCategory::Window, "SDL_RegisterEvents failed: %s", SDL_GetError());
        return false;
    }

#ifdef __APPLE__
    Uint64 window_flags = SDL_WINDOW_METAL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#else
    Uint64 window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#endif

    SDL_Rect usable_bounds = {};
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    if (display && SDL_GetDisplayUsableBounds(display, &usable_bounds))
    {
        int max_width = usable_bounds.w - 80;
        int max_height = usable_bounds.h - 80;
        if (max_width < 640)
            max_width = 640;
        if (max_height < 400)
            max_height = 400;
        if (width > max_width)
            width = max_width;
        if (height > max_height)
            height = max_height;
    }

    window_ = SDL_CreateWindow(
        title.c_str(),
        width, height,
        window_flags);

    if (!window_)
    {
        SPECTRE_LOG_ERROR(LogCategory::Window, "SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    // Enable text input events (required in SDL3)
    SDL_StartTextInput(window_);

#if defined(_WIN32) || defined(__APPLE__)
    if (log_would_emit(LogLevel::Debug, LogCategory::Window))
        log_display_info(window_);
#endif

    return true;
}

void SdlWindow::activate()
{
    if (!window_)
        return;

    SDL_ShowWindow(window_);
    SDL_RaiseWindow(window_);
    SDL_SyncWindow(window_);

#ifdef _WIN32
    HWND hwnd = (HWND)SDL_GetPointerProperty(
        SDL_GetWindowProperties(window_), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if (hwnd)
    {
        HWND foreground = GetForegroundWindow();
        DWORD current_thread = GetCurrentThreadId();
        DWORD foreground_thread = foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;

        ShowWindow(hwnd, IsIconic(hwnd) ? SW_RESTORE : SW_SHOW);
        if (foreground_thread && foreground_thread != current_thread)
            AttachThreadInput(foreground_thread, current_thread, TRUE);

        BringWindowToTop(hwnd);
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);

        if (foreground_thread && foreground_thread != current_thread)
            AttachThreadInput(foreground_thread, current_thread, FALSE);
    }
#endif
}

void SdlWindow::shutdown()
{
    if (window_)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

bool SdlWindow::handle_event(const SDL_Event& event)
{
    if (event.type == wake_event_type_)
        return true;

    switch (event.type)
    {
    case SDL_EVENT_QUIT:
        return false;

    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        if (on_resize)
        {
            // Use pixel dimensions (not points) for correct Retina/HiDPI rendering
            int pw, ph;
            SDL_GetWindowSizeInPixels(window_, &pw, &ph);
            on_resize({ pw, ph });
        }
        break;

    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        if (on_key)
        {
            on_key({ (int)event.key.scancode,
                (int)event.key.key,
                event.key.mod,
                event.type == SDL_EVENT_KEY_DOWN });
        }
        break;

    case SDL_EVENT_TEXT_INPUT:
        if (on_text_input)
        {
            on_text_input({ event.text.text });
        }
        break;

    case SDL_EVENT_TEXT_EDITING:
        if (on_text_editing)
        {
            on_text_editing({ event.edit.text, event.edit.start, event.edit.length });
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (on_mouse_button)
        {
            on_mouse_button({ (int)event.button.button,
                event.type == SDL_EVENT_MOUSE_BUTTON_DOWN,
                SDL_GetModState(),
                (int)event.button.x,
                (int)event.button.y });
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (on_mouse_move)
        {
            on_mouse_move({ SDL_GetModState(), (int)event.motion.x, (int)event.motion.y });
        }
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        if (on_mouse_wheel)
        {
            on_mouse_wheel({ event.wheel.x, event.wheel.y,
                SDL_GetModState(),
                (int)event.wheel.mouse_x, (int)event.wheel.mouse_y });
        }
        break;
    }

    return true;
}

bool SdlWindow::poll_events()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (!handle_event(event))
            return false;
    }
    return true;
}

bool SdlWindow::wait_events(int timeout_ms)
{
    SDL_Event event;
    SDL_ClearError();
    bool got_event = timeout_ms < 0 ? SDL_WaitEvent(&event) : SDL_WaitEventTimeout(&event, timeout_ms);
    if (!got_event)
    {
        const char* err = SDL_GetError();
        if (err && err[0] != '\0')
        {
            SPECTRE_LOG_WARN(LogCategory::Window, "SDL wait failed: %s", err);
        }
        return true;
    }

    if (!handle_event(event))
        return false;

    return poll_events();
}

void SdlWindow::wake()
{
    if (!window_ || wake_event_type_ == 0 || wake_event_type_ == static_cast<Uint32>(-1))
        return;

    SDL_Event event = {};
    event.type = wake_event_type_;
    SDL_PushEvent(&event);
}

std::pair<int, int> SdlWindow::size_pixels() const
{
    int w = 0, h = 0;
    if (window_)
    {
        SDL_GetWindowSizeInPixels(window_, &w, &h);
    }
    return { w, h };
}

std::pair<int, int> SdlWindow::size_logical() const
{
    int w = 0, h = 0;
    if (window_)
    {
        SDL_GetWindowSize(window_, &w, &h);
    }
    return { w, h };
}

float SdlWindow::display_ppi() const
{
#ifdef __APPLE__
    // Get actual physical PPI from CoreGraphics.
    // CGDisplayPixelsWide returns logical pixels on HiDPI displays (e.g. 1920
    // on a 4K display at 2x scale). CGDisplayModeGetPixelWidth returns the true
    // hardware pixel count (e.g. 3840), which is what we need for correct sizing.
    CGDirectDisplayID displayID = CGMainDisplayID();
    CGSize physicalSize = CGDisplayScreenSize(displayID); // millimeters
    size_t pixelWidth = 0;
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displayID);
    if (mode)
    {
        pixelWidth = CGDisplayModeGetPixelWidth(mode);
        CGDisplayModeRelease(mode);
    }
    if (pixelWidth == 0)
        pixelWidth = CGDisplayPixelsWide(displayID); // fallback
    if (physicalSize.width > 0)
    {
        return (float)(pixelWidth / (physicalSize.width / 25.4));
    }
#elif defined(_WIN32)
    // Windows: get actual physical DPI from monitor hardware (EDID)
    if (window_)
    {
        HWND hwnd = (HWND)SDL_GetPointerProperty(
            SDL_GetWindowProperties(window_),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        if (hwnd)
        {
            HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            UINT dpi_x = 0, dpi_y = 0;
            if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_RAW_DPI, &dpi_x, &dpi_y)) && dpi_x > 0)
            {
                return (float)dpi_x;
            }
        }
    }
#endif
    return 96.0f; // fallback
}

void SdlWindow::set_title(const std::string& title)
{
    if (window_)
        SDL_SetWindowTitle(window_, title.c_str());
}

std::string SdlWindow::clipboard_text() const
{
    char* text = SDL_GetClipboardText();
    if (!text)
        return {};

    std::string value = text;
    SDL_free(text);
    return value;
}

bool SdlWindow::set_clipboard_text(const std::string& text)
{
    return SDL_SetClipboardText(text.c_str());
}

void SdlWindow::set_text_input_area(int x, int y, int w, int h)
{
    if (!window_)
        return;

    SDL_Rect area = { x, y, w, h };
    SDL_SetTextInputArea(window_, &area, 0);
}

} // namespace spectre
