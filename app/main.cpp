#include "app.h"
#include "render_test.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <megacitycode/log.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <shellapi.h>
#include <windows.h>
#endif

namespace
{

#ifdef _WIN32
std::vector<std::string> command_line_args()
{
    std::vector<std::string> args;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        return args;

    for (int i = 0; i < argc; ++i)
    {
        int size = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
        if (size <= 1)
        {
            args.emplace_back();
            continue;
        }
        std::string converted(size - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, converted.data(), static_cast<int>(converted.size()), nullptr, nullptr);
        args.push_back(std::move(converted));
    }
    LocalFree(argv);
    return args;
}
#else
std::vector<std::string> command_line_args(int argc, char* argv[])
{
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i)
        args.emplace_back(argv[i]);
    return args;
}
#endif

struct ParsedArgs
{
    bool want_console = false;
    bool smoke_test = false;
    bool bless_render_test = false;
    std::filesystem::path render_test_path;
    std::filesystem::path export_render_test_path;
};

ParsedArgs parse_args(const std::vector<std::string>& args)
{
    ParsedArgs parsed;
    for (size_t i = 1; i < args.size(); ++i)
    {
        if (args[i] == "--console")
            parsed.want_console = true;
        else if (args[i] == "--smoke-test")
            parsed.smoke_test = true;
        else if (args[i] == "--bless-render-test")
            parsed.bless_render_test = true;
        else if (args[i] == "--render-test" && i + 1 < args.size())
            parsed.render_test_path = args[++i];
        else if (args[i] == "--export-render-test" && i + 1 < args.size())
            parsed.export_render_test_path = args[++i];
    }
    return parsed;
}

void update_current_directory(const std::vector<std::string>& args)
{
    if (args.empty())
        return;

    auto exe_path = std::filesystem::path(args.front()).parent_path();
    if (!exe_path.empty())
        std::filesystem::current_path(exe_path);
}

} // namespace

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    auto args = command_line_args();
#else
int main(int argc, char* argv[])
{
    auto args = command_line_args(argc, argv);
#endif
    ParsedArgs parsed = parse_args(args);

#ifdef _WIN32
    if (parsed.want_console)
    {
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
#endif

    update_current_directory(args);

    megacitycode::configure_default_logging();

    std::optional<megacitycode::RenderTestScenario> render_test;
    megacitycode::AppOptions options;
    if (!parsed.render_test_path.empty())
    {
        std::string load_error;
        render_test = megacitycode::load_render_test_scenario(parsed.render_test_path, &load_error);
        if (!render_test)
        {
            MEGACITYCODE_LOG_ERROR(megacitycode::LogCategory::App, "%s", load_error.c_str());
            megacitycode::shutdown_logging();
            return 1;
        }
        options = render_test->make_app_options();
    }

    megacitycode::App app(std::move(options));

    if (!app.initialize())
    {
        MEGACITYCODE_LOG_ERROR(megacitycode::LogCategory::App, "Failed to initialize megacitycode");
        megacitycode::shutdown_logging();
        return 1;
    }

    int status = 0;
    if (render_test)
    {
        auto frame = app.run_render_test(std::chrono::milliseconds(render_test->timeout_ms),
            std::chrono::milliseconds(render_test->settle_ms));
        if (!frame)
        {
            megacitycode::write_render_test_failure_report(*render_test, app.last_render_test_error());
            status = 1;
        }
        else
        {
            std::string finalize_error;
            bool ok = false;
            if (!parsed.export_render_test_path.empty())
                ok = megacitycode::export_render_test_frame(parsed.export_render_test_path, *frame, &finalize_error);
            else
                ok = megacitycode::finalize_render_test_result(*render_test, *frame, parsed.bless_render_test, &finalize_error);

            if (!ok)
            {
                megacitycode::write_render_test_failure_report(*render_test, finalize_error);
                MEGACITYCODE_LOG_ERROR(megacitycode::LogCategory::App, "%s", finalize_error.c_str());
                status = 1;
            }
        }
    }
    else if (parsed.smoke_test)
    {
        if (!app.run_smoke_test(std::chrono::seconds(3)))
            status = 1;
    }
    else
    {
        app.run();
    }
    app.shutdown();
    megacitycode::shutdown_logging();

    return status;
}
