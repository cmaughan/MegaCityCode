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

# Collect source files (exclude platform-specific renderer dirs, then add back the right one)
file(GLOB_RECURSE SPECTRE_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.h
)

if(APPLE)
    # Remove Vulkan sources
    file(GLOB_RECURSE VK_SOURCES CONFIGURE_DEPENDS src/renderer/vulkan/*.cpp src/renderer/vulkan/*.h)
    list(REMOVE_ITEM SPECTRE_SOURCES ${VK_SOURCES})
    # Add Metal sources (.mm)
    file(GLOB_RECURSE METAL_SOURCES CONFIGURE_DEPENDS src/renderer/metal/*.mm src/renderer/metal/*.h)
    list(APPEND SPECTRE_SOURCES ${METAL_SOURCES})
else()
    # Remove Metal sources (if any .mm files exist)
    file(GLOB_RECURSE METAL_SOURCES CONFIGURE_DEPENDS src/renderer/metal/*.mm src/renderer/metal/*.h)
    if(METAL_SOURCES)
        list(REMOVE_ITEM SPECTRE_SOURCES ${METAL_SOURCES})
    endif()
endif()

add_executable(spectre ${SPECTRE_SOURCES})
target_include_directories(spectre PRIVATE src)

# Common libraries
target_link_libraries(spectre PRIVATE
    SDL3::SDL3
    freetype
    harfbuzz
    mpack_lib
)

if(APPLE)
    # Metal frameworks
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    target_link_libraries(spectre PRIVATE
        ${METAL_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
    )

    # Shader compilation dependency
    add_dependencies(spectre compile_metal_shaders)

    # Copy metallib to build dir next to executable
    add_custom_command(TARGET spectre POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:spectre>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/shaders/grid.metallib
            $<TARGET_FILE_DIR:spectre>/shaders/
    )
else()
    target_link_libraries(spectre PRIVATE
        Vulkan::Vulkan
        vk-bootstrap::vk-bootstrap
        GPUOpen::VulkanMemoryAllocator
    )

    # Shader compilation dependency
    add_dependencies(spectre compile_shaders)

    # Copy shaders to build dir next to executable
    add_custom_command(TARGET spectre POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_BINARY_DIR}/shaders
            $<TARGET_FILE_DIR:spectre>/shaders
    )
endif()

# Copy font to build directory
if(EXISTS ${CMAKE_SOURCE_DIR}/fonts/JetBrainsMonoNerdFontMono-Regular.ttf)
    add_custom_command(TARGET spectre POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:spectre>/fonts
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/fonts/JetBrainsMonoNerdFontMono-Regular.ttf
            $<TARGET_FILE_DIR:spectre>/fonts/
    )
endif()

if(WIN32)
    target_link_libraries(spectre PRIVATE ws2_32)
    set_target_properties(spectre PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
