#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <spectre/nvim_rpc.h>

namespace spectre
{

bool encode_mpack_value(const MpackValue& value, std::vector<char>& out);
bool decode_mpack_value(std::span<const uint8_t> bytes, MpackValue& value, size_t* consumed = nullptr);

bool encode_rpc_request(
    uint32_t msgid, const std::string& method, const std::vector<MpackValue>& params, std::vector<char>& out);
bool encode_rpc_notification(
    const std::string& method, const std::vector<MpackValue>& params, std::vector<char>& out);

} // namespace spectre
