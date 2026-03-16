#include "vk_renderer.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <megacitycode/log.h>
#include <megacitycode/window.h>

namespace megacitycode
{

namespace
{

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Mat4
{
    std::array<float, 16> m = {};
};

Vec3 operator-(const Vec3& lhs, const Vec3& rhs)
{
    return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

float dot(const Vec3& lhs, const Vec3& rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 cross(const Vec3& lhs, const Vec3& rhs)
{
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

Vec3 normalize(const Vec3& value)
{
    const float length = std::sqrt(dot(value, value));
    if (length <= 0.0001f)
        return {};
    return { value.x / length, value.y / length, value.z / length };
}

Mat4 identity_matrix()
{
    Mat4 matrix;
    matrix.m[0] = 1.0f;
    matrix.m[5] = 1.0f;
    matrix.m[10] = 1.0f;
    matrix.m[15] = 1.0f;
    return matrix;
}

Mat4 multiply(const Mat4& lhs, const Mat4& rhs)
{
    Mat4 result;
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            float value = 0.0f;
            for (int k = 0; k < 4; ++k)
                value += lhs.m[k * 4 + row] * rhs.m[column * 4 + k];
            result.m[column * 4 + row] = value;
        }
    }
    return result;
}

Mat4 perspective(float vertical_fov_radians, float aspect, float z_near, float z_far)
{
    const float f = 1.0f / std::tan(vertical_fov_radians * 0.5f);
    Mat4 matrix = {};
    matrix.m[0] = f / aspect;
    matrix.m[5] = -f;
    matrix.m[10] = z_far / (z_near - z_far);
    matrix.m[11] = -1.0f;
    matrix.m[14] = (z_near * z_far) / (z_near - z_far);
    return matrix;
}

Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up)
{
    const Vec3 forward = normalize(target - eye);
    const Vec3 side = normalize(cross(forward, up));
    const Vec3 corrected_up = cross(side, forward);

    Mat4 matrix = identity_matrix();
    matrix.m[0] = side.x;
    matrix.m[1] = corrected_up.x;
    matrix.m[2] = -forward.x;
    matrix.m[4] = side.y;
    matrix.m[5] = corrected_up.y;
    matrix.m[6] = -forward.y;
    matrix.m[8] = side.z;
    matrix.m[9] = corrected_up.z;
    matrix.m[10] = -forward.z;
    matrix.m[12] = -dot(side, eye);
    matrix.m[13] = -dot(corrected_up, eye);
    matrix.m[14] = dot(forward, eye);
    return matrix;
}

ScenePushConstants build_scene_push_constants(const VkExtent2D& extent)
{
    const float aspect = extent.height == 0 ? 1.0f : static_cast<float>(extent.width) / static_cast<float>(extent.height);
    const Mat4 proj = perspective(55.0f * 3.1415926535f / 180.0f, aspect, 0.1f, 100.0f);
    const Mat4 view = look_at({ 0.0f, 4.5f, 4.5f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
    const Mat4 view_proj = multiply(proj, view);

    ScenePushConstants push = {};
    std::memcpy(push.view_proj, view_proj.m.data(), sizeof(push.view_proj));
    push.material_color[0] = 0.83f;
    push.material_color[1] = 0.76f;
    push.material_color[2] = 0.63f;
    push.material_color[3] = 1.0f;
    return push;
}

void transition_image_layout(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout,
    VkAccessFlags src_access, VkAccessFlags dst_access, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage)
{
    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

} // namespace

bool VkRenderer::ensure_capture_buffer(size_t required_size)
{
    if (required_size <= capture_buffer_size_ && capture_buffer_ != VK_NULL_HANDLE)
        return true;

    destroy_capture_buffer();

    VkBufferCreateInfo buffer_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer_info.size = required_size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocation_info = {};
    if (vmaCreateBuffer(ctx_.allocator(), &buffer_info, &alloc_info, &capture_buffer_, &capture_allocation_, &allocation_info) != VK_SUCCESS)
    {
        MEGACITYCODE_LOG_ERROR(LogCategory::Renderer, "Failed to create Vulkan readback buffer");
        return false;
    }

    capture_mapped_ = allocation_info.pMappedData;
    capture_buffer_size_ = required_size;
    return true;
}

void VkRenderer::destroy_capture_buffer()
{
    if (capture_buffer_ == VK_NULL_HANDLE)
        return;

    vmaDestroyBuffer(ctx_.allocator(), capture_buffer_, capture_allocation_);
    capture_buffer_ = VK_NULL_HANDLE;
    capture_allocation_ = VK_NULL_HANDLE;
    capture_mapped_ = nullptr;
    capture_buffer_size_ = 0;
}

void VkRenderer::finish_capture_readback()
{
    if (!capture_requested_ || capture_buffer_ == VK_NULL_HANDLE || capture_mapped_ == nullptr)
        return;

    vmaInvalidateAllocation(ctx_.allocator(), capture_allocation_, 0, VK_WHOLE_SIZE);

    const uint32_t width = ctx_.swapchain().extent.width;
    const uint32_t height = ctx_.swapchain().extent.height;
    CapturedFrame frame;
    frame.width = static_cast<int>(width);
    frame.height = static_cast<int>(height);
    frame.rgba.resize(static_cast<size_t>(width) * height * 4);

    const auto* src = static_cast<const uint8_t*>(capture_mapped_);
    for (size_t i = 0; i < frame.rgba.size(); i += 4)
    {
        frame.rgba[i + 0] = src[i + 2];
        frame.rgba[i + 1] = src[i + 1];
        frame.rgba[i + 2] = src[i + 0];
        frame.rgba[i + 3] = src[i + 3];
    }

    captured_frame_ = std::move(frame);
    capture_requested_ = false;
}

bool VkRenderer::initialize(IWindow& window)
{
    if (!ctx_.initialize(static_cast<SDL_Window*>(window.native_handle())))
        return false;

    auto [w, h] = window.size_pixels();
    pixel_w_ = w;
    pixel_h_ = h;

    if (!pipeline_.initialize(ctx_.device(), ctx_.render_pass(), "shaders"))
        return false;
    if (!create_command_buffers())
        return false;
    if (!create_sync_objects())
        return false;
    return true;
}

void VkRenderer::shutdown()
{
    if (ctx_.device())
        vkDeviceWaitIdle(ctx_.device());

    for (auto& sem : image_available_sem_)
        vkDestroySemaphore(ctx_.device(), sem, nullptr);
    for (auto& sem : render_finished_sem_)
        vkDestroySemaphore(ctx_.device(), sem, nullptr);
    for (auto& fence : in_flight_fences_)
        vkDestroyFence(ctx_.device(), fence, nullptr);
    if (cmd_pool_)
        vkDestroyCommandPool(ctx_.device(), cmd_pool_, nullptr);
    destroy_capture_buffer();

    pipeline_.shutdown(ctx_.device());
    ctx_.shutdown();
}

bool VkRenderer::create_sync_objects()
{
    image_available_sem_.resize(MAX_FRAMES_IN_FLIGHT);
    render_finished_sem_.resize(MAX_FRAMES_IN_FLIGHT);
    in_flight_fences_.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo sem_ci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fence_ci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        if (vkCreateSemaphore(ctx_.device(), &sem_ci, nullptr, &image_available_sem_[i]) != VK_SUCCESS)
            return false;
        if (vkCreateSemaphore(ctx_.device(), &sem_ci, nullptr, &render_finished_sem_[i]) != VK_SUCCESS)
            return false;
        if (vkCreateFence(ctx_.device(), &fence_ci, nullptr, &in_flight_fences_[i]) != VK_SUCCESS)
            return false;
    }
    return true;
}

bool VkRenderer::create_command_buffers()
{
    VkCommandPoolCreateInfo pool_ci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    pool_ci.queueFamilyIndex = ctx_.graphics_queue_family();
    pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(ctx_.device(), &pool_ci, nullptr, &cmd_pool_) != VK_SUCCESS)
        return false;

    cmd_buffers_.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    alloc_info.commandPool = cmd_pool_;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    return vkAllocateCommandBuffers(ctx_.device(), &alloc_info, cmd_buffers_.data()) == VK_SUCCESS;
}

bool VkRenderer::recreate_frame_resources()
{
    PendingSwapchainResources pending_swapchain;
    if (!ctx_.build_swapchain_resources(pixel_w_, pixel_h_, pending_swapchain))
        return false;

    VkPipelineManager pending_pipeline;
    if (!pending_pipeline.initialize(ctx_.device(), pending_swapchain.render_pass, "shaders"))
    {
        ctx_.destroy_pending_swapchain_resources(pending_swapchain);
        return false;
    }

    ctx_.commit_swapchain_resources(std::move(pending_swapchain));
    pipeline_.swap(pending_pipeline);
    pending_pipeline.shutdown(ctx_.device());
    return true;
}

void VkRenderer::set_grid_size(int cols, int rows)
{
    (void)cols;
    (void)rows;
}

void VkRenderer::update_cells(std::span<const CellUpdate> updates)
{
    (void)updates;
}

void VkRenderer::set_overlay_cells(std::span<const CellUpdate> updates)
{
    (void)updates;
}

void VkRenderer::set_atlas_texture(const uint8_t* data, int w, int h)
{
    (void)data;
    (void)w;
    (void)h;
}

void VkRenderer::update_atlas_region(int x, int y, int w, int h, const uint8_t* data)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)data;
}

