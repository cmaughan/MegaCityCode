#include "render_test.h"
#include "test_support.h"
#include "toml_util.h"

#include <filesystem>
#include <fstream>
#include <string>

using spectre::tests::expect;
using spectre::tests::expect_eq;
using spectre::tests::run_test;
using namespace spectre::toml;

namespace
{

void write_text_file(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::trunc);
    out << text;
}

} // namespace

void run_render_test_parser_tests()
{
    // --- Scenario loading ---

    run_test("multiline commands array parses", [] {
        const auto dir = std::filesystem::temp_directory_path() / "spectre-render-test-parser-multi";
        const auto path = dir / "multiline.toml";
        write_text_file(path,
            "name = \"multiline\"\n"
            "width = 900\n"
            "height = 700\n"
            "debug_overlay = true\n"
            "commands = [\n"
            "  \"edit ${PROJECT_ROOT}/README.md\",\n"
            "  \"set nowrap\",\n"
            "  \"lua vim.cmd([[normal! ggzt]])\",\n"
            "]\n");

        std::string err;
        auto scenario = spectre::load_render_test_scenario(path, &err);
        expect(scenario.has_value(), "multiline render scenario should load");
        expect_eq(scenario->name, std::string("multiline"), "name");
        expect_eq(scenario->width, 900, "width");
        expect_eq(scenario->height, 700, "height");
        expect(scenario->debug_overlay, "debug_overlay flag");
        expect_eq(static_cast<int>(scenario->commands.size()), 3, "command count");
        expect_eq(scenario->commands[0],
            std::string("edit " + (dir.parent_path().parent_path() / "README.md").lexically_normal().generic_string()),
            "project root placeholder expands");
        expect_eq(scenario->commands[1], std::string("set nowrap"), "second command");
        expect_eq(scenario->commands[2], std::string("lua vim.cmd([[normal! ggzt]])"), "third command");

        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    });

    run_test("missing commands field returns error", [] {
        const auto dir = std::filesystem::temp_directory_path() / "spectre-render-test-parser-nocmd";
        const auto path = dir / "nocmd.toml";
        write_text_file(path, "name = \"no-commands\"\nwidth = 800\n");

        std::string err;
        auto scenario = spectre::load_render_test_scenario(path, &err);
        expect(!scenario.has_value(), "missing commands should fail");
        expect(!err.empty(), "error message should be set");

        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    });

    run_test("hash comments are stripped in scenario files", [] {
        const auto dir = std::filesystem::temp_directory_path() / "spectre-render-test-parser-comment";
        const auto path = dir / "commented.toml";
        write_text_file(path,
            "# This is a comment\n"
            "width = 640 # inline comment\n"
            "height = 480\n"
            "commands = [\"echo\"]\n");

        std::string err;
        auto scenario = spectre::load_render_test_scenario(path, &err);
        expect(scenario.has_value(), "scenario with comments should load");
        expect_eq(scenario->width, 640, "width after stripping inline comment");
        expect_eq(scenario->height, 480, "height");

        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    });

    // --- toml_util helpers ---

    run_test("trim removes leading and trailing whitespace", [] {
        expect_eq(trim("  hello  "), std::string("hello"), "spaces");
        expect_eq(trim("\t\nhello\n"), std::string("hello"), "tabs and newlines");
        expect_eq(trim(""), std::string(""), "empty string");
        expect_eq(trim("   "), std::string(""), "only whitespace");
    });

    run_test("unquote strips double quotes", [] {
        expect_eq(unquote("\"hello\""), std::string("hello"), "quoted string");
        expect_eq(unquote("  \"hello\"  "), std::string("hello"), "quoted with surrounding spaces");
        expect_eq(unquote("hello"), std::string("hello"), "unquoted passthrough");
        expect_eq(unquote("\"\""), std::string(""), "empty quoted string");
    });

    run_test("parse_string_array handles basic comma-separated list", [] {
        auto items = parse_string_array("[\"a\", \"b\", \"c\"]");
        expect_eq(static_cast<int>(items.size()), 3, "three items");
        expect_eq(items[0], std::string("a"), "first");
        expect_eq(items[1], std::string("b"), "second");
        expect_eq(items[2], std::string("c"), "third");
    });

    run_test("parse_string_array handles backslash escape inside strings", [] {
        auto items = parse_string_array("[\"say \\\"hi\\\"\", \"path\\\\file\"]");
        expect_eq(static_cast<int>(items.size()), 2, "two items");
        expect_eq(items[0], std::string("say \"hi\""), "escaped quotes preserved");
        expect_eq(items[1], std::string("path\\file"), "escaped backslash preserved");
    });

    run_test("parse_string_array returns empty for non-array input", [] {
        expect_eq(static_cast<int>(parse_string_array("not an array").size()), 0, "plain string");
        expect_eq(static_cast<int>(parse_string_array("").size()), 0, "empty");
    });

    run_test("is_complete_array_literal detects open and closed arrays", [] {
        expect(is_complete_array_literal("[\"a\", \"b\"]"), "simple complete array");
        expect(!is_complete_array_literal("[\"a\", "), "incomplete array");
        expect(!is_complete_array_literal("not an array"), "plain string is not array");
        expect(is_complete_array_literal("[\"has \\\"quotes\\\"\"]"), "array with escaped quotes");
    });

    run_test("parse_bool parses true/false case-insensitively", [] {
        expect(parse_bool("true", false), "true");
        expect(parse_bool("TRUE", false), "TRUE");
        expect(parse_bool("True", false), "True");
        expect(!parse_bool("false", true), "false");
        expect(!parse_bool("FALSE", true), "FALSE");
        expect(parse_bool("garbage", true), "fallback on garbage");
    });

    run_test("parse_int and parse_double fall back on invalid input", [] {
        expect_eq(parse_int("42", 0), 42, "valid int");
        expect_eq(parse_int("not_a_number", 7), 7, "fallback on non-number");
        expect_eq(parse_double("3.14", 0.0), 3.14, "valid double");
        expect_eq(parse_double("nope", 1.5), 1.5, "fallback on non-double");
    });

    // --- JSON escaping ---

    run_test("json_escape_string escapes double quotes and backslashes", [] {
        expect_eq(json_escape_string("hello"), std::string("hello"), "plain passthrough");
        expect_eq(json_escape_string("say \"hi\""), std::string("say \\\"hi\\\""), "embedded quotes");
        expect_eq(json_escape_string("path\\file"), std::string("path\\\\file"), "backslash");
        expect_eq(json_escape_string("line1\nline2"), std::string("line1\\nline2"), "newline");
        expect_eq(json_escape_string("tab\there"), std::string("tab\\there"), "tab");
    });
}
