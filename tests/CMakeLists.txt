add_executable(spectre-tests
    test_main.cpp
    font_tests.cpp
    grid_tests.cpp
    ui_events_tests.cpp
    input_tests.cpp
    unicode_tests.cpp
    renderer_state_tests.cpp
    rpc_codec_tests.cpp
)

target_include_directories(spectre-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${PROJECT_SOURCE_DIR}/libs/spectre-renderer/src
    ${PROJECT_SOURCE_DIR}/libs/spectre-nvim/src
)
target_link_libraries(spectre-tests PRIVATE spectre-font spectre-grid spectre-nvim spectre-renderer)

add_test(NAME spectre-tests COMMAND $<TARGET_FILE:spectre-tests>)