void VkRenderer::set_cursor(int col, int row, const CursorStyle& style)
{
    (void)col;
    (void)row;
    (void)style;
}

void VkRenderer::resize(int pixel_w, int pixel_h)
{
    pixel_w_ = pixel_w;
    pixel_h_ = pixel_h;
    framebuffer_resized_ = true;
}

std::pair<int, int> VkRenderer::cell_size_pixels() const
{
    return { cell_w_, cell_h_ };
}

void VkRenderer::set_cell_size(int w, int h)
{
    cell_w_ = w;
    cell_h_ = h;
}

void VkRenderer::set_ascender(int a)
{
    ascender_ = a;
}

void VkRenderer::request_frame_capture()
{
    capture_requested_ = true;
}

std::optional<CapturedFrame> VkRenderer::take_captured_frame()
{
    auto frame = std::move(captured_frame_);
    captured_frame_.reset();
    return frame;
}

bool VkRenderer::begin_frame()
{
    vkWaitForFences(ctx_.device(), 1, &in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(
        ctx_.device(), ctx_.swapchain().swapchain, UINT64_MAX,
        image_available_sem_[current_frame_], VK_NULL_HANDLE, &current_image_);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        MEGACITYCODE_LOG_WARN(LogCategory::Renderer, "Vulkan acquire returned VK_ERROR_OUT_OF_DATE_KHR");
        if (!recreate_frame_resources())
            MEGACITYCODE_LOG_ERROR(LogCategory::Renderer, "Failed to rebuild Vulkan frame resources after acquire");
        return false;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        MEGACITYCODE_LOG_ERROR(LogCategory::Renderer, "vkAcquireNextImageKHR failed: %d", static_cast<int>(result));
        return false;
    }

    vkResetFences(ctx_.device(), 1, &in_flight_fences_[current_frame_]);
    vkResetCommandBuffer(cmd_buffers_[current_frame_], 0);
    return true;
}

