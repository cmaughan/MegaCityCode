#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace megacitycode
{

enum class LogLevel
{
    Error = 0,
    Warn = 1,
    Info = 2,
    Debug = 3,
    Trace = 4,
};

enum class LogCategory
{
    App,
    Rpc,
    Window,
    Font,
    Renderer,
    Input,
    Test,
};

struct LogRecord
{
    LogLevel level = LogLevel::Info;
    LogCategory category = LogCategory::App;
    std::string message;
};

struct LogOptions
{
    LogLevel min_level = LogLevel::Info;
    bool enable_stderr = true;
    bool enable_file = false;
    std::string file_path;
    std::vector<LogCategory> enabled_categories;
};

using LogSink = std::function<void(const LogRecord&)>;

void configure_logging(LogOptions options = {});
void configure_default_logging(const char* default_file_name = "megacitycode.log", bool prefer_file_when_no_console = true);
void shutdown_logging();

bool log_would_emit(LogLevel level, LogCategory category);
void set_log_sink(LogSink sink);
void clear_log_sink();

void log_message(LogLevel level, LogCategory category, std::string_view message);
void log_printf(LogLevel level, LogCategory category, const char* fmt, ...);

const char* to_string(LogLevel level);
const char* to_string(LogCategory category);

} // namespace megacitycode

#define MEGACITYCODE_LOG_ERROR(category, ...) ::megacitycode::log_printf(::megacitycode::LogLevel::Error, category, __VA_ARGS__)
#define MEGACITYCODE_LOG_WARN(category, ...) ::megacitycode::log_printf(::megacitycode::LogLevel::Warn, category, __VA_ARGS__)
#define MEGACITYCODE_LOG_INFO(category, ...) ::megacitycode::log_printf(::megacitycode::LogLevel::Info, category, __VA_ARGS__)
#define MEGACITYCODE_LOG_DEBUG(category, ...) ::megacitycode::log_printf(::megacitycode::LogLevel::Debug, category, __VA_ARGS__)
#define MEGACITYCODE_LOG_TRACE(category, ...) ::megacitycode::log_printf(::megacitycode::LogLevel::Trace, category, __VA_ARGS__)
