add_executable(spectre-rpc-fake
    rpc_fake_server.cpp
)
target_include_directories(spectre-rpc-fake PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/spectre-nvim/src
)
target_link_libraries(spectre-rpc-fake PRIVATE spectre-nvim)

add_executable(spectre-tests
    ../app/app_config.cpp
    ../app/cursor_blinker.cpp
    ../app/render_test.cpp
    test_main.cpp
    app_config_tests.cpp
    cursor_blinker_tests.cpp
    log_tests.cpp
    highlight_tests.cpp
    font_tests.cpp
    grid_tests.cpp
    ui_events_tests.cpp
    input_tests.cpp
    unicode_tests.cpp
    renderer_state_tests.cpp
    vk_resource_helpers_tests.cpp
    rpc_codec_tests.cpp
    rpc_integration_tests.cpp
    render_test_parser_tests.cpp
)

target_include_directories(spectre-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${PROJECT_SOURCE_DIR}/app
    ${PROJECT_SOURCE_DIR}/libs/spectre-font/src
    ${PROJECT_SOURCE_DIR}/libs/spectre-renderer/src
    ${PROJECT_SOURCE_DIR}/libs/spectre-nvim/src
)
target_link_libraries(spectre-tests PRIVATE spectre-font spectre-grid spectre-nvim spectre-renderer)
target_compile_definitions(spectre-tests PRIVATE
    SPECTRE_RPC_FAKE_PATH="$<TARGET_FILE:spectre-rpc-fake>"
)

add_test(NAME spectre-tests COMMAND $<TARGET_FILE:spectre-tests>)