void VkRenderer::record_command_buffer(VkCommandBuffer cmd, uint32_t image_index)
{
    VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cmd, &begin_info);

    VkClearValue clear_value = {};
    clear_value.color = { { 0.58f, 0.68f, 0.80f, 1.0f } };

    VkRenderPassBeginInfo rp_begin = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    rp_begin.renderPass = ctx_.render_pass();
    rp_begin.framebuffer = ctx_.swapchain().framebuffers[image_index];
    rp_begin.renderArea.extent = ctx_.swapchain().extent;
    rp_begin.clearValueCount = 1;
    rp_begin.pClearValues = &clear_value;

    vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {};
    viewport.width = static_cast<float>(ctx_.swapchain().extent.width);
    viewport.height = static_cast<float>(ctx_.swapchain().extent.height);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.extent = ctx_.swapchain().extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    const ScenePushConstants push_constants = build_scene_push_constants(ctx_.swapchain().extent);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.scene_pipeline());
    vkCmdPushConstants(cmd, pipeline_.scene_layout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(push_constants), &push_constants);
    vkCmdDraw(cmd, 6, 1, 0, 0);

    vkCmdEndRenderPass(cmd);

    if (capture_requested_)
    {
        transition_image_layout(cmd, ctx_.swapchain().images[image_index],
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = { ctx_.swapchain().extent.width, ctx_.swapchain().extent.height, 1 };
        vkCmdCopyImageToBuffer(cmd, ctx_.swapchain().images[image_index], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            capture_buffer_, 1, &region);

        transition_image_layout(cmd, ctx_.swapchain().images[image_index],
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_ACCESS_TRANSFER_READ_BIT, 0,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }
    else
    {
        transition_image_layout(cmd, ctx_.swapchain().images[image_index],
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }

    vkEndCommandBuffer(cmd);
}

void VkRenderer::end_frame()
{
    if (capture_requested_ && !ensure_capture_buffer(static_cast<size_t>(ctx_.swapchain().extent.width) * ctx_.swapchain().extent.height * 4))
        capture_requested_ = false;

    record_command_buffer(cmd_buffers_[current_frame_], current_image_);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &image_available_sem_[current_frame_];
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd_buffers_[current_frame_];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &render_finished_sem_[current_frame_];

    if (vkQueueSubmit(ctx_.graphics_queue(), 1, &submit, in_flight_fences_[current_frame_]) != VK_SUCCESS)
    {
        MEGACITYCODE_LOG_ERROR(LogCategory::Renderer, "Failed to submit Vulkan frame");
        return;
    }

    VkPresentInfoKHR present = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &render_finished_sem_[current_frame_];
    present.swapchainCount = 1;
    present.pSwapchains = &ctx_.swapchain().swapchain;
    present.pImageIndices = &current_image_;

    VkResult result = vkQueuePresentKHR(ctx_.graphics_queue(), &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized_)
    {
        framebuffer_resized_ = false;
        if (!recreate_frame_resources())
            MEGACITYCODE_LOG_ERROR(LogCategory::Renderer, "Failed to rebuild Vulkan frame resources after present");
    }

    if (capture_requested_)
    {
        vkWaitForFences(ctx_.device(), 1, &in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);
        finish_capture_readback();
    }

    current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

} // namespace megacitycode
