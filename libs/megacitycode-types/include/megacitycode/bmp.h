#pragma once

#include <filesystem>
#include <megacitycode/types.h>
#include <optional>

namespace megacitycode
{

bool write_bmp_rgba(const std::filesystem::path& path, const CapturedFrame& frame);
std::optional<CapturedFrame> read_bmp_rgba(const std::filesystem::path& path);

} // namespace megacitycode
