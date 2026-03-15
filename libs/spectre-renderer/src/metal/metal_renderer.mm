#include "metal_renderer.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>
#include <algorithm>
#include <cstring>
#include <spectre/log.h>
#include <spectre/window.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

namespace spectre
{

namespace
{

NSUInteger align_capture_row_bytes(NSUInteger width)
{
    const NSUInteger raw = width * 4;
    const NSUInteger alignment = 256;
    return ((raw + alignment - 1) / alignment) * alignment;
}

} // namespace

void MetalRenderer::upload_dirty_state()
{
    id<MTLBuffer> buf = (__bridge id<MTLBuffer>)grid_buffer_;
    if (!buf)
        return;

    auto* bytes = static_cast<std::byte*>([buf contents]);
    if (state_.has_dirty_cells())
    {
        state_.copy_dirty_cells_to(bytes + state_.dirty_cell_offset_bytes());
    }

    if (state_.overlay_region_dirty())
    {
        state_.copy_overlay_region_to(bytes + state_.overlay_offset_bytes());
    }

    state_.clear_dirty();
}

bool MetalRenderer::ensure_capture_buffer(size_t width, size_t height)
{
    const size_t bytes_per_row = align_capture_row_bytes(static_cast<NSUInteger>(width));
    const size_t required_size = static_cast<size_t>(bytes_per_row * height);
    if (capture_buffer_ && required_size <= capture_buffer_size_ && bytes_per_row == capture_bytes_per_row_)
        return true;

    if (capture_buffer_)
    {
        (void)(__bridge_transfer id)capture_buffer_;
        capture_buffer_ = nullptr;
    }

    id<MTLDevice> device = (__bridge id<MTLDevice>)device_;
    id<MTLBuffer> buffer = [device newBufferWithLength:required_size options:MTLResourceStorageModeShared];
    if (!buffer)
    {
        SPECTRE_LOG_ERROR(LogCategory::Renderer, "Failed to create Metal capture buffer");
        return false;
    }

    capture_buffer_ = (__bridge_retained void*)buffer;
    capture_buffer_size_ = required_size;
    capture_bytes_per_row_ = bytes_per_row;
    return true;
}

bool MetalRenderer::initialize(IWindow& window)
{
    // Get Metal device
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device)
    {
        SPECTRE_LOG_ERROR(LogCategory::Renderer, "Failed to create Metal device");
        return false;
    }
    device_ = (__bridge_retained void*)device;

    // Get CAMetalLayer from SDL window
    SDL_MetalView metalView = SDL_Metal_CreateView(window.native_handle());
    if (!metalView)
    {
        SPECTRE_LOG_ERROR(LogCategory::Renderer, "Failed to create Metal view: %s", SDL_GetError());
        return false;
    }
    CAMetalLayer* layer = (__bridge CAMetalLayer*)SDL_Metal_GetLayer(metalView);
    layer.device = device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = NO;
    layer_ = (__bridge_retained void*)layer;

    auto [w, h] = window.size_pixels();
    pixel_w_ = w;
    pixel_h_ = h;
    layer.drawableSize = CGSizeMake(w, h);

    // Create command queue
    id<MTLCommandQueue> queue = [device newCommandQueue];
    command_queue_ = (__bridge_retained void*)queue;

    // Load shader library from metallib
    NSError* error = nil;
    NSURL* libURL = [[NSBundle mainBundle] URLForResource:@"grid" withExtension:@"metallib"];
    id<MTLLibrary> library = nil;
    if (libURL)
    {
        library = [device newLibraryWithURL:libURL error:&error];
    }
    if (!library)
    {
        // Try loading from shaders/ directory next to executable
        NSString* exePath = [[NSBundle mainBundle] executablePath];
        NSString* exeDir = [exePath stringByDeletingLastPathComponent];
        NSString* shaderPath = [exeDir stringByAppendingPathComponent:@"shaders/grid.metallib"];
        NSURL* shaderURL = [NSURL fileURLWithPath:shaderPath];
        library = [device newLibraryWithURL:shaderURL error:&error];
    }
    if (!library)
    {
        SPECTRE_LOG_ERROR(LogCategory::Renderer, "Failed to load shader library: %s",
            error ? [[error localizedDescription] UTF8String] : "unknown");
        return false;
    }

