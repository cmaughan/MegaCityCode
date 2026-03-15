#include "render_test.h"
#include "test_support.h"

#include <filesystem>
#include <fstream>
#include <string>

using spectre::tests::expect;
using spectre::tests::expect_eq;

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
    const auto scenario_dir = std::filesystem::temp_directory_path() / "spectre-render-test-parser";
    const auto scenario_path = scenario_dir / "multiline.toml";

    write_text_file(scenario_path,
        "name = \"multiline\"\n"
        "width = 900\n"
        "height = 700\n"
        "commands = [\n"
        "  \"edit ${PROJECT_ROOT}/README.md\",\n"
        "  \"set nowrap\",\n"
        "  \"lua vim.cmd([[normal! ggzt]])\",\n"
        "]\n");

    std::string error;
    auto scenario = spectre::load_render_test_scenario(scenario_path, &error);
    expect(scenario.has_value(), "multiline render scenario should load");
    if (!scenario)
        return;

    expect_eq(scenario->name, std::string("multiline"), "scenario name should parse");
    expect_eq(scenario->width, 900, "scenario width should parse");
    expect_eq(scenario->height, 700, "scenario height should parse");
    expect_eq(static_cast<int>(scenario->commands.size()), 3, "multiline commands array should parse");
    expect_eq(scenario->commands[0], std::string("edit " + (scenario_dir.parent_path().parent_path() / "README.md").lexically_normal().generic_string()),
        "project root placeholder should expand inside multiline commands");
    expect_eq(scenario->commands[1], std::string("set nowrap"), "second command should parse");
    expect_eq(scenario->commands[2], std::string("lua vim.cmd([[normal! ggzt]])"), "third command should parse");

    std::error_code ec;
    std::filesystem::remove_all(scenario_dir, ec);
}
