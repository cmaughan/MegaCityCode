#include "app.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <spectre/log.h>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
namespace
{

bool has_flag(const char* cmdline, std::string_view flag)
{
    return cmdline && std::strstr(cmdline, flag.data());
}

} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int)
{
    const bool want_console = has_flag(lpCmdLine, "--console");
    const bool smoke_test = has_flag(lpCmdLine, "--smoke-test");

    if (want_console)
    {
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }

    wchar_t exe_buf[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exe_buf, MAX_PATH))
    {
        auto exe_path = std::filesystem::path(exe_buf).parent_path();
        if (!exe_path.empty())
        {
            std::filesystem::current_path(exe_path);
        }
    }
#else
int main(int argc, char* argv[])
{
    bool smoke_test = false;
    for (int i = 1; i < argc; i++)
    {
        if (std::strcmp(argv[i], "--smoke-test") == 0)
            smoke_test = true;
    }

    if (argc > 0)
    {
        auto exe_path = std::filesystem::path(argv[0]).parent_path();
        if (!exe_path.empty())
        {
            std::filesystem::current_path(exe_path);
        }
    }
#endif

    spectre::configure_default_logging();

    spectre::App app;

    if (!app.initialize())
    {
        SPECTRE_LOG_ERROR(spectre::LogCategory::App, "Failed to initialize spectre");
        spectre::shutdown_logging();
        return 1;
    }

    int status = 0;
    if (smoke_test)
    {
        if (!app.run_smoke_test(std::chrono::seconds(3)))
            status = 1;
    }
    else
    {
        app.run();
    }
    app.shutdown();
    spectre::shutdown_logging();

    return status;
}
