#pragma once

#include <string>
#include <vulkan/vulkan.h>

namespace megacitycode
{

struct ScenePushConstants
{
    float view_proj[16] = {};
    float material_color[4] = {};
};

class VkPipelineManager
{
public:
    bool initialize(VkDevice device, VkRenderPass render_pass, const std::string& shader_dir);
    void shutdown(VkDevice device);
    void swap(VkPipelineManager& other) noexcept;

    VkPipeline scene_pipeline() const
    {
        return scene_pipeline_;
    }

    VkPipelineLayout scene_layout() const
    {
        return scene_layout_;
    }

private:
    static VkShaderModule load_shader(VkDevice device, const std::string& path);
    void reset_objects(VkDevice device);

    VkPipeline scene_pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout scene_layout_ = VK_NULL_HANDLE;
};

} // namespace megacitycode
