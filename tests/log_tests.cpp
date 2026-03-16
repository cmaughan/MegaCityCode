#include "support/test_support.h"

#include <megacitycode/log.h>

#include <string>
#include <vector>

using namespace megacitycode;
using namespace megacitycode::tests;

namespace
{

struct ScopedLogCapture
{
    std::vector<LogRecord> records;

    ScopedLogCapture()
    {
        LogOptions options;
        options.enable_stderr = false;
        options.enable_file = false;
        configure_logging(options);
        set_log_sink([this](const LogRecord& record) { records.push_back(record); });
    }

    ~ScopedLogCapture()
    {
        clear_log_sink();
        shutdown_logging();
    }
};

} // namespace

void run_log_tests()
{
    run_test("logger filters by level", [] {
        ScopedLogCapture capture;
        LogOptions options;
        options.min_level = LogLevel::Warn;
        options.enable_stderr = false;
        configure_logging(options);
        set_log_sink([&capture](const LogRecord& record) { capture.records.push_back(record); });

        log_message(LogLevel::Info, LogCategory::App, "hidden");
        log_message(LogLevel::Warn, LogCategory::App, "shown");

        expect_eq(capture.records.size(), size_t(1), "logger should keep only warn or higher");
        expect_eq(capture.records[0].message, std::string("shown"), "logger should keep the emitted message");
    });

    run_test("logger filters by category", [] {
        ScopedLogCapture capture;
        LogOptions options;
        options.enable_stderr = false;
        options.enabled_categories = { LogCategory::Rpc };
        configure_logging(options);
        set_log_sink([&capture](const LogRecord& record) { capture.records.push_back(record); });

        log_message(LogLevel::Error, LogCategory::App, "skip");
        log_message(LogLevel::Error, LogCategory::Rpc, "keep");

        expect_eq(capture.records.size(), size_t(1), "logger should keep only enabled categories");
        expect_eq(capture.records[0].category, LogCategory::Rpc, "logger should keep the rpc category");
    });
}
