#pragma once

#include <memory>
#include <spectre/font_metrics.h>
#include <spectre/types.h>
#include <string>
#include <vector>

namespace spectre
{

struct AtlasDirtyRect
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct TextServiceConfig
{
    std::string font_path;
    std::vector<std::string> fallback_paths;
};

class TextService
{
public:
    static constexpr int DEFAULT_POINT_SIZE = 11;
    static constexpr int MIN_POINT_SIZE = 6;
    static constexpr int MAX_POINT_SIZE = 36;

    TextService();
    ~TextService();
    TextService(const TextService&) = delete;
    TextService& operator=(const TextService&) = delete;
    TextService(TextService&& other) noexcept;
    TextService& operator=(TextService&& other) noexcept;

    bool initialize(int point_size, float display_ppi);
    bool initialize(const TextServiceConfig& config, int point_size, float display_ppi);
    void shutdown();

    bool set_point_size(int point_size);
    int point_size() const;
    const FontMetrics& metrics() const;
    const std::string& primary_font_path() const;

    AtlasRegion resolve_cluster(const std::string& text);

    bool atlas_dirty() const;
    void clear_atlas_dirty();
    const uint8_t* atlas_data() const;
    int atlas_width() const;
    int atlas_height() const;
    AtlasDirtyRect atlas_dirty_rect() const;
    bool consume_atlas_reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace spectre
