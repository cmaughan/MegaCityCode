add_library(megacitycode-font
    src/font_manager.cpp
    src/glyph_cache.cpp
    src/text_shaper.cpp
    src/text_service.cpp
)
target_include_directories(megacitycode-font PUBLIC include)
target_link_libraries(megacitycode-font PUBLIC megacitycode-types freetype harfbuzz)
