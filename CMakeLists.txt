cmake_minimum_required(VERSION 3.25)
project(megacitycode VERSION 0.1.0 LANGUAGES CXX C)

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
add_subdirectory(libs/megacitycode-types)
add_subdirectory(libs/megacitycode-window)
add_subdirectory(libs/megacitycode-renderer)
add_subdirectory(libs/megacitycode-font)
add_subdirectory(libs/megacitycode-grid)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# --- App executable ---
if(APPLE)
    add_executable(megacitycode
        app/main.cpp
        app/app.cpp
        app/app_config.cpp
        app/render_test.cpp
    )
else()
    add_executable(megacitycode
        app/main.cpp
        app/app.cpp
        app/app_config.cpp
        app/render_test.cpp
    )
endif()

target_include_directories(megacitycode PRIVATE app)

target_link_libraries(megacitycode PRIVATE
    megacitycode-renderer
    megacitycode-font
    megacitycode-grid
)

if(APPLE)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
    target_link_libraries(megacitycode PRIVATE ${COREGRAPHICS_FRAMEWORK})
    add_dependencies(megacitycode compile_metal_shaders)

    # Copy metallib to build dir next to executable
    add_custom_command(TARGET megacitycode POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:megacitycode>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/shaders/grid.metallib
            $<TARGET_FILE_DIR:megacitycode>/shaders/
    )
else()
    add_dependencies(megacitycode compile_shaders)

    # Copy shaders to build dir next to executable
    add_custom_command(TARGET megacitycode POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_BINARY_DIR}/shaders
            $<TARGET_FILE_DIR:megacitycode>/shaders
    )
endif()

# Copy font to build directory
if(EXISTS ${CMAKE_SOURCE_DIR}/fonts/JetBrainsMonoNerdFont-Regular.ttf)
    add_custom_command(TARGET megacitycode POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:megacitycode>/fonts
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/fonts/JetBrainsMonoNerdFont-Regular.ttf
            $<TARGET_FILE_DIR:megacitycode>/fonts/
    )
endif()

if(WIN32)
    target_link_libraries(megacitycode PRIVATE ws2_32 Vulkan::Headers GPUOpen::VulkanMemoryAllocator)
    set_target_properties(megacitycode PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(BUILD_TESTING)
    if(WIN32)
        add_test(NAME megacitycode-app-smoke COMMAND $<TARGET_FILE:megacitycode> --console --smoke-test)
    else()
        add_test(NAME megacitycode-app-smoke COMMAND $<TARGET_FILE:megacitycode> --smoke-test)
    endif()
    set_tests_properties(megacitycode-app-smoke PROPERTIES TIMEOUT 20)

    if(WIN32)
        set(MEGACITYCODE_RENDER_PLATFORM "windows")
    elseif(APPLE)
        set(MEGACITYCODE_RENDER_PLATFORM "macos")
    else()
        set(MEGACITYCODE_RENDER_PLATFORM "linux")
    endif()

    set(MEGACITYCODE_RENDER_SCENARIOS
        plane-view
    )
    foreach(MEGACITYCODE_RENDER_NAME IN LISTS MEGACITYCODE_RENDER_SCENARIOS)
        set(MEGACITYCODE_RENDER_SCENARIO ${CMAKE_SOURCE_DIR}/tests/render/${MEGACITYCODE_RENDER_NAME}.toml)
        set(MEGACITYCODE_RENDER_REFERENCE ${CMAKE_SOURCE_DIR}/tests/render/reference/${MEGACITYCODE_RENDER_NAME}.${MEGACITYCODE_RENDER_PLATFORM}.bmp)
        if(EXISTS ${MEGACITYCODE_RENDER_SCENARIO} AND EXISTS ${MEGACITYCODE_RENDER_REFERENCE})
            if(WIN32)
                add_test(NAME megacitycode-render-${MEGACITYCODE_RENDER_NAME} COMMAND $<TARGET_FILE:megacitycode> --console --render-test ${MEGACITYCODE_RENDER_SCENARIO})
            else()
                add_test(NAME megacitycode-render-${MEGACITYCODE_RENDER_NAME} COMMAND $<TARGET_FILE:megacitycode> --render-test ${MEGACITYCODE_RENDER_SCENARIO})
            endif()
            set_tests_properties(megacitycode-render-${MEGACITYCODE_RENDER_NAME} PROPERTIES TIMEOUT 30)
        else()
            message(STATUS "Skipping megacitycode-render-${MEGACITYCODE_RENDER_NAME} because ${MEGACITYCODE_RENDER_REFERENCE} is not available")
        endif()
    endforeach()
endif()
