add_library(megacitycode-grid
    src/grid.cpp
)
target_include_directories(megacitycode-grid PUBLIC include)
target_link_libraries(megacitycode-grid PUBLIC megacitycode-types)
