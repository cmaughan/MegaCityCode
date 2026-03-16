#pragma once
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace spectre
{

class VkContext;

class VkAtlas
{
public:
    static constexpr int ATLAS_SIZE = 2048;

    bool initialize(VkContext& ctx);
    void shutdown(VkContext& ctx);

    void upload(VkContext& ctx, const uint8_t* data, int w, int h);
    void upload_region(VkContext& ctx, int x, int y, int w, int h, const uint8_t* data);

    VkImageView image_view() const
    {
        return image_view_;
    }
    VkSampler sampler() const
    {
        return sampler_;
    }

private:
    bool upload_internal(VkContext& ctx, int x, int y, int w, int h, const uint8_t* data);
    VkCommandBuffer begin_single_command(VkContext& ctx);
    bool end_single_command(VkContext& ctx, VkCommandBuffer cmd);

    VkImage image_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;
    VkImageView image_view_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;
    VkCommandPool cmd_pool_ = VK_NULL_HANDLE;
    VkBuffer staging_buffer_ = VK_NULL_HANDLE;
    VmaAllocation staging_alloc_ = VK_NULL_HANDLE;
    void* staging_mapped_ = nullptr;
    VkImageLayout current_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
};

} // namespace spectre