    // Create background pipeline
    {
        id<MTLFunction> vertFunc = [library newFunctionWithName:@"bg_vertex"];
        id<MTLFunction> fragFunc = [library newFunctionWithName:@"bg_fragment"];
        if (!vertFunc || !fragFunc)
        {
            SPECTRE_LOG_ERROR(LogCategory::Renderer, "Failed to find bg shader functions");
            return false;
        }

        MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction = vertFunc;
        desc.fragmentFunction = fragFunc;
        desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        // BG pass: no blending (opaque)
        desc.colorAttachments[0].blendingEnabled = NO;

        id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:desc error:&error];
        if (!pipeline)
        {
            SPECTRE_LOG_ERROR(LogCategory::Renderer, "Failed to create bg pipeline: %s",
                [[error localizedDescription] UTF8String]);
            return false;
        }
        bg_pipeline_ = (__bridge_retained void*)pipeline;
    }

    // Create foreground pipeline
    {
        id<MTLFunction> vertFunc = [library newFunctionWithName:@"fg_vertex"];
        id<MTLFunction> fragFunc = [library newFunctionWithName:@"fg_fragment"];
        if (!vertFunc || !fragFunc)
        {
            SPECTRE_LOG_ERROR(LogCategory::Renderer, "Failed to find fg shader functions");
            return false;
        }

        MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction = vertFunc;
        desc.fragmentFunction = fragFunc;
        desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        // FG pass: alpha blending
        desc.colorAttachments[0].blendingEnabled = YES;
        desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        desc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        desc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;

        id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:desc error:&error];
        if (!pipeline)
        {
            SPECTRE_LOG_ERROR(LogCategory::Renderer, "Failed to create fg pipeline: %s",
                [[error localizedDescription] UTF8String]);
            return false;
        }
        fg_pipeline_ = (__bridge_retained void*)pipeline;
    }

    // Create atlas texture (RGBA so color glyphs can bypass fg tinting)
    {
        MTLTextureDescriptor* texDesc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                         width:ATLAS_SIZE
                                        height:ATLAS_SIZE
                                     mipmapped:NO];
        texDesc.usage = MTLTextureUsageShaderRead;
        texDesc.storageMode = MTLStorageModeShared;

        id<MTLTexture> texture = [device newTextureWithDescriptor:texDesc];
        atlas_texture_ = (__bridge_retained void*)texture;
    }

    // Create atlas sampler
    {
        MTLSamplerDescriptor* sampDesc = [[MTLSamplerDescriptor alloc] init];
        sampDesc.minFilter = MTLSamplerMinMagFilterLinear;
        sampDesc.magFilter = MTLSamplerMinMagFilterLinear;
        sampDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
        sampDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;

        id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:sampDesc];
        atlas_sampler_ = (__bridge_retained void*)sampler;
    }

    // Create grid buffer (start with 80x24)
    {
        size_t initial_size = 80 * 24 * sizeof(GpuCell);
        id<MTLBuffer> buffer = [device newBufferWithLength:initial_size
                                                   options:MTLResourceStorageModeShared];
        grid_buffer_ = (__bridge_retained void*)buffer;
    }

    // Create frame semaphore for double buffering
    // Use count of 1 since we have a single shared grid buffer —
    // must wait for GPU to finish reading before CPU modifies it
    frame_semaphore_ = (__bridge_retained void*)dispatch_semaphore_create(1);

    SPECTRE_LOG_INFO(LogCategory::Renderer, "Metal renderer initialized (%s)", [[device name] UTF8String]);
    return true;
}

void MetalRenderer::shutdown()
{
    // Wait for the in-flight frame to complete
    dispatch_semaphore_t sema = (__bridge dispatch_semaphore_t)frame_semaphore_;
    if (sema)
    {
        dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
        dispatch_semaphore_signal(sema);
    }

    // Release Metal objects by transferring ownership back to ARC
    auto release = [](void*& p) {
        if (p)
        {
            (void)(__bridge_transfer id)p;
            p = nullptr;
        }
    };
    release(atlas_sampler_);
    release(atlas_texture_);
    release(grid_buffer_);
    release(capture_buffer_);
    release(fg_pipeline_);
    release(bg_pipeline_);
    release(command_queue_);
    release(layer_);
    release(device_);
}

