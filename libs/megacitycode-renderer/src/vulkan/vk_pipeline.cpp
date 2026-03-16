#include "vk_pipeline.h"

#include <fstream>
#include <megacitycode/log.h>
#include <utility>
#include <vector>

namespace megacitycode
{

VkShaderModule VkPipelineManager::load_shader(VkDevice device, const std::string& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        MEGACITYCODE_LOG_ERROR(LogCategory::Renderer, "Failed to open shader: %s", path.c_str());
        return VK_NULL_HANDLE;
    }

    size_t size = static_cast<size_t>(file.tellg());
    std::vector<char> code(size);
    file.seekg(0);
    file.read(code.data(), static_cast<std::streamsize>(size));

    VkShaderModuleCreateInfo ci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    ci.codeSize = size;
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &ci, nullptr, &module) != VK_SUCCESS)
    {
        MEGACITYCODE_LOG_ERROR(LogCategory::Renderer, "Failed to create shader module: %s", path.c_str());
        return VK_NULL_HANDLE;
    }
    return module;
}

bool VkPipelineManager::initialize(VkDevice device, VkRenderPass render_pass, const std::string& shader_dir)
{
    VkShaderModule scene_vert = load_shader(device, shader_dir + "/plane.vert.spv");
    VkShaderModule scene_frag = load_shader(device, shader_dir + "/plane.frag.spv");
    if (!scene_vert || !scene_frag)
    {
        if (scene_vert)
            vkDestroyShaderModule(device, scene_vert, nullptr);
        if (scene_frag)
            vkDestroyShaderModule(device, scene_frag, nullptr);
        return false;
    }

    VkPipelineManager pending;
    auto cleanup = [&]() {
        pending.shutdown(device);
        vkDestroyShaderModule(device, scene_vert, nullptr);
        vkDestroyShaderModule(device, scene_frag, nullptr);
    };

    VkPushConstantRange push_range = {};
    push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push_range.offset = 0;
    push_range.size = sizeof(ScenePushConstants);

    VkPipelineLayoutCreateInfo layout_ci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    layout_ci.pushConstantRangeCount = 1;
    layout_ci.pPushConstantRanges = &push_range;
    if (vkCreatePipelineLayout(device, &layout_ci, nullptr, &pending.scene_layout_) != VK_SUCCESS)
    {
        cleanup();
        return false;
    }

    VkPipelineVertexInputStateCreateInfo vertex_input = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo input_assembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport_state = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend_attachment = {};
    blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo blend = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    blend.attachmentCount = 1;
    blend.pAttachments = &blend_attachment;

    VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = scene_vert;
    stages[0].pName = "main";
    stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = scene_frag;
    stages[1].pName = "main";

    VkGraphicsPipelineCreateInfo pi = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pi.stageCount = 2;
    pi.pStages = stages;
    pi.pVertexInputState = &vertex_input;
    pi.pInputAssemblyState = &input_assembly;
    pi.pViewportState = &viewport_state;
    pi.pRasterizationState = &raster;
    pi.pMultisampleState = &multisample;
    pi.pColorBlendState = &blend;
    pi.pDynamicState = &dynamic_state;
    pi.layout = pending.scene_layout_;
    pi.renderPass = render_pass;
    pi.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pi, nullptr, &pending.scene_pipeline_) != VK_SUCCESS)
    {
        cleanup();
        return false;
    }

    vkDestroyShaderModule(device, scene_vert, nullptr);
    vkDestroyShaderModule(device, scene_frag, nullptr);

    reset_objects(device);
    swap(pending);
    pending.shutdown(device);
    return true;
}

void VkPipelineManager::shutdown(VkDevice device)
{
    reset_objects(device);
}

void VkPipelineManager::swap(VkPipelineManager& other) noexcept
{
    using std::swap;

    swap(scene_pipeline_, other.scene_pipeline_);
    swap(scene_layout_, other.scene_layout_);
}

void VkPipelineManager::reset_objects(VkDevice device)
{
    if (scene_pipeline_)
        vkDestroyPipeline(device, scene_pipeline_, nullptr);
    if (scene_layout_)
        vkDestroyPipelineLayout(device, scene_layout_, nullptr);

    scene_pipeline_ = VK_NULL_HANDLE;
    scene_layout_ = VK_NULL_HANDLE;
}

} // namespace megacitycode
