add_library(spectre-nvim
    src/nvim_process.cpp
    src/mpack_codec.cpp
    src/rpc.cpp
    src/ui_events.cpp
    src/input.cpp
)
target_include_directories(spectre-nvim PUBLIC include)
target_link_libraries(spectre-nvim PUBLIC spectre-types mpack_lib SDL3::SDL3)
