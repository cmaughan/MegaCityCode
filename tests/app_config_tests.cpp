#include "support/test_support.h"

#include "app_config.h"

using namespace spectre;
using namespace spectre::tests;

void run_app_config_tests()
{
    run_test("app config parse returns defaults for empty content", []() {
        AppConfig config = AppConfig::parse("");
        AppConfig defaults;
        expect_eq(config.window_width, defaults.window_width, "default window_width");
        expect_eq(config.window_height, defaults.window_height, "default window_height");
        expect_eq(config.font_size, defaults.font_size, "default font_size");
        expect(config.font_path.empty(), "default font_path is empty");
        expect(config.fallback_paths.empty(), "default fallback_paths is empty");
    });

    run_test("app config parse reads all fields", []() {
        const char* content =
            "window_width = 1920\n"
            "window_height = 1080\n"
            "font_size = 14\n"
            "font_path = \"/usr/share/fonts/mono.ttf\"\n"
            "fallback_paths = [\"/fonts/a.ttf\", \"/fonts/b.ttf\"]\n";

        AppConfig config = AppConfig::parse(content);
        expect_eq(config.window_width, 1920, "window_width parsed");
        expect_eq(config.window_height, 1080, "window_height parsed");
        expect_eq(config.font_size, 14, "font_size parsed");
        expect_eq(config.font_path, std::string("/usr/share/fonts/mono.ttf"), "font_path parsed");
        expect_eq(static_cast<int>(config.fallback_paths.size()), 2, "fallback_paths count");
        expect_eq(config.fallback_paths[0], std::string("/fonts/a.ttf"), "first fallback path");
        expect_eq(config.fallback_paths[1], std::string("/fonts/b.ttf"), "second fallback path");
    });

    run_test("app config parse ignores comments and blank lines", []() {
        const char* content =
            "# this is a comment\n"
            "\n"
            "window_width = 1600 # inline comment\n"
            "\n";

        AppConfig config = AppConfig::parse(content);
        expect_eq(config.window_width, 1600, "window_width parsed past comments");
        expect_eq(config.window_height, AppConfig{}.window_height, "window_height stays default");
    });

    run_test("app config parse clamps out-of-range window dimensions to defaults", []() {
        const char* content =
            "window_width = 99999\n"
            "window_height = -5\n";

        AppConfig config = AppConfig::parse(content);
        AppConfig defaults;
        expect_eq(config.window_width, defaults.window_width, "oversized width falls back to default");
        expect_eq(config.window_height, defaults.window_height, "negative height falls back to default");
    });

    run_test("app config parse clamps font_size to valid range", []() {
        AppConfig too_small = AppConfig::parse("font_size = 0\n");
        AppConfig too_large = AppConfig::parse("font_size = 9999\n");

        expect(too_small.font_size >= TextService::MIN_POINT_SIZE, "font_size clamped up to min");
        expect(too_large.font_size <= TextService::MAX_POINT_SIZE, "font_size clamped down to max");
    });

    run_test("app config serialize/parse round-trip preserves all fields", []() {
        AppConfig original;
        original.window_width = 1440;
        original.window_height = 900;
        original.font_size = 13;
        original.font_path = "/home/user/fonts/mono.ttf";
        original.fallback_paths = { "/fonts/emoji.ttf", "/fonts/cjk.ttf" };

        AppConfig round_tripped = AppConfig::parse(original.serialize());

        expect_eq(round_tripped.window_width, original.window_width, "window_width survives round-trip");
        expect_eq(round_tripped.window_height, original.window_height, "window_height survives round-trip");
        expect_eq(round_tripped.font_size, original.font_size, "font_size survives round-trip");
        expect_eq(round_tripped.font_path, original.font_path, "font_path survives round-trip");
        expect_eq(static_cast<int>(round_tripped.fallback_paths.size()),
            static_cast<int>(original.fallback_paths.size()), "fallback_paths count survives round-trip");
        expect_eq(round_tripped.fallback_paths[0], original.fallback_paths[0], "first fallback path survives");
        expect_eq(round_tripped.fallback_paths[1], original.fallback_paths[1], "second fallback path survives");
    });

    run_test("app config serialize clamps out-of-range values", []() {
        AppConfig config;
        config.window_width = 99999;
        config.font_size = 9999;

        AppConfig round_tripped = AppConfig::parse(config.serialize());
        expect(round_tripped.window_width <= 3840, "oversized width clamped on serialize");
        expect(round_tripped.font_size <= TextService::MAX_POINT_SIZE, "oversized font_size clamped on serialize");
    });
}
