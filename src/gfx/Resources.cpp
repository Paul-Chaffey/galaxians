#include "gfx/Resources.h"

#include "gfx/VkCheck.h"
#include "gfx/VulkanContext.h"

#include <SDL3/SDL_surface.h>

#include <cstring>

namespace gfx {

MappedBuffer createMappedBuffer(const VulkanContext& ctx, VkDeviceSize size, VkBufferUsageFlags usage)
{
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                      VMA_ALLOCATION_CREATE_MAPPED_BIT;

    MappedBuffer result;
    VmaAllocationInfo info{};
    VK_CHECK(vmaCreateBuffer(ctx.allocator(), &bufferInfo, &allocInfo,
                             &result.buffer, &result.allocation, &info));
    result.mapped = info.pMappedData;
    return result;
}

void destroyBuffer(const VulkanContext& ctx, MappedBuffer& buffer)
{
    if (buffer.buffer)
        vmaDestroyBuffer(ctx.allocator(), buffer.buffer, buffer.allocation);
    buffer = {};
}

Image createImage2D(const VulkanContext& ctx, VkExtent2D extent, VkFormat format, VkImageUsageFlags usage)
{
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {extent.width, extent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = usage;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    Image result;
    result.extent = extent;
    result.format = format;
    VK_CHECK(vmaCreateImage(ctx.allocator(), &imageInfo, &allocInfo,
                            &result.image, &result.allocation, nullptr));

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = result.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VK_CHECK(vkCreateImageView(ctx.device(), &viewInfo, nullptr, &result.view));
    return result;
}

void destroyImage(const VulkanContext& ctx, Image& image)
{
    if (image.view)
        vkDestroyImageView(ctx.device(), image.view, nullptr);
    if (image.image)
        vmaDestroyImage(ctx.allocator(), image.image, image.allocation);
    image = {};
}

Image loadTexture(const VulkanContext& ctx, const std::string& path)
{
    SDL_Surface* loaded = SDL_LoadPNG(path.c_str());
    if (!loaded)
        throw std::runtime_error("Failed to load " + path + ": " + SDL_GetError());
    SDL_Surface* surface = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (!surface)
        throw std::runtime_error("Failed to convert " + path + ": " + SDL_GetError());

    const VkExtent2D extent{static_cast<uint32_t>(surface->w), static_cast<uint32_t>(surface->h)};
    const size_t rowBytes = size_t(extent.width) * 4;

    MappedBuffer staging = createMappedBuffer(ctx, rowBytes * extent.height, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    for (uint32_t y = 0; y < extent.height; ++y)
        std::memcpy(static_cast<uint8_t*>(staging.mapped) + y * rowBytes,
                    static_cast<const uint8_t*>(surface->pixels) + size_t(y) * surface->pitch, rowBytes);
    SDL_DestroySurface(surface);
    VK_CHECK(vmaFlushAllocation(ctx.allocator(), staging.allocation, 0, VK_WHOLE_SIZE));

    Image image = createImage2D(ctx, extent, VK_FORMAT_R8G8B8A8_SRGB,
                                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    ctx.immediateSubmit([&](VkCommandBuffer cmd) {
        transitionImage(cmd, image.image,
                        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {extent.width, extent.height, 1};
        vkCmdCopyBufferToImage(cmd, staging.buffer, image.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        transitionImage(cmd, image.image,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    });

    destroyBuffer(ctx, staging);
    return image;
}

void transitionImage(VkCommandBuffer cmd, VkImage image,
                     VkImageLayout oldLayout, VkImageLayout newLayout,
                     VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
                     VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess)
{
    VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.srcStageMask = srcStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStage;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmd, &dep);
}

} // namespace gfx
