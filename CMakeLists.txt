cmake_minimum_required(VERSION 3.25)
project(spectre VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} -fobjc-arc")
endif()

# Platform-specific GPU API
if(NOT APPLE)
    find_package(Vulkan REQUIRED COMPONENTS glslc)
endif()

# Fetch all dependencies
include(cmake/FetchDependencies.cmake)

# Compile shaders
if(APPLE)
    include(cmake/CompileShaders_Metal.cmake)
else()
    include(cmake/CompileShaders.cmake)
endif()

# --- Libraries ---
add_subdirectory(libs/spectre-types)
add_subdirectory(libs/spectre-window)
add_subdirectory(libs/spectre-renderer)
add_subdirectory(libs/spectre-font)
add_subdirectory(libs/spectre-grid)
add_subdirectory(libs/spectre-nvim)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# --- App executable ---
if(APPLE)
    add_executable(spectre
        app/main.cpp
        app/app.cpp
        app/app_config.cpp
        app/grid_rendering_pipeline.cpp
        app/render_test.cpp
        app/ui_request_worker.cpp
    )
else()
    add_executable(spectre
        app/main.cpp
        app/app.cpp
        app/app_config.cpp
        app/grid_rendering_pipeline.cpp
        app/render_test.cpp
        app/ui_request_worker.cpp
    )
endif()

target_include_directories(spectre PRIVATE app)

target_link_libraries(spectre PRIVATE
    spectre-types
    spectre-renderer
    spectre-font
    spectre-grid
    spectre-nvim
)

if(APPLE)
    add_dependencies(spectre compile_metal_shaders)

    # Copy metallib to build dir next to executable
    add_custom_command(TARGET spectre POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:spectre>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/shaders/grid.metallib
            $<TARGET_FILE_DIR:spectre>/shaders/
    )
else()
    add_dependencies(spectre compile_shaders)

    # Copy shaders to build dir next to executable
    add_custom_command(TARGET spectre POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_BINARY_DIR}/shaders
            $<TARGET_FILE_DIR:spectre>/shaders
    )
endif()

# Copy font to build directory
if(EXISTS ${CMAKE_SOURCE_DIR}/fonts/JetBrainsMonoNerdFont-Regular.ttf)
    add_custom_command(TARGET spectre POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:spectre>/fonts
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/fonts/JetBrainsMonoNerdFont-Regular.ttf
            $<TARGET_FILE_DIR:spectre>/fonts/
    )
endif()

if(WIN32)
    target_link_libraries(spectre PRIVATE ws2_32 Vulkan::Headers GPUOpen::VulkanMemoryAllocator)
    set_target_properties(spectre PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(BUILD_TESTING)
    find_program(NVIM_EXECUTABLE nvim)
    if(NVIM_EXECUTABLE)
        if(WIN32)
            add_test(NAME spectre-app-smoke COMMAND $<TARGET_FILE:spectre> --console --smoke-test)
        else()
            add_test(NAME spectre-app-smoke COMMAND $<TARGET_FILE:spectre> --smoke-test)
        endif()
        set_tests_properties(spectre-app-smoke PROPERTIES TIMEOUT 20)

        if(WIN32)
            set(SPECTRE_RENDER_PLATFORM "windows")
        elseif(APPLE)
            set(SPECTRE_RENDER_PLATFORM "macos")
        else()
            set(SPECTRE_RENDER_PLATFORM "linux")
        endif()

        set(SPECTRE_RENDER_SCENARIOS
            basic-view
            cmdline-view
            unicode-view
        )
        foreach(SPECTRE_RENDER_NAME IN LISTS SPECTRE_RENDER_SCENARIOS)
            set(SPECTRE_RENDER_SCENARIO ${CMAKE_SOURCE_DIR}/tests/render/${SPECTRE_RENDER_NAME}.toml)
            set(SPECTRE_RENDER_REFERENCE ${CMAKE_SOURCE_DIR}/tests/render/reference/${SPECTRE_RENDER_NAME}.${SPECTRE_RENDER_PLATFORM}.bmp)
            if(EXISTS ${SPECTRE_RENDER_SCENARIO} AND EXISTS ${SPECTRE_RENDER_REFERENCE})
                if(WIN32)
                    add_test(NAME spectre-render-${SPECTRE_RENDER_NAME} COMMAND $<TARGET_FILE:spectre> --console --render-test ${SPECTRE_RENDER_SCENARIO})
                else()
                    add_test(NAME spectre-render-${SPECTRE_RENDER_NAME} COMMAND $<TARGET_FILE:spectre> --render-test ${SPECTRE_RENDER_SCENARIO})
                endif()
                set_tests_properties(spectre-render-${SPECTRE_RENDER_NAME} PROPERTIES TIMEOUT 30)
            else()
                message(STATUS "Skipping spectre-render-${SPECTRE_RENDER_NAME} because ${SPECTRE_RENDER_REFERENCE} is not available")
            endif()
        endforeach()
    else()
        message(WARNING "Skipping spectre-app-smoke test because nvim was not found on PATH")
    endif()
endif()
