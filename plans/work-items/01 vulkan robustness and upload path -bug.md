# 01 Vulkan Robustness And Upload Path -bug

## Problem

The reviews agree that Vulkan still has two classes of risk:
- weak or inconsistent `VkResult` checking
- upload/recreate paths that are more synchronous and duplicated than they should be

## Goal

Make Vulkan failures explicit and reduce avoidable GPU stalls or duplicated rebuild logic.

## Main Files

- `libs/spectre-renderer/src/vulkan/vk_renderer.cpp`
- `libs/spectre-renderer/src/vulkan/vk_context.cpp`
- `libs/spectre-renderer/src/vulkan/vk_pipeline.cpp`
- `libs/spectre-renderer/src/vulkan/vk_atlas.cpp`
- `libs/spectre-renderer/src/vulkan/vk_buffers.cpp`

## Implementation Plan

1. Audit and normalize all `VkResult` checks in resource creation and recreation code.
2. Extract the duplicated swapchain/pipeline rebuild sequence into one internal helper.
3. Rework atlas upload synchronization so the hot path does not rely on `vkQueueWaitIdle` for every glyph upload.
4. Add clearer error logging at the point of failure, not only after downstream damage.

## Suggested Split

- Agent 1: return-value audit and helper extraction
- Agent 2: atlas upload synchronization change
- Agent 3: resize/recreate regression tests

## Exit Criteria

- resource-creation failures are checked and surfaced
- rebuild logic exists in one place
- atlas upload no longer forces whole-queue idle on the normal path
