// Platform-specific renderer factory owned by spectre-renderer so app code only
// depends on the public renderer API.

#include <spectre/renderer.h>

#ifdef __APPLE__
#include "metal/metal_renderer.h"
#else
#include "vulkan/vk_renderer.h"
#endif

namespace spectre
{

std::unique_ptr<IRenderer> create_renderer()
{
#ifdef __APPLE__
    return std::make_unique<MetalRenderer>();
#else
    return std::make_unique<VkRenderer>();
#endif
}

} // namespace spectre