void MetalRenderer::set_grid_size(int cols, int rows)
{
    state_.set_grid_size(cols, rows, padding_);

    size_t required = state_.buffer_size_bytes();

    id<MTLDevice> device = (__bridge id<MTLDevice>)device_;
    id<MTLBuffer> existing = (__bridge id<MTLBuffer>)grid_buffer_;

    if (!existing || [existing length] < required)
    {
        if (grid_buffer_)
        {
            (void)(__bridge_transfer id)grid_buffer_;
            grid_buffer_ = nullptr;
        }
        id<MTLBuffer> newBuf = [device newBufferWithLength:required
                                                   options:MTLResourceStorageModeShared];
        grid_buffer_ = (__bridge_retained void*)newBuf;
    }

    upload_dirty_state();
}

void MetalRenderer::update_cells(std::span<const CellUpdate> updates)
{
    state_.update_cells(updates);
    upload_dirty_state();
}

void MetalRenderer::set_overlay_cells(std::span<const CellUpdate> updates)
{
    state_.set_overlay_cells(updates);
    upload_dirty_state();
}

void MetalRenderer::set_atlas_texture(const uint8_t* data, int w, int h)
{
    id<MTLTexture> tex = (__bridge id<MTLTexture>)atlas_texture_;
    MTLRegion region = MTLRegionMake2D(0, 0, w, h);
    [tex replaceRegion:region mipmapLevel:0 withBytes:data bytesPerRow:w * 4];
}

void MetalRenderer::update_atlas_region(int x, int y, int w, int h, const uint8_t* data)
{
    id<MTLTexture> tex = (__bridge id<MTLTexture>)atlas_texture_;
    MTLRegion region = MTLRegionMake2D(x, y, w, h);
    [tex replaceRegion:region mipmapLevel:0 withBytes:data bytesPerRow:w * 4];
}

void MetalRenderer::set_cursor(int col, int row, const CursorStyle& style)
{
    state_.set_cursor(col, row, style);
}

void MetalRenderer::resize(int pixel_w, int pixel_h)
{
    pixel_w_ = pixel_w;
    pixel_h_ = pixel_h;
    CAMetalLayer* layer = (__bridge CAMetalLayer*)layer_;
    layer.drawableSize = CGSizeMake(pixel_w, pixel_h);
}

std::pair<int, int> MetalRenderer::cell_size_pixels() const
{
    return { cell_w_, cell_h_ };
}

void MetalRenderer::set_cell_size(int w, int h)
{
    cell_w_ = w;
    cell_h_ = h;
    state_.set_cell_size(w, h);
}

void MetalRenderer::set_ascender(int a)
{
    ascender_ = a;
    state_.set_ascender(a);
}

void MetalRenderer::request_frame_capture()
{
    capture_requested_ = true;
}

std::optional<CapturedFrame> MetalRenderer::take_captured_frame()
{
    auto frame = std::move(captured_frame_);
    captured_frame_.reset();
    return frame;
}

bool MetalRenderer::begin_frame()
{
    dispatch_semaphore_t sema = (__bridge dispatch_semaphore_t)frame_semaphore_;
    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

    state_.restore_cursor();
    upload_dirty_state();

    CAMetalLayer* layer = (__bridge CAMetalLayer*)layer_;
    id<CAMetalDrawable> drawable = [layer nextDrawable];
    if (!drawable)
    {
        dispatch_semaphore_signal(sema);
        return false;
    }
    current_drawable_ = (__bridge_retained void*)drawable;

    return true;
}

