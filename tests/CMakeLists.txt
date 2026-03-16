add_executable(megacitycode-tests
    ../app/app_config.cpp
    ../app/render_test.cpp
    test_main.cpp
    app_config_tests.cpp
    log_tests.cpp
    highlight_tests.cpp
    font_tests.cpp
    grid_tests.cpp
    renderer_state_tests.cpp
    vk_resource_helpers_tests.cpp
    render_test_parser_tests.cpp
)

target_include_directories(megacitycode-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${PROJECT_SOURCE_DIR}/app
    ${PROJECT_SOURCE_DIR}/libs/megacitycode-font/src
    ${PROJECT_SOURCE_DIR}/libs/megacitycode-renderer/src
)
target_link_libraries(megacitycode-tests PRIVATE megacitycode-font megacitycode-grid megacitycode-renderer)

add_test(NAME megacitycode-tests COMMAND $<TARGET_FILE:megacitycode-tests>)
