// Platform-specific renderer factory owned by megacitycode-renderer so app code only
// depends on the public renderer API.

#include <megacitycode/renderer.h>

#ifdef __APPLE__
#include "metal/metal_renderer.h"
#else
#include "vulkan/vk_renderer.h"
#endif

namespace megacitycode
{

std::unique_ptr<IRenderer> create_renderer()
{
#ifdef __APPLE__
    return std::make_unique<MetalRenderer>();
#else
    return std::make_unique<VkRenderer>();
#endif
}

} // namespace megacitycode
