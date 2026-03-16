#pragma once
// Convenience header: includes both nvim_rpc.h and nvim_ui.h.
// Prefer the narrower headers in new code:
//   <spectre/nvim_rpc.h>  — NvimProcess, MpackValue, RPC types, IRpcChannel, NvimRpc
//   <spectre/nvim_ui.h>   — ModeInfo, UiEventHandler, NvimInput
#include <spectre/nvim_rpc.h>
#include <spectre/nvim_ui.h>
