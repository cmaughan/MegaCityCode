add_library(megacitycode-window
    src/sdl_window.cpp
)
target_include_directories(megacitycode-window PUBLIC include)
target_link_libraries(megacitycode-window PUBLIC megacitycode-types SDL3::SDL3)

if(APPLE)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
    target_link_libraries(megacitycode-window PRIVATE ${COREGRAPHICS_FRAMEWORK})
elseif(WIN32)
    target_link_libraries(megacitycode-window PRIVATE Shcore)
endif()