void MetalRenderer::end_frame()
{
    state_.apply_cursor();
    upload_dirty_state();

    id<CAMetalDrawable> drawable = (__bridge_transfer id<CAMetalDrawable>)current_drawable_;
    current_drawable_ = nullptr;

    id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)command_queue_;
    id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];

    // Create render pass descriptor
    MTLRenderPassDescriptor* rpDesc = [MTLRenderPassDescriptor renderPassDescriptor];
    rpDesc.colorAttachments[0].texture = drawable.texture;
    rpDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
    rpDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
    rpDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.1, 0.1, 0.1, 1.0);

    id<MTLRenderCommandEncoder> encoder = [cmdBuf renderCommandEncoderWithDescriptor:rpDesc];

    int total_cells = state_.fg_instances();
    int bg_instances = state_.bg_instances();

    if (bg_instances > 0)
    {
        // Push constants
        struct
        {
            float screen_w, screen_h, cell_w, cell_h;
        } push_data = {
            (float)pixel_w_, (float)pixel_h_,
            (float)cell_w_, (float)cell_h_
        };

        id<MTLBuffer> gridBuf = (__bridge id<MTLBuffer>)grid_buffer_;
        id<MTLTexture> atlasTex = (__bridge id<MTLTexture>)atlas_texture_;
        id<MTLSamplerState> sampler = (__bridge id<MTLSamplerState>)atlas_sampler_;

        // Background pass
        [encoder setRenderPipelineState:(__bridge id<MTLRenderPipelineState>)bg_pipeline_];
        [encoder setVertexBuffer:gridBuf offset:0 atIndex:0];
        [encoder setVertexBytes:&push_data length:sizeof(push_data) atIndex:1];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                    vertexStart:0
                    vertexCount:6
                  instanceCount:bg_instances];

        // Foreground pass
        [encoder setRenderPipelineState:(__bridge id<MTLRenderPipelineState>)fg_pipeline_];
        [encoder setVertexBuffer:gridBuf offset:0 atIndex:0];
        [encoder setVertexBytes:&push_data length:sizeof(push_data) atIndex:1];
        [encoder setFragmentTexture:atlasTex atIndex:0];
        [encoder setFragmentSamplerState:sampler atIndex:0];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                    vertexStart:0
                    vertexCount:6
                  instanceCount:total_cells];
    }

    [encoder endEncoding];

    if (capture_requested_)
    {
        const size_t width = drawable.texture.width;
        const size_t height = drawable.texture.height;
        if (ensure_capture_buffer(width, height))
        {
            id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
            [blit copyFromTexture:drawable.texture
                             sourceSlice:0
                             sourceLevel:0
                            sourceOrigin:MTLOriginMake(0, 0, 0)
                              sourceSize:MTLSizeMake(width, height, 1)
                                toBuffer:(__bridge id<MTLBuffer>)capture_buffer_
                       destinationOffset:0
                  destinationBytesPerRow:capture_bytes_per_row_
                destinationBytesPerImage:capture_bytes_per_row_ * height];
            [blit endEncoding];
        }
        else
        {
            capture_requested_ = false;
        }
    }

    [cmdBuf presentDrawable:drawable];

    // Signal semaphore when GPU is done
    dispatch_semaphore_t sema = (__bridge dispatch_semaphore_t)frame_semaphore_;
    [cmdBuf addCompletedHandler:^(id<MTLCommandBuffer>) {
        dispatch_semaphore_signal(sema);
    }];

    [cmdBuf commit];

    if (capture_requested_)
    {
        [cmdBuf waitUntilCompleted];

        CapturedFrame frame;
        frame.width = pixel_w_;
        frame.height = pixel_h_;
        frame.rgba.resize(static_cast<size_t>(frame.width) * frame.height * 4);

        const auto* src = static_cast<const uint8_t*>((__bridge id<MTLBuffer>)capture_buffer_ ? [(__bridge id<MTLBuffer>)capture_buffer_ contents] : nullptr);
        if (src)
        {
            for (int y = 0; y < frame.height; ++y)
            {
                const size_t src_row = static_cast<size_t>(y) * capture_bytes_per_row_;
                const size_t dst_row = static_cast<size_t>(y) * frame.width * 4;
                for (int x = 0; x < frame.width; ++x)
                {
                    const size_t src_index = src_row + static_cast<size_t>(x * 4);
                    const size_t dst_index = dst_row + static_cast<size_t>(x * 4);
                    frame.rgba[dst_index + 0] = src[src_index + 2];
                    frame.rgba[dst_index + 1] = src[src_index + 1];
                    frame.rgba[dst_index + 2] = src[src_index + 0];
                    frame.rgba[dst_index + 3] = src[src_index + 3];
                }
            }
            captured_frame_ = std::move(frame);
        }
        capture_requested_ = false;
    }
}

} // namespace spectre
