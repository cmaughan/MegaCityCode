#pragma once

#include <filesystem>
#include <optional>
#include <spectre/types.h>

namespace spectre
{

bool write_bmp_rgba(const std::filesystem::path& path, const CapturedFrame& frame);
std::optional<CapturedFrame> read_bmp_rgba(const std::filesystem::path& path);

} // namespace spectre
