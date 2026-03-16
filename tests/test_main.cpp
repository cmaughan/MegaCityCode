#include <exception>
#include <iostream>

void run_app_config_tests();
void run_font_tests();
void run_log_tests();
void run_highlight_tests();
void run_grid_tests();
void run_renderer_state_tests();
void run_vk_resource_helpers_tests();
void run_render_test_parser_tests();

int main()
{
    try
    {
        run_app_config_tests();
        run_log_tests();
        run_highlight_tests();
        run_font_tests();
        run_grid_tests();
        run_renderer_state_tests();
        run_vk_resource_helpers_tests();
        run_render_test_parser_tests();
        std::cout << "megacitycode-tests: ok\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "megacitycode-tests: " << ex.what() << '\n';
        return 1;
    }
}
